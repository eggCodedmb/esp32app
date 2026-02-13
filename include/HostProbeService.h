#pragma once

#include <Arduino.h>

class HostProbeService {
 public:
  // Returns true when host is reachable by ICMP ping or TCP connect on the given port.
  bool isHostReachable(const String& hostIp, uint16_t port, uint32_t timeoutMs = 1200) const;
};
