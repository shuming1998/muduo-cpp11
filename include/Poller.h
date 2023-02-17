#pragma once
#include "Timestamp.h"
#include "noncopyable.h"

#include <unordered_map>
#include <vector>

class Channel;
class EventLoop;

/*
 * muduo 库中多路事件分发器中的核心 I/O 复用模块
 */

class Poller : noncopyable {
public:
  using ChannelList = std::vector<Channel *>;

  Poller(EventLoop *loop);
  virtual ~Poller() = default;

  // 给所有I/O复用保留统一的接口，需要重写，此项目虽只用epoll，为了可拓展
  virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
  virtual void updateChannel(Channel *channel) = 0;
  virtual void removeChannel(Channel *channel) = 0;

  // 判断 channel 是否在当前 poller 中
  bool hasChannel(Channel *channel) const;
  // 通过该接口获得I/O复用的具体对象，epoll/poll/select，但不实现在 Poller
  // 文件中，具体原因看函数实现
  static Poller *newDefaultPoller(EventLoop *loop);

protected:
  using ChannelMap = std::unordered_map<int, Channel *>; // 保存 sockfd <---> 包含该 fd的 Channel
  ChannelMap channels_;   // poller 检测到某个 fd 上有注册的事件发生时，就通过这个 map 找到 channel，从而找到 channel 中记录的回调函数

private:
  EventLoop *ownerLoop_;  // 记录 poller 所属的事件循环，用于和 channel 通信
};