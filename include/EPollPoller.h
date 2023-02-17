#pragma once

/*
 * epoll 的使用
 * epoll_create   (构造函数)
 * epoll_ctl      add/mod/del (update/removeChannel)
 * epoll_wait     (poll)
 *
 *            EventLoop
 *   channelList     poller
 *                ChannelMap<fd, channel *>    epollfd
 */

#include "Poller.h"
#include "Timestamp.h"

#include <sys/epoll.h>
#include <vector>

class Channel;

class EPollPoller : public Poller {
public:
  EPollPoller(EventLoop *loop);
  ~EPollPoller() override;

  // 重写 Poller 的成员方法
  // 对应于 epoll_ctl
  void updateChannel(Channel *channel) override;
  void removeChannel(Channel *channel) override;
  // 对应于 epoll_wait，用可扩容的数组存储 epoll_event
  Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;

private:
  // 填写活跃的连接
  void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
  // 更新 channel 通道
  void update(int operation, Channel *channel);

  using EventList = std::vector<epoll_event>;

  static const int kInitEventListSize = 16; // epoll_event 数组的初始大小
  int epollfd_;                             // epoll 对象的文件描述符，通过 epoll_creat 创建
  EventList events_;                        // epoll_wait 的第二个参数，代表发生事件的 fd
};