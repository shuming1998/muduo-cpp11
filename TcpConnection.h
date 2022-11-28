#pragma once
#include "Buffer.h"
#include "Callbacks.h"
#include "InetAddress.h"
#include "noncopyable.h"

#include <atomic>
#include <memory>
#include <string>

class Socket;
class Channel;
class EventLoop;

/*
 * 一个连接成功的客户端对应一个 TcpConnection
 * TcpServer ==> Acceptor ==> 新用户连接，通过 accept 函数拿到 connfd
 * ==> TcpConnection 设置回调 ==> Channel ==> Poller ==> Channel 回调
 */

class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection> {
public:
  TcpConnection(EventLoop *loop, const std::string &nameArg, int sockfd,
                const InetAddress &localAddr, const InetAddress &peerAddr);
  ~TcpConnection();

  EventLoop *getLoop() const { return loop_; }
  const std::string &name() const { return name_; }
  const InetAddress &localAddress() const { return localAddr_; }
  const InetAddress &peerAddress() const { return peerAddr_; }

  bool connected() const { return state_ == kConnected; }
  bool disconnected() const { return state_ == kDisconnected; }

  // 发送数据
  void send(const std::string &buf);
  // 关闭连接
  void shutdown();

  // 设置回调
  void setConnectionCallback(const ConnectionCallback &cb) {
    connectionCallback_ = cb;
  }
  void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }

  void setWriteCompleteCallback(const WriteCompleteCallback &cb) {
    writeCompleteCallback_ = cb;
  }

  void setHighWaterMarkCallback(const HighWaterMarkCallback &cb,
                                size_t highWaterMark) {
    highWaterMarkCallback_ = cb;
    highWaterMark_ = highWaterMark;
  }
  void setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }

  // 连接建立
  void connectEstablished();
  // 连接销毁
  void connectDestroyed();

private:
  enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
  void setState(StateE state) { state_ = state; }

  void handleRead(Timestamp receiveTime);
  void handleWrite();
  void handleClose();
  void handleError();

  void sendInLoop(const void *data, size_t len);
  void shutdownInLoop();

  EventLoop *loop_; // 这里绝对不是 mainLoop，因为 TcpConnection 都是在 subLoop 中管理的
  const std::string name_;
  std::atomic_int state_;
  bool reading_;

  // 和 Acceptor 类似， 只不过 Acceptor 在 mainLoop， 而 TcpConnection 在 subLoop
  std::unique_ptr<Socket> socket_;
  std::unique_ptr<Channel> channel_;

  const InetAddress localAddr_;
  const InetAddress peerAddr_;

  // 用户 ==> TcpServer ==> TcpConnection ==> Channel
  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  WriteCompleteCallback writeCompleteCallback_;
  HighWaterMarkCallback highWaterMarkCallback_;
  CloseCallback closeCallback_;

  size_t highWaterMark_;  // 高水位线避免发送过快
  Buffer inputBuffer_;    // 接收数据的缓冲区
  Buffer outputBuffer_;   // 发送数据的缓冲区
};