#pragma once
#include "Channel.h"
#include "Socket.h"
#include "noncopyable.h"

#include <functional>

/*
 * Acceptor 主要封装了 listenfd 相关的操作(socket、bind、listen)，listen 成功后打包成 acceptChannel 注册在 mainLoop 中监听新连接
 * 每当有新连接时，该类中的新连接回调函数 newConnectCallback_ 会将新连接的 fd 打包
 * 成 channel 然后通过 getNextLoop 轮询唤醒一个 subLoop，再将 channel 分发给它
 */

class EventLoop;
class InetAddress;

class Acceptor : noncopyable {
public:
  using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;

  Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
  ~Acceptor();

  // 当有新连接时，用来让 TcpServer 设置要执行的回调函数，在 TcpServer 对象的构造函数中调用
  void setNewConnectionCallback(const NewConnectionCallback &cb) {
    newConnectionCallback_ = cb;
  }

  bool listenning() const { return listenning_; }
  void listen();

private:
  void handleRead();

  // Acceptor 就运行在用户定义的 baseLoop 中，专用于监听 I/O
  EventLoop *loop_;
  Socket acceptSocket_;
  Channel acceptChannel_;
  NewConnectionCallback newConnectionCallback_; // 有新连接时，执行 TcpServer 提供的回调函数
  bool listenning_;
};
