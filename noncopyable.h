#pragma once

/*
 * noncopyable 类被继承以后，派生类的对象可以正常构造和析构， 但是派生类对象无法进行拷贝构造和赋值
*/

class noncopyable {
public:
  noncopyable(const noncopyable &) = delete;
  noncopyable &operator = (const noncopyable &) = delete;

protected:
  noncopyable() = default;
  ~noncopyable() = default;
};