#pragma once

#include <Arduino.h>
#include <vector>

struct WifiNetworkInfo {
  String ssid;
  int32_t rssi;
  bool secured;
};

class WifiService {
 public:
  std::vector<WifiNetworkInfo> scanNetworks();
  bool connectTo(const String& ssid, const String& password, uint32_t timeoutMs = 15000);

  bool isConnected() const;
  String currentSsid() const;
  String ipAddress() const;
  String lastMessage() const;

 private:
  String _lastMessage = "Not connected.";
};
