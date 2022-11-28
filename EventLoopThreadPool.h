#pragma once
#include "noncopyable.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class EventLoop;
class EventLoopThread;

/*
 * 事件循环线程池
 *
*/

class EventLoopThreadPool : noncopyable {
public:
  using ThreadInitCallback = std::function<void(EventLoop *)>;

  EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
  ~EventLoopThreadPool();

  // 设置底层线程数量，TcpServer 的 setThreadNum 方法底层调用的就是这个方法
  // 一个创建的 thread 对应一个 loop，即 one loop per thread
  void setThreadNum(int numThreads) { numThreads_ = numThreads; }

  void start(const ThreadInitCallback &cb = ThreadInitCallback());

  // 如果工作在多线程中，baseLoop 会通过轮询的方式获取 subLoop，如果用户没有 setThreadNum，则返回的就是主线程的 mainLoop
  EventLoop *getNextLoop();

  std::vector<EventLoop *> getAllLoops();

  bool started() const { return started_; }
  const std::string name() const { return name_; }

private:
  EventLoop *baseLoop_; // 如果使用网络库时不设置多线程，就只有这一个线程，是用户主线程
  std::string name_;
  bool started_;
  int numThreads_;
  int next_;
  std::vector<std::unique_ptr<EventLoopThread>> threads_; // 所有事件的线程
  std::vector<EventLoop *> loops_; // 所有事件线程对应的 loop 指针，通过调用 EventLoopThread 的 startLoop 可以获得一个指针
};