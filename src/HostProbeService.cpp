#include "HostProbeService.h"

#include <ESP32Ping.h>
#include <WiFi.h>

bool HostProbeService::isHostReachable(const String& hostIp, uint16_t port, uint32_t timeoutMs) const {
  if (port == 0) {
    return false;
  }

  IPAddress ip;
  if (!ip.fromString(hostIp)) {
    return false;
  }

  const bool pingReachable = Ping.ping(ip, 1);

  WiFiClient client;
  const bool tcpReachable = client.connect(ip, port, timeoutMs);
  if (tcpReachable) {
    client.stop();
  }
  
  return pingReachable || tcpReachable;
}
