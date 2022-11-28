#pragma once
#include "Thread.h"
#include "noncopyable.h"

#include <condition_variable>
#include <functional>
#include <mutex>

class EventLoop;

class EventLoopThread : noncopyable {
public:
  using ThreadInitCallback = std::function<void(EventLoop *)>;

  EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                  const std::string &name = std::string());
  ~EventLoopThread();

  EventLoop *startLoop();

private:
  void threadFunc(); // 在新创建的线程中执行的方法，创建一个loop，并且执行绑定的 callback_

  EventLoop *loop_;
  bool exiting_;
  Thread thread_;
  std::mutex mutex_;
  std::condition_variable cond_;
  ThreadInitCallback callback_;
};