#include "WakeOnLanService.h"

#include <WiFiUdp.h>
#include <cctype>
#include <cstring>

namespace {
bool isHexChar(char c) {
  return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

uint8_t hexToNibble(char c) {
  if (c >= '0' && c <= '9') {
    return static_cast<uint8_t>(c - '0');
  }
  if (c >= 'a' && c <= 'f') {
    return static_cast<uint8_t>(10 + (c - 'a'));
  }
  return static_cast<uint8_t>(10 + (c - 'A'));
}
}  // namespace

bool WakeOnLanService::sendMagicPacket(const String& macAddress,
                                       const IPAddress& broadcastIp,
                                       uint16_t port,
                                       String* errorCode) const {
  uint8_t mac[6] = {0};
  if (!parseMacAddress(macAddress, mac)) {
    if (errorCode != nullptr) {
      *errorCode = "invalid_mac";
    }
    return false;
  }

  uint8_t packet[102] = {0};
  for (size_t i = 0; i < 6; ++i) {
    packet[i] = 0xFF;
  }
  for (size_t i = 1; i <= 16; ++i) {
    std::memcpy(packet + (i * 6), mac, 6);
  }

  WiFiUDP udp;
  if (!udp.beginPacket(broadcastIp, port)) {
    if (errorCode != nullptr) {
      *errorCode = "udp_begin_failed";
    }
    return false;
  }

  const size_t written = udp.write(packet, sizeof(packet));
  const bool sent = udp.endPacket() == 1;
  if (!sent || written != sizeof(packet)) {
    if (errorCode != nullptr) {
      *errorCode = "wol_send_failed";
    }
    return false;
  }

  if (errorCode != nullptr) {
    *errorCode = "";
  }
  return true;
}

bool WakeOnLanService::parseMacAddress(const String& macAddress, uint8_t mac[6]) {
  if (mac == nullptr) {
    return false;
  }

  String compact;
  compact.reserve(12);
  for (size_t i = 0; i < macAddress.length(); ++i) {
    const char c = macAddress.charAt(i);
    if (c == ':' || c == '-' || c == ' ') {
      continue;
    }
    if (!isHexChar(c)) {
      return false;
    }
    compact += c;
  }

  if (compact.length() != 12) {
    return false;
  }

  for (size_t i = 0; i < 6; ++i) {
    const char high = compact.charAt(i * 2);
    const char low = compact.charAt((i * 2) + 1);
    mac[i] = static_cast<uint8_t>((hexToNibble(high) << 4) | hexToNibble(low));
  }
  return true;
}
