#pragma once

#include <Arduino.h>
#include <vector>

struct WifiNetworkInfo {
  String ssid;
  int32_t rssi;
  bool secured;
};

struct WifiScanResult {
  std::vector<WifiNetworkInfo> networks;
  bool fromCache = false;
  bool scanInProgress = false;
  uint32_t ageMs = 0;
};

class WifiService {
 public:
  WifiScanResult scanNetworks();
  bool connectTo(const String& ssid, const String& password, uint32_t timeoutMs = 15000);
  bool reconnectFromStored(uint32_t timeoutMs = 15000);

  bool isConnected() const;
  String currentSsid() const;
  String ipAddress() const;
  String lastMessage() const;

 private:
  static constexpr uint32_t kScanCacheTtlMs = 10000;
  static constexpr uint32_t kScanTimeoutMs = 15000;
  static constexpr const char* kStorageNamespace = "esp32app";
  static constexpr const char* kStorageSsidKey = "wifi_ssid";
  static constexpr const char* kStoragePasswordKey = "wifi_pwd";

  void startAsyncScan();
  bool finalizeScanIfReady();
  std::vector<WifiNetworkInfo> collectScanResults(int count) const;
  bool loadStoredCredentials(String* ssid, String* password) const;
  bool persistCredentials(const String& ssid, const String& password) const;

  std::vector<WifiNetworkInfo> _cachedNetworks;
  uint32_t _lastScanAtMs = 0;
  uint32_t _scanStartedAtMs = 0;
  bool _hasScanCache = false;
  bool _scanInProgress = false;
  String _lastMessage = "Not connected.";
};
