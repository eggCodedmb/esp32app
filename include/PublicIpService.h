#pragma once

#include <Arduino.h>

class PublicIpService {
 public:
  static constexpr uint32_t kDefaultTimeoutMs = 5000;

  // Resolve current IP. If useLocalIp is true, returns WiFi STA local IP.
  static String resolve(bool useLocalIp = false, uint32_t timeoutMs = kDefaultTimeoutMs);

 private:
  static String fetchFromUrl(const char* url, uint32_t timeoutMs);
  static String extractIpv4FromText(const String& response);
};
