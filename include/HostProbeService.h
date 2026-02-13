#pragma once

#include <Arduino.h>

class HostProbeService {
 public:
  bool isHostReachable(const String& hostIp, uint16_t port, uint32_t timeoutMs = 1200) const;
};
