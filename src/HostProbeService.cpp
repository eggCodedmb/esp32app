#include "HostProbeService.h"

#include <WiFi.h>

bool HostProbeService::isHostReachable(const String& hostIp, uint16_t port, uint32_t timeoutMs) const {
  if (port == 0) {
    return false;
  }

  IPAddress ip;
  if (!ip.fromString(hostIp)) {
    return false;
  }

  WiFiClient client;
  const bool connected = client.connect(ip, port, timeoutMs);
  if (connected) {
    client.stop();
  }
  return connected;
}
