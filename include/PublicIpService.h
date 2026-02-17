#pragma once

#include <Arduino.h>

class PublicIpService {
 public:
  static constexpr uint32_t kDefaultTimeoutMs = 5000;

  // Resolve current IP. If useLocalIp is true and useIpv6 is false, returns WiFi STA local IPv4.
  static String resolve(bool useLocalIp = false,
                        bool useIpv6 = false,
                        uint32_t timeoutMs = kDefaultTimeoutMs);

 private:
  static String fetchFromUrl(const char* url, bool useIpv6, uint32_t timeoutMs);
  static String extractIpv4FromText(const String& response);
  static String extractIpv6FromText(const String& response);
};
