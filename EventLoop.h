#pragma once
#include "CurrentThread.h"
#include "Timestamp.h" // 类中使用的是Timestamp变量而非指针，编译需要知道这个类的大小，所以前置声明不满足要求
#include "noncopyable.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

/*
 * 对应于 Reactor 组件
 * 时间循环类，主要包含两个大模块 Channel、poller(epoll的抽象)，掌控 poller 和 channel
 */

class Channel;
class Poller;

class EventLoop : noncopyable {
public:
  using Functor = std::function<void()>;

  EventLoop();
  ~EventLoop();

  void loop(); // 开启事件循环
  void quit(); // 退出事件循环

  Timestamp pollReturnTime() const { return pollReturnTime_; }

  void runInLoop(Functor cb); // 在当前 loop 中执行 cb
  void queueInLoop(Functor cb); // 把 cb 放在队列中，唤醒 loop 所在线程的执行 cb

  void wakeup(); // mainReactor 唤醒 subReactor(用来唤醒 loop 所在的线程)

  // channel 的方法 ==> EventLoop 的这两个方法 ==> poller
  // 上的update/removeChannel 方法
  void updateChannel(Channel *channel);
  void removeChannel(Channel *channel);
  bool hasChannel(Channel *channel);

  // 判断 EventLoop 对象是否在创建它的线程中
  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
  void handleRead();        // 唤醒线程时被公有方法调用
  void doPendingFunctors(); // 执行回调，回调函数都放在 vector<Functor> 中

  using ChannelList = std::vector<Channel *>;

  std::atomic_bool looping_; // 原子操作通过CAS实现
  std::atomic_bool quit_; // 标识退出 loop 循环

  const pid_t threadId_; // 记录当前 loop 所在线程的 id

  Timestamp pollReturnTime_; // poller 返回发生事件的 channels 的时间点
  std::unique_ptr<Poller> poller_; // EventLoop 管理的 poller，监听所有 channels 上发生的事件

  // muduo 通过 eventfd 系统调用实现线程间的通信，wakeFd_ 是该系统调用创建的。mainLoop 获取一个新用户连接
  // 时，通过轮询算法选择一个subLoop(有可能阻塞)，通过 wakeupFd_ 唤醒(向这个 fd 写一个数据)选择的 subLoop
  int wakeupFd_;  // 每一个 loop 都有一个 wakeupFd_
  std::unique_ptr<Channel> wakeupChannel_;  // wakeupFd_ 封装的 channel，注册在了自己所属 loop 的 poller上

  ChannelList activeChannels_;      // 包含所有的 channel
  Channel *currentActivateChannel_; // 主要用于断言操作，可以不用

  std::atomic_bool callingPendingFunctors_; // 标识当前 loop 是否有需要执行的回调操作
  // 如果当前线程不是该回调函数对应的 loop 所属的线程，就要放在一个队列中，唤醒相应的线程之后再执行该回调函数
  std::vector<Functor> pendingFunctors_; // 存储 loop 需要执行的所有的回调操作
  std::mutex mutex_; // 互斥锁，用来保护上面 vector<Functor> 的线程安全操作
};