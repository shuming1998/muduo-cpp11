#include "EPollPoller.h"
#include "Channel.h"
#include "Logger.h"
#include "errno.h"
#include <string.h>
#include <unistd.h>

// channel 在 epoll 中的状态
const int kNew = -1; // channel 未添加到 poller 中，成员 index_ 初始化值为 -1
const int kAdded = 1;   // channel 已添加到 poller 中
const int kDeleted = 2; // channel 已从 poller 中删除

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) {
  if (epollfd_ < 0) {
    LOG_FATAL("[%s:%s:%d]\nepoll_create error: %d\n", __FILE__, __FUNCTION__,
              __LINE__, errno);
  }
}

EPollPoller::~EPollPoller() { ::close(epollfd_); }

// 更新 channel => epoll_ctl add/mod/del
void EPollPoller::update(int operation, Channel *channel) {
  epoll_event event;
  bzero(&event, sizeof(event)); // memset(&event, 0, sizeof(event));

  int fd = channel->fd();

  event.events = channel->events();
  event.data.fd = fd;
  event.data.ptr = channel;

  if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
    if (operation == EPOLL_CTL_DEL) {
      LOG_ERROR("[%s:%s:%d]\nepoll_ctl del error: fd = %d errno = %d\n",
                __FILE__, __FUNCTION__, __LINE__, fd, errno);
    } else if (operation == EPOLL_CTL_ADD || operation == EPOLL_CTL_MOD) {
      LOG_FATAL("[%s:%s:%d]\nepoll_ctl add/mod error: fd = %d errno = %d\n",
                __FILE__, __FUNCTION__, __LINE__, fd, errno);
    }
  }
}

// channel update/remove => EventLoop update/removeChannel => Poller
// update/removeChannel
void EPollPoller::updateChannel(Channel *channel) {
  const int index = channel->index();
  LOG_INFO("[%s:%s:%d]\nfd = %d events = %d index = %d\n", __FILE__,
           __FUNCTION__, __LINE__, channel->fd(), channel->events(), index);

  if (index == kNew || index == kDeleted) {
    if (index == kNew) {
      int fd = channel->fd();
      channels_[fd] = channel;
    }

    channel->set_index(kAdded);
    update(EPOLL_CTL_ADD, channel);
  } else {
    int fd = channel->fd();
    if (channel->isNoneEvent()) {
      update(EPOLL_CTL_DEL, channel);
      channel->set_index(kDeleted);
    } else {
      update(EPOLL_CTL_MOD, channel);
    }
  }
}

// 将 channel 从 poller 中删除
void EPollPoller::removeChannel(Channel *channel) {
  int fd = channel->fd();
  LOG_INFO("[%s:%s:%d]\nfd = %d events = %d\n", __FILE__, __FUNCTION__,
           __LINE__, fd, channel->events());
  channels_.erase(fd);
  int index = channel->index();
  if (index == kAdded) {
    // 将 channel 从 epoll 中删掉
    update(EPOLL_CTL_DEL, channel);
  }
  channel->set_index(kNew);
}

// 该函数被 EventLoop 中的 loop() 函数调用，内部通过 epoll_wait 监听哪些 channel
// 发生了事件， 将发生的事件通过 fillActiveChannels() 方法将 activeChannels 写入
// 到 EventLoop 的 ChannelList 实参中，具体看 EventLoop 中的代码逻辑
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels) {
  // 用 LOG_DEBUG 输出日志更为合理，能降低高并发时对 poll 的性能影响
  LOG_DEBUG("[%s:%s:%d]\nfd total count: %d\n", __FILE__, __FUNCTION__, __LINE__,
           static_cast<int>(channels_.size()));
  // epoll_wait 第二个参数应该是 events 数组的首地址，为了方便扩容，这里用
  // vector 存储 event vector 的底层也是数组，events_.begin()
  // 是首个元素的迭代器， 解引用后就是首个元素的值，再取地址就是数组首地址
  // 另外，也可以用 &events_[0]、&events.front()、&events.at(0)、events.data()
  int numEvents = ::epoll_wait(epollfd_, &*events_.begin(),
                               static_cast<int>(events_.size()), timeoutMs);
  // 将全局 errno 提取为局部变量，因为每个线程都可能设置全局 errno
  int saveErrno = errno;
  // 获取当前时间
  Timestamp now(Timestamp::now());
  if (numEvents > 0) {
    LOG_DEBUG("[%s:%s:%d]\n%d events happened\n", __FILE__, __FUNCTION__, __LINE__,
             numEvents);
    fillActiveChannels(numEvents, activeChannels);
    if (numEvents == events_.size()) {
      events_.resize(events_.size() * 2);
    }
  } else if (numEvents == 0) {
    LOG_DEBUG("[%s:%s:%d]\ntimeout!\n", __FILE__, __FUNCTION__, __LINE__);
  } else {
    // 如果不是外部中断
    if (saveErrno != EINTR) {
      errno = saveErrno;
      // 日志系统
      LOG_ERROR("[%s:%s:%d]\nEPollpoller::poll() error!\n", __FILE__,
                __FUNCTION__, __LINE__);
    }
  }
  return now;
}

//  将 poller 返回的 events 赋值到 EventLoop 所有发生事件的 channelList 列表中
void EPollPoller::fillActiveChannels(int numEvents,
                                     ChannelList *activeChannels) const {
  for (int i = 0; i < numEvents; ++i) {
    Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
    channel->set_revents(events_[i].events);
    activeChannels->push_back(channel);
  }
}
