#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"

#include <memory>

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop)
    , name_(nameArg)
    , started_(false)
    , numThreads_(0)
    , next_(0) {}

// 由于在 EventLoopThread 中绑定的新线程执行的函数中，创建的 EventLoop
// 是个栈对象，所以当事件循环的 poller
// 结束工作后，该对象会自动析构，所以不需要手动delete
EventLoopThreadPool::~EventLoopThreadPool() {}

void EventLoopThreadPool::start(const ThreadInitCallback &cb) {
  started_ = true;
  // 如果用户设置了多线程模式，就执行这段逻辑，不执行下一段
  for (int i = 0; i < numThreads_; ++i) {
    char buf[name_.size() + 32];
    snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);
    EventLoopThread *t = new EventLoopThread(cb, buf);
    threads_.push_back(std::unique_ptr<EventLoopThread>(t));
    loops_.push_back(t->startLoop()); // 创建线程，绑定一个新的EventLoop，并返回它的地址
  }

  // 整个服务端只有一个线程，运行着 baseloop_
  if (numThreads_ == 0 && cb) {
    cb(baseLoop_);
  }
}

// 如果工作在多线程中，baseLoop 会默认一轮询的方式分配 channel 给 subLoop
EventLoop *EventLoopThreadPool::getNextLoop() {
  // 如果单线程，就是用户线程
  EventLoop *loop = baseLoop_;
  // 如果多线程，轮询获取下一个处理事件的 loop
  if (!loops_.empty()) {
    loop = loops_[next_];
    ++next_;
    if (next_ >= loops_.size()) {
      next_ = 0;
    }
  }
  return loop;
}

std::vector<EventLoop *> EventLoopThreadPool::getAllLoops() {
  if (loops_.empty()) {
    return std::vector<EventLoop *>(1, baseLoop_);
  } else {
    return loops_;
  }
}