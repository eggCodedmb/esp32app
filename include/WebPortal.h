#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "AuthService.h"
#include "ConfigStore.h"
#include "DeviceStateService.h"
#include "WifiService.h"

class WebPortal {
 public:
  WebPortal(uint16_t port,
            AuthService& authService,
            WifiService& wifiService,
            ConfigStore& configStore,
            DeviceStateService& deviceState);

  void begin();

 private:
  AsyncWebServer _server;
  AuthService& _authService;
  WifiService& _wifiService;
  ConfigStore& _configStore;
  DeviceStateService& _deviceState;

  void registerRoutes();
  bool ensureAuthorized(AsyncWebServerRequest* request, bool apiRequest) const;

  String loginPage(const String& errorMessage = "") const;
  String dashboardPage() const;

  static String jsonEscape(const String& value);
  static bool parseBoolValue(const String& value, bool defaultValue = false);
};
