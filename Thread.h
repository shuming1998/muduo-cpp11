#pragma once
#include "noncopyable.h"

#include <functional>
#include <thread>
#include <memory>
#include <unistd.h>
#include <string>
#include <atomic>

/*
 * 该类主要关注一个线程
 * EventLoopThread 类绑定了一个 EventLoop类 和 Thread类，loop 是在一个指定的 thread 中创建的
 */

class Thread : noncopyable{
public:
  using ThreadFunc = std::function<void()>;
  explicit Thread(ThreadFunc, const std::string &name = std::string());
  ~Thread();

  void start();
  void join();

  bool started() const { return started_; }
  pid_t tid() const { return tid_; }
  const std::string &name() const { return name_; }

  static int numCreated() { return numCreated_; }

private:
  void setDefaultName();

  bool started_;
  bool joined_;
  std::shared_ptr<std::thread> thread_;  // 用 std::thread 定义线程直接启动，应该用智能指针 + 绑定
  pid_t tid_;
  ThreadFunc func_;
  std::string name_;
  static std::atomic_int numCreated_;
};