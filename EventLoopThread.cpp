#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
                                 const std::string &name)
    : loop_(nullptr)
    , exiting_(false)
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name) // Thread 的 start 方法中调用的函数
    , mutex_()
    , cond_()
    , callback_(cb) {}

EventLoopThread::~EventLoopThread() {
  exiting_ = true;
  if (loop_ != nullptr) {
    loop_->quit();
    thread_.join();
  }
}

// 获取一个新线程，返回该新线程中单独运行的一个 loop 对象指针
EventLoop *EventLoopThread::startLoop() {
  // 每次 start 都会启动底层的一个新线程，start() 方法中子线程执行的线程函数是在
  // EventLoopThread 构造函数中绑定的 EventLoopThread::threadFunc()
  thread_.start();
  // 为了等待并判断上面新创建的子线程是否初始化完成，需要用互斥锁等待子线程中使用条件变量通知
  EventLoop *loop = nullptr;
  {
    // mutex_被start()中创建新的子线程执行 threadFunc
    // 时锁住，当前线程阻塞在这里，等待
    std::unique_lock<std::mutex> lock(mutex_);
    while (loop_ == nullptr) {
      cond_.wait(lock); // 一旦得到子线程 notify 唤醒，该函数取消阻塞并获取锁
    }
    loop = loop_;
  }
  return loop;
}

// 这个方法是在单独的新线程中运行的,绑定到了 Thread 类中的 func_ 对象上，在 Thread::start() 方法中新创建的子线程中执行
void EventLoopThread::threadFunc() {
  // 创建一个独立的 EventLoop， 在这里实现了 one loop per thread
  EventLoop loop;

  if (callback_) {
    callback_(&loop);
  }

  {
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = &loop;
    cond_.notify_one(); // 唤醒阻塞的线程之一
  }

  // 开启事件循环
  loop.loop(); // EventLoop loop ==> Poller.poll

  // 关闭事件循环
  std::unique_lock<std::mutex> lock(mutex_);
  loop_ = nullptr;
}