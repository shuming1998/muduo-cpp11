#include <mymuduo/TcpServer.h>
#include <mymuduo/Logger.h>

#include <string>
#include <functional>

class EchoServer {
public:
  EchoServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
      : server_(loop, addr, name)
      , loop_(loop) {
    server_.setConnectionCallback(std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2,
        std::placeholders::_3));
    server_.setThreadNum(7);
}

  void start() { server_.start(); }

private:
  void onConnection(const TcpConnectionPtr &conn) {
    if (conn->connected()) {
      LOG_INFO("Connection success : %s", conn->peerAddress().toIpPort().c_str());
    } else {
      LOG_INFO("Connection failed : %s", conn->peerAddress().toIpPort().c_str());
    }
  }

  void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time) {
    std::string msg = buf->retrieveAllAsString();
    conn->send(msg);
    conn->shutdown(); // 关闭写端
  }

  EventLoop *loop_;
  TcpServer server_;
};

int main(int argc, char *argv[]) {
  EventLoop loop;
  InetAddress addr(8000);
  EchoServer server(&loop, addr, "EchoServer-01");
  // 开启子线程 loop，注册 wakeupfd
  server.start(); // listen loopthread listenfd => acceptChannel => mainLoop
  loop.loop(); // 启动 mainLoop Poller
  return 0;
}