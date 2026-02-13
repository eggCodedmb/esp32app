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

  // Step 1: ICMP probe (host reachability). ICMP may be blocked by target firewall.
  const bool pingReachable = Ping.ping(ip, 1);

  // Step 2: TCP probe (service reachability on configured port).
  WiFiClient client;
  const bool tcpReachable = client.connect(ip, port, timeoutMs);
  if (tcpReachable) {
    client.stop();
  }

  // Consider host online if either ICMP or TCP probe succeeds.
  return pingReachable || tcpReachable;
}
