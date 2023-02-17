#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

// 注意 std::atomic<int> 直接初始化
std::atomic<int> Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))  // func 是在 EventLoopThread 的构造函数中设置的 EventLoopThread::threadFunc
    , name_(name) {
  setDefaultName();
}

Thread::~Thread() {
  // 线程要么处于join状态(工作线程)，要么处于分离状态
  if (started_ && !joined_) {
    // 将线程设置为分离状态，执行结束后会自动回收资源，不会成为孤儿线程
    thread_->detach(); // c++ 11 thread 类提供的设置分离线程的方法(对pthread_detach系统调用的封装)
  }
}

// 一个 Thread 对象，记录的就是一个新线程的详细信息(类中的私有属性描述的值)
void Thread::start() {
  started_ = true;
  sem_t sem;
  sem_init(&sem, false, 0);

  //------------------------新开辟的子线程---------------------------//
  // 开启子线程，使用 lambda 表达式，以隐式引用捕获的方式传递成员变量 func_
  thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
    tid_ = CurrentThread::tid();  // 获取线程的 tid 值
    sem_post(&sem);               // 子线程将信号量资源 +1 之前，原线程会一直阻塞在 sem_wait(&sem)
    func_();                      // 开启一个新线程，专门执行该线程函数
  }));
  //------------------------新开辟的子线程--------------------------//

  // 由于线程的执行顺序无法预知，而这里必须等待获取上面新创建的线程的 tid 值，所以用信号量处理线程间的同步
  sem_wait(&sem);
}

void Thread::join() {
  joined_ = true;
  thread_->join();
}

void Thread::setDefaultName() {
  int num = ++numCreated_;
  if (name_.empty()) {
    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "Thread%d", num);
    name_ = buf;
  }
}