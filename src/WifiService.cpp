#include "WifiService.h"

#include <WiFi.h>

std::vector<WifiNetworkInfo> WifiService::scanNetworks() {
  WiFi.mode(WIFI_AP_STA);

  std::vector<WifiNetworkInfo> networks;
  const int count = WiFi.scanNetworks(false, true);
  if (count <= 0) {
    WiFi.scanDelete();
    return networks;
  }

  networks.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    WifiNetworkInfo info;
    info.ssid = WiFi.SSID(i);
    info.rssi = WiFi.RSSI(i);
    info.secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    networks.push_back(info);
  }

  WiFi.scanDelete();
  return networks;
}

bool WifiService::connectTo(const String& ssid, const String& password, uint32_t timeoutMs) {
  if (ssid.isEmpty()) {
    _lastMessage = "SSID is required.";
    return false;
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  const uint32_t startTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < timeoutMs) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    _lastMessage = "Connected to " + ssid + ".";
    return true;
  }

  _lastMessage = "WiFi connection failed or timed out.";
  return false;
}

bool WifiService::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

String WifiService::currentSsid() const {
  return isConnected() ? WiFi.SSID() : "";
}

String WifiService::ipAddress() const {
  return isConnected() ? WiFi.localIP().toString() : "";
}

String WifiService::lastMessage() const {
  return _lastMessage;
}
