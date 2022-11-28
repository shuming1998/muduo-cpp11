#include "InetAddress.h"

#include <string.h>
#include <strings.h> // bzero

InetAddress::InetAddress(uint16_t port, std::string ip) {
  bzero(&addr_, sizeof addr_);
  addr_.sin_family = AF_INET;
  addr_.sin_port = htons(port);
  // addr_.sin_addr.s_addr = inet_addr(ip.c_str());
  addr_.sin_addr.s_addr = htonl(INADDR_ANY);
}

std::string InetAddress::toIp() const {
  // 将 addr_中网络字节序的Ip地址转为本地字节序并输出
  char buf[64] = {0};
  ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
  return buf;
}

std::string InetAddress::toIpPort() const {
  // ip:port
  char buf[64] = {0};
  ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
  size_t end = strlen(buf);
  uint16_t port = ntohs(addr_.sin_port);
  sprintf(buf + end, ":%u", port);
  return buf;
}

uint16_t InetAddress::toPort() const { return ntohs(addr_.sin_port); }

// #define INET_ADDRESS_TEST
#ifdef INET_ADDRESS_TEST
#include <iostream>
int main() {
  InetAddress addr(8080);
  std::cout << addr.toIpPort() << '\n';
  return 0;
}
#endif