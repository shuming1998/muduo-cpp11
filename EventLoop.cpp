#include "EventLoop.h"
#include "Channel.h"
#include "Logger.h"
#include "Poller.h"

#include <errno.h>
#include <fcntl.h>
#include <memory>
#include <sys/eventfd.h>
#include <unistd.h>

// 防止一个线程创建多个 EventLoop，该指针指向创建的一个
// EventLoop，后续该指针不为空，无法再重复创建EventLoop
// ____thread 是一个 thread_local
// 机制，每个线程中都有一个该变量的副本，线程之间不共享该变量的值
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义 Poller I/O 复用接口的默认超时时间
const int kPollTimeMs = 10000;

// 创建 wakeupfd，用来唤醒(notify) subReactor 处理新连接的 channel
int createEventFd() {
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0) {
    LOG_FATAL("[%s:%s:%d]\neventfd error: %d\n", __FILE__, __FUNCTION__,
              __LINE__, errno);
  }
  return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventFd()) // 注册一个 fd,但还没设置该 fd 感兴趣的事件
    , wakeupChannel_(new Channel(this, wakeupFd_)) { // 唤醒 subReactor
  LOG_DEBUG("EventLoop created %p in thread %d\n", __FILE__, __FUNCTION__,
            __LINE__, this, threadId_);
  if (t_loopInThisThread) {
    // 当前线程已经创建了一个 EventLoop 对象
    LOG_FATAL("[%s:%s:%d]\nAnother EventLoop %p exists in this thread %d\n",
              __FILE__, __FUNCTION__, __LINE__, t_loopInThisThread, threadId_);
  } else {
    t_loopInThisThread = this;
  }

  // 设置 wakeupFd_ 的感兴趣事件类型，以及发生事件后的回调操作(主要是为了唤醒相应的 EventLoop)
  wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
  // 每一个 EventLoop 都将监听 wakeupChannel 的 EPOLLIN 读事件，需要唤醒某个线程时，往该线程写数据就会触发读事件
  // 的回调函数 EventLoop::handleRead，通过 read 该 wakeupfd_ 上的数据唤醒该线程
  wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
  wakeupChannel_->disableAll(); // 设置 channel 对所有事件都不感兴趣
  wakeupChannel_->remove();     // 删除 channel
  ::close(wakeupFd_);           // 然后关闭 fd，线程阻塞
  t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop() {
  looping_ = true;
  quit_ = false;

  LOG_INFO("[%s:%s:%d]\nEventLoop %p start looping\n", __FILE__, __FUNCTION__,
           __LINE__, this);
  while (!quit_) {
    activeChannels_.clear();
    // 监听两类 fd：与客户端通信用的连接 fd 和 mainLoop 与 subLoop
    // 之间通信(唤醒subLoop)的 wakeupfd_ loop() 方法通过调用 poller 封装的 I/O
    // 复用接口，获取 activeChannels_ 中所有的 channel
    pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
    for (Channel *channel : activeChannels_) {
      // Poller 监听哪些 channel 发生了事件，并将其上报给 EventLoop,通知 channel
      // 处理 events 事件 然后 channel 会通过 handleEvent 在
      // handleEventWithGuard 方法中，根据事件类型执行相应的处理逻辑
      channel->handleEvent(pollReturnTime_);
    }
    // 执行当前 EventLoop 事件循环需要处理的事件回调操作
    // I/O 线程(mainLoop) 主要 accept 新用户连接，返回一个与客户端之间的连接
    // fd，打包于 channel 中 并通过轮询的方式 wakeup 一个 subLoop，将 channel
    // 分发给它 mainLoop 事先注册一个回调cb(需要subLoop执行)，wakeup subLoop
    // 后，执行之前 mainLoop 注册 cb
    doPendingFunctors();
  }
  LOG_INFO("[%s:%s:%d]\nEventLoop %p stop looping!\n", __FILE__, __FUNCTION__,
           __LINE__, this);
  looping_ = false;
}

// 退出事件循环，两种可能：
// 1.loop在自己的线程中调用 quit()
// 2.在非 loop 线程中调用 quit()，比如在一个 subLoop 中调用了 mainLoop 的quit()
// (在subLoop中结束mainLoop)
void EventLoop::quit() {
  quit_ = true;
  // 如果是第二种情况，必须先唤醒那个线程，使其能够跳出 while
  // 循环，然后结束那个线程的 loop
  if (!isInLoopThread()) {
    wakeup();
  }
}

// 在当前 loop 中执行 cb
void EventLoop::runInLoop(Functor cb) {
  if (isInLoopThread()) {
    cb();
  } else {
    // 在非当前 loop 线程中执行 cb，就需要唤醒 loop 所在的线程执行 cb
    queueInLoop(cb);
  }
}

// 把 cb 放在队列中，唤醒 loop 所在线程，执行 cb
void EventLoop::queueInLoop(Functor cb) {
  // 出了代码块锁就消失了
  {
    std::unique_lock<std::mutex> lock(mutex_);
    pendingFunctors_.emplace_back(cb);
  }
  // 唤醒相应的、需要执行上面回调操作的 loop 线程
  // 1. 当前代码所在线程不是要执行回调的 loop 线程，需要唤醒那个 loop 线程
  // 2. 当前 loop 正在执行回调，此时又写了新的回调(此时 callingPendingFunctors_
  // 在 doPendingFunctors 中还未设置为 false)，如果不考虑这种情况，上一轮
  // doPendingFunctors 结束后 loop 将又阻塞于 while 循环中的 poller_->poll()
  // 方法，就会没有机会执行新写入的回调
  if (!isInLoopThread() || callingPendingFunctors_) {
    // 唤醒 loop 所在线程
    wakeup();
  }
}

// 唤醒线程时被公有方法调用
void EventLoop::handleRead() {
  uint64_t one = 1;
  ssize_t n = read(wakeupFd_, &one, sizeof one);
  if (n != sizeof one) {
    LOG_ERROR(
        "[%s:%s:%d]\nEventLoop::handleRead() reads %ld bytes instead of 8\n",
        __FILE__, __FUNCTION__, __LINE__, n);
  }
}

// mainReactor 唤醒 subReactor(用来唤醒 loop 所在的线程)
// 事先通过构造函数中的 wakeupChannel_->enableReading() 注册了读事件，向
// wakeupFd_ 写一个数据 wakeupChannel 就会发生读事件，当前 loop 就会从
// loop()方法 中 poller 的 poll() 方法的阻塞中唤醒
void EventLoop::wakeup() {
  uint64_t one = 1;
  ssize_t n = write(wakeupFd_, &one, sizeof one);
  if (n != sizeof one) {
    LOG_ERROR(
        "[%s:%s:%d]\nEventLoop::wakeup() writes %lu bytes instead of 8!\n",
        __FILE__, __FUNCTION__, __LINE__, n);
  }
}

// channel 的方法 ==> EventLoop 的这两个方法 ==> poller 上的update/removeChannel
// 方法
void EventLoop::updateChannel(Channel *channel) {
  poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel) {
  poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel) {
  return poller_->hasChannel(channel);
}

// 在 loop() 中调用，执行回调，回调函数都放在 vector<Functor> 中
// 子线程从存放要执行的 cb 的容器中取 cb 时是加锁的，在不断从容器取 cb 并执行 cb
// 的过程中，主线程是有可能往容器中添加新 cb
// 的，而此时容器被子线程占有，就会导致主线程阻塞
// 为了避免这种情况，申请了一个局部 cb 容器，将要执行的 cb
// 拷贝到里面，然后子线程在局部 cb 容器中 取 cb、执行 cb，避免了与主线程之间关于
// cb 容器争用的冲突，实现了并发操作
void EventLoop::doPendingFunctors() {
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;
  // 出了代码块锁就消失了
  {
    std::unique_lock<std::mutex> lock(mutex_);
    functors.swap(pendingFunctors_);
  }
  for (const Functor &functor : functors) {
    functor(); // 执行当前 loop 需要执行的回调操作
  }
  callingPendingFunctors_ = false;
}