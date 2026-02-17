#include "WifiService.h"

#include <Preferences.h>
#include <WiFi.h>
#include <algorithm>

namespace {
constexpr int kWifiScanRunning = WIFI_SCAN_RUNNING;
constexpr int kWifiScanFailed = WIFI_SCAN_FAILED;

void ensureStationEnabled() {
  const wifi_mode_t currentMode = WiFi.getMode();
  if (currentMode == WIFI_MODE_AP) {
    WiFi.mode(WIFI_AP_STA);
    return;
  }
  if (currentMode == WIFI_MODE_NULL) {
    WiFi.mode(WIFI_STA);
  }
}
}  // namespace

WifiScanResult WifiService::scanNetworks() {
  ensureStationEnabled();

  const uint32_t previousScanAtMs = _lastScanAtMs;
  const bool refreshed = finalizeScanIfReady();

  WifiScanResult result;
  result.fromCache = true;
  result.scanInProgress = _scanInProgress;
  result.ageMs = _hasScanCache ? (millis() - _lastScanAtMs) : 0;

  if (!_hasScanCache || result.ageMs >= kScanCacheTtlMs) {
    if (!_scanInProgress) {
      startAsyncScan();
    }
    result.scanInProgress = _scanInProgress;
  }

  if (_hasScanCache) {
    const bool immediateRefresh = _lastScanAtMs != previousScanAtMs;
    result.networks = _cachedNetworks;
    result.ageMs = millis() - _lastScanAtMs;
    result.fromCache = !(refreshed || immediateRefresh);
    return result;
  }

  result.fromCache = false;
  return result;
}

void WifiService::startAsyncScan() {
  WiFi.scanDelete();

  const int startResult = WiFi.scanNetworks(true, true);
  if (startResult == kWifiScanFailed) {
    _scanInProgress = false;
    return;
  }

  if (startResult >= 0) {
    _cachedNetworks = collectScanResults(startResult);
    _hasScanCache = true;
    _lastScanAtMs = millis();
    WiFi.scanDelete();
    _scanInProgress = false;
    return;
  }

  _scanInProgress = true;
  _scanStartedAtMs = millis();
}

bool WifiService::finalizeScanIfReady() {
  if (!_scanInProgress) {
    return false;
  }

  if ((millis() - _scanStartedAtMs) > kScanTimeoutMs) {
    WiFi.scanDelete();
    _scanInProgress = false;
    return false;
  }

  const int status = WiFi.scanComplete();
  if (status == kWifiScanRunning) {
    return false;
  }

  _scanInProgress = false;
  if (status == kWifiScanFailed || status < 0) {
    WiFi.scanDelete();
    return false;
  }

  _cachedNetworks = collectScanResults(status);
  _hasScanCache = true;
  _lastScanAtMs = millis();
  WiFi.scanDelete();
  return true;
}

std::vector<WifiNetworkInfo> WifiService::collectScanResults(int count) const {
  std::vector<WifiNetworkInfo> networks;
  if (count <= 0) {
    return networks;
  }

  networks.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    String ssid = WiFi.SSID(i);
    ssid.trim();
    if (ssid.isEmpty()) {
      continue;
    }

    WifiNetworkInfo candidate;
    candidate.ssid = ssid;
    candidate.rssi = WiFi.RSSI(i);
    candidate.secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;

    bool merged = false;
    for (size_t existingIndex = 0; existingIndex < networks.size(); ++existingIndex) {
      if (networks[existingIndex].ssid == candidate.ssid) {
        if (candidate.rssi > networks[existingIndex].rssi) {
          networks[existingIndex] = candidate;
        }
        merged = true;
        break;
      }
    }

    if (!merged) {
      networks.push_back(candidate);
    }
  }

  std::sort(networks.begin(), networks.end(), [](const WifiNetworkInfo& lhs, const WifiNetworkInfo& rhs) {
    if (lhs.rssi == rhs.rssi) {
      return lhs.ssid.compareTo(rhs.ssid) < 0;
    }
    return lhs.rssi > rhs.rssi;
  });

  return networks;
}

bool WifiService::connectTo(const String& ssid, const String& password, uint32_t timeoutMs) {
  String normalizedSsid = ssid;
  normalizedSsid.trim();
  if (normalizedSsid.isEmpty()) {
    _lastMessage = "SSID is required.";
    return false;
  }

  ensureStationEnabled();
  WiFi.begin(normalizedSsid.c_str(), password.c_str());

  const uint32_t startTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < timeoutMs) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    const bool persisted = persistCredentials(normalizedSsid, password);
    _lastMessage = persisted ? ("Connected to " + normalizedSsid + ".")
                             : ("Connected to " + normalizedSsid + ", but failed to save credentials.");
    return true;
  }

  _lastMessage = "WiFi connection failed or timed out.";
  return false;
}

bool WifiService::reconnectFromStored(uint32_t timeoutMs) {
  String storedSsid;
  String storedPassword;
  if (!loadStoredCredentials(&storedSsid, &storedPassword)) {
    _lastMessage = "No stored WiFi credentials.";
    return false;
  }

  const bool connected = connectTo(storedSsid, storedPassword, timeoutMs);
  if (!connected) {
    _lastMessage = "Stored WiFi credentials failed for SSID: " + storedSsid;
  }
  return connected;
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

bool WifiService::loadStoredCredentials(String* ssid, String* password) const {
  if (ssid == nullptr || password == nullptr) {
    return false;
  }

  Preferences preferences;
  if (!preferences.begin(kStorageNamespace, true)) {
    return false;
  }

  *ssid = preferences.getString(kStorageSsidKey, "");
  *password = preferences.getString(kStoragePasswordKey, "");
  preferences.end();

  ssid->trim();
  return !ssid->isEmpty();
}

bool WifiService::persistCredentials(const String& ssid, const String& password) const {
  Preferences preferences;
  if (!preferences.begin(kStorageNamespace, false)) {
    return false;
  }

  preferences.putString(kStorageSsidKey, ssid);
  preferences.putString(kStoragePasswordKey, password);
  preferences.end();

  return true;
}
