#include "Timestamp.h"
#include <time.h>

Timestamp::Timestamp() : microSecondsSinceEpoch_(0) {}
Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
    : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}

Timestamp Timestamp::now() {
  // time_t ti = time(NULL);
  return Timestamp(time(NULL));
}

std::string Timestamp::toString() const {
  char buf[128] = {0};
  tm *tm_time = localtime(&microSecondsSinceEpoch_);
  snprintf(buf, sizeof(buf), "%4d/%02d/%02d %02d:%02d:%02d",
      tm_time->tm_year + 1900,
      tm_time->tm_mon + 1,
      tm_time->tm_mday,
      tm_time->tm_hour,
      tm_time->tm_min,
      tm_time->tm_sec);
  return buf;
}


//#define TEST_TIME
#ifdef TEST_TIME
#include <iostream>
int main() {
  Timestamp tm;
  std::cout << tm.now().toString() << '\n';
  return 0;
}
#endif