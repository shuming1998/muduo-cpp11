#pragma once
/*
 * 对外的服务器编程使用的类
 * 为了方便用户使用 muduo 编写服务器程序，将相关头文件都包含在这个文件中
 * TcpServer 对象的构造函数的初始化列表中会构造一个 Acceptor 对象，而 Acceptor 对象在构造函数中
 * 会创建 socket 将该 socket 封装成一个 channel 后，添加到 poller 中：
 * Acceptor::listen() 是 TcpServer 对象通过 start() 方法调用的，在 Acceptor::listen()
 * 中会调用 listen 和 enableReading[通过注册一个读事件，把listenfd 添加到 poller 中]。
 */

#include "noncopyable.h"


#include "Buffer.h"
#include "Acceptor.h"
#include "Callbacks.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "TcpConnection.h"
#include "EventLoopThreadPool.h"

#include <string>
#include <memory>
#include <atomic>
#include <functional>
#include <unordered_map>

class TcpServer : noncopyable {
public:
  using ThreadInitCallback = std::function<void(EventLoop *)>;

  enum Option {
    kNoReusePort,
    kReusePort
  };

  TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg, Option option = kNoReusePort);
  ~TcpServer();

  void setThreadInitCallback(const ThreadInitCallback &cb) { threadInitCallback_ = std::move(cb); }
  void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = std::move(cb); }
  void setMessageCallback(const MessageCallback &cb) { messageCallback_ = std::move(cb); }
  void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = std::move(cb); }

  // 设置底层 loop 个数
  void setThreadNum(int numThreads);

  // 开启服务器监听(开启 Acceptor 的 listen)
  void start();

private:
  // 根据轮询算法，选择并唤醒一个 subLoop，将当前的 connfd 封装成 channel 分发给 subLoop，并设置回调
  // 该函数运行在主线程中，如果想执行子线程 loop 的回调，必须调用 QueueInLoop，通过 wakeupFd_ 唤醒相应的子线程
  void newConnection(int sockfd, const InetAddress &peerAddr);

  void removeConnection(const TcpConnectionPtr &conn);
  void removeConnectionInLoop(const TcpConnectionPtr &conn);


  using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
  EventLoop *loop_;                                 // 用户定义的 baseLoop

  const std::string ipPort_;
  const std::string name_;

  std::unique_ptr<Acceptor> acceptor_;              // 运行在 mainLoop， 监听 [新客户端连接] 事件

  std::shared_ptr<EventLoopThreadPool> threadPool_; // 将打包好的连接 channel 分发给通过 EventLoopThreadPool::getNextLoop 轮询获取的子线程

  // 用户设置的回调
  ConnectionCallback connectionCallback_;           // 有新连接时的回调
  MessageCallback messageCallback_;                 // 有读写消息时的回调
  WriteCompleteCallback writeCompleteCallback_;     // 消息发送完成之后的回调

  ThreadInitCallback threadInitCallback_;           // loop 线程初始化时的回调
  std::atomic_int started_;

  int nextConnId_;                                  // 在主线程中处理，不涉及多线程访问问题，所以不需要定义为原子整型
  ConnectionMap connections_;                       // 保存所有的连接
};