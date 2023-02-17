#pragma once
#include "noncopyable.h"

/*
 * 封装 socket 及相关的系统调用方法，供 Acceptor 使用
 */

class InetAddress;

class Socket : noncopyable {
public:
  explicit Socket(int sockfd) : sockfd_(sockfd) {}

  ~Socket();

  int fd() const { return sockfd_; }
  void bindAddress(const InetAddress &localaddr);
  void listen();
  int accept(InetAddress *peeraddr);

  void shutdownWrite();

  void setTcpNoDelay(bool on);
  void setReuseAddr(bool on);
  void setReusePort(bool on);
  void setKeepAlive(bool on);
  static int getSocketError(int sockfd);

private:
  const int sockfd_;
};