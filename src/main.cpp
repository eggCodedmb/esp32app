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

void startSetupAccessPoint() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(kSetupApSsid, kSetupApPassword);

  Serial.print("Setup AP started: ");
  Serial.println(kSetupApSsid);
  Serial.print("Setup AP IP: ");
  Serial.println(WiFi.softAPIP());
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  authService.begin();
  startSetupAccessPoint();
  webPortal.begin();

  Serial.println("Web portal ready.");
}

void loop() {
  powerOnService.tick(wifiService.isConnected());
  delay(100);
}
