#pragma once
/*
 * 时间类
*/
#include <iostream>
#include <string>

// 时间类
class Timestamp {
public:
  Timestamp();
  explicit Timestamp(int64_t microSecondsSinceEpoch);
  // 获取当前时间
  static Timestamp now();
  // 时间转为字符串
  std::string toString() const;

private:
  int64_t microSecondsSinceEpoch_;
};