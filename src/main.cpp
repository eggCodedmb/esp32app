#include <Arduino.h>
#include <WiFi.h>

#include "AuthService.h"
#include "BemfaService.h"
#include "ConfigStore.h"
#include "FirmwareUpgradeService.h"
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
BemfaService bemfaService;
FirmwareUpgradeService firmwareUpgradeService;
WebPortal webPortal(80,
                    authService,
                    wifiService,
                    configStore,
                    powerOnService,
                    bemfaService,
                    firmwareUpgradeService);
bool setupApRunning = false;
bool wifiStateInitialized = false;
bool lastWifiConnected = false;
String lastReportedPowerState = "";

bool isPowerOnCommand(const String& command) {
  return command == "on" || command == "1" || command == "start" || command == "wake";
}

bool isOtaCommand(const String& command) {
  return command == "ota" || command == "upgrade" || command == "update";
}

void onFirmwareUpgradeEvent(const FirmwareUpgradeStatus& status, void* context) {
  BemfaService* bemfa = static_cast<BemfaService*>(context);
  if (bemfa == nullptr || !bemfa->isConnected()) {
    return;
  }

  String payload = "ota:" + status.state;
  if (status.progressTotalBytes > 0) {
    payload += ",progress:" + String(status.progressPercent);
  }
  if (!status.lastError.isEmpty()) {
    payload += ",error:" + status.lastError;
  }
  bemfa->publishStatus(payload);
}

void handleBemfaCommand(bool wifiConnected) {
  String command;
  if (!bemfaService.takeCommand(&command)) {
    return;
  }

  Serial.print("Bemfa command received: ");
  Serial.println(command);

  if (command == "status") {
    const PowerOnStatus status = powerOnService.getStatus();
    const FirmwareUpgradeStatus otaStatus = firmwareUpgradeService.getStatus();
    String payload = "power:" + status.stateText + ",ota:" + otaStatus.state;
    if (otaStatus.progressTotalBytes > 0) {
      payload += ",progress:" + String(otaStatus.progressPercent);
    }
    bemfaService.publishStatus(payload);
    return;
  }

  if (isOtaCommand(command)) {
    String errorCode;
    const bool accepted = firmwareUpgradeService.requestManualUpgrade(wifiConnected, &errorCode);
    if (!accepted) {
      const FirmwareUpgradeStatus otaStatus = firmwareUpgradeService.getStatus();
      const String normalizedError = errorCode.isEmpty() ? otaStatus.lastError : errorCode;
      bemfaService.publishStatus("ota:" + otaStatus.state + ",error:" + normalizedError);
      return;
    }

    bemfaService.publishStatus("ota:QUEUED");
    return;
  }

  if (!isPowerOnCommand(command)) {
    bemfaService.publishStatus("unknown_command");
    return;
  }

  const ComputerConfig config = configStore.loadComputerConfig();
  String errorCode;
  const bool accepted = powerOnService.requestPowerOn(config, wifiConnected, &errorCode);
  powerOnService.tick(wifiConnected);

  if (!accepted) {
    const PowerOnStatus status = powerOnService.getStatus();
    const String normalizedError = errorCode.isEmpty() ? status.errorCode : errorCode;
    bemfaService.publishStatus("power:" + status.stateText + ",error:" + normalizedError);
    return;
  }

  bemfaService.publishStatus("power:BOOTING");
}

void reportPowerStateIfChanged() {
  if (!bemfaService.isConnected()) {
    return;
  }

  const PowerOnStatus status = powerOnService.getStatus();
  if (status.stateText == lastReportedPowerState) {
    return;
  }

  String payload = "power:" + status.stateText;
  if (!status.errorCode.isEmpty()) {
    payload += ",error:" + status.errorCode;
  }
  if (bemfaService.publishStatus(payload)) {
    lastReportedPowerState = status.stateText;
  }
}

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
  const bool restored = wifiService.reconnectFromStored();
  if (restored) {
    Serial.print("Connected using stored WiFi credentials: ");
    Serial.println(wifiService.currentSsid());
  } else {
    Serial.print("Stored WiFi reconnect not ready: ");
    Serial.println(wifiService.lastMessage());
  }

  syncSetupAccessPoint(wifiService.isConnected());
  bemfaService.begin();
  firmwareUpgradeService.setEventCallback(onFirmwareUpgradeEvent, &bemfaService);
  firmwareUpgradeService.begin();

  const BemfaConfig bemfaConfig = configStore.loadBemfaConfig();
  const SystemConfig systemConfig = configStore.loadSystemConfig();
  bemfaService.updateConfig(bemfaConfig);
  firmwareUpgradeService.updateConfig(bemfaConfig);
  firmwareUpgradeService.updateAutoCheckConfig(false,
                                               systemConfig.otaAutoCheckIntervalMinutes);
  lastReportedPowerState = "";
  webPortal.begin();

  Serial.println("Web portal ready.");
}

void loop() {
  const bool wifiConnected = wifiService.isConnected();
  syncSetupAccessPoint(wifiConnected);
  powerOnService.tick(wifiConnected);
  bemfaService.tick(wifiConnected);
  firmwareUpgradeService.tick(wifiConnected);
  handleBemfaCommand(wifiConnected);
  reportPowerStateIfChanged();
  delay(100);
}
