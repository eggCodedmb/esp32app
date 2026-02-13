#include <Arduino.h>
#include <WiFi.h>

#include "AuthService.h"
#include "ConfigStore.h"
#include "HostProbeService.h"
#include "PowerOnService.h"
#include "WakeOnLanService.h"
#include "WebPortal.h"
#include "WifiService.h"

namespace {
constexpr const char* kSetupApSsid = "ESP32-Setup";
constexpr const char* kSetupApPassword = "12345678";

AuthService authService("admin", "admin123");
WifiService wifiService;
ConfigStore configStore;
WakeOnLanService wakeOnLanService;
HostProbeService hostProbeService;
PowerOnService powerOnService(wakeOnLanService, hostProbeService);
WebPortal webPortal(80, authService, wifiService, configStore, powerOnService);
bool setupApRunning = false;
bool wifiStateInitialized = false;
bool lastWifiConnected = false;

void startSetupAccessPoint() {
  if (setupApRunning) {
    return;
  }

  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP(kSetupApSsid, kSetupApPassword)) {
    Serial.println("Failed to start setup AP.");
    return;
  }

  setupApRunning = true;

  Serial.print("Setup AP started: ");
  Serial.println(kSetupApSsid);
  Serial.print("Setup AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void stopSetupAccessPoint() {
  if (!setupApRunning) {
    return;
  }

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  setupApRunning = false;
  Serial.println("Setup AP stopped.");
}

void syncSetupAccessPoint(bool wifiConnected) {
  if (!wifiStateInitialized || wifiConnected != lastWifiConnected) {
    if (wifiConnected) {
      stopSetupAccessPoint();
    } else {
      startSetupAccessPoint();
    }
    lastWifiConnected = wifiConnected;
    wifiStateInitialized = true;
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  authService.begin();
  syncSetupAccessPoint(wifiService.isConnected());
  webPortal.begin();

  Serial.println("Web portal ready.");
}

void loop() {
  const bool wifiConnected = wifiService.isConnected();
  syncSetupAccessPoint(wifiConnected);
  powerOnService.tick(wifiConnected);
  delay(100);
}
