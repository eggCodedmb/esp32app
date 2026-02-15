#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "AuthService.h"
#include "BemfaService.h"
#include "ConfigStore.h"
#include "DdnsService.h"
#include "FirmwareUpgradeService.h"
#include "PowerOnService.h"
#include "TimeService.h"
#include "WifiService.h"

class WebPortal {
 public:
   WebPortal(uint16_t port,
             AuthService& authService,
             WifiService& wifiService,
             ConfigStore& configStore,
             PowerOnService& powerOnService,
             BemfaService& bemfaService,
             DdnsService& ddnsService,
             TimeService& timeService,
             FirmwareUpgradeService& firmwareUpgradeService);

  void begin();

#ifdef UNIT_TEST
  static String testJsonEscape(const String& value) { return jsonEscape(value); }
  static bool testParseBoolValue(const String& value, bool defaultValue = false) {
    return parseBoolValue(value, defaultValue);
  }
  String testLoginPage(const String& errorMessage = "") const { return loginPage(errorMessage); }
  String testDashboardPage() const { return dashboardPage(); }
#endif

 private:
  AsyncWebServer _server;
  AuthService& _authService;
  WifiService& _wifiService;
  ConfigStore& _configStore;
  PowerOnService& _powerOnService;
  BemfaService& _bemfaService;
  DdnsService& _ddnsService;
  TimeService& _timeService;
  FirmwareUpgradeService& _firmwareUpgradeService;

  void registerRoutes();
  bool ensureAuthorized(AsyncWebServerRequest* request, bool apiRequest) const;

  String loginPage(const String& errorMessage = "") const;
  String dashboardPage() const;

  static String jsonEscape(const String& value);
  static bool parseBoolValue(const String& value, bool defaultValue = false);
};
