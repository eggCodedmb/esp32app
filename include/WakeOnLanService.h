#pragma once

#include <Arduino.h>
#include <IPAddress.h>

class WakeOnLanService {
 public:
  bool sendMagicPacket(const String& macAddress,
                       const IPAddress& broadcastIp = IPAddress(255, 255, 255, 255),
                       uint16_t port = 9,
                       String* errorCode = nullptr) const;

 private:
  static bool parseMacAddress(const String& macAddress, uint8_t mac[6]);
};
