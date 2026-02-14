#include "WebPortal.h"

#include <cctype>
#include <cstdio>
#include <ESP.h>

#include "DdnsProviderRegistry.h"

namespace {
constexpr uint16_t kDefaultStatusPollIntervalMinutes = 3;
constexpr uint32_t kDefaultDdnsIntervalSeconds = 300;
constexpr uint32_t kMinDdnsIntervalSeconds = 30;
constexpr uint32_t kMaxDdnsIntervalSeconds = 86400;

bool normalizeMacAddress(const String& source, String* normalized) {
  if (normalized == nullptr) {
    return false;
  }

  String compact;
  compact.reserve(12);

  for (size_t i = 0; i < source.length(); ++i) {
    const char c = source.charAt(i);
    if (c == ':' || c == '-' || c == ' ') {
      continue;
    }
    if (!std::isxdigit(static_cast<unsigned char>(c))) {
      return false;
    }
    compact += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }

  if (compact.length() != 12) {
    return false;
  }

  normalized->clear();
  normalized->reserve(17);
  for (size_t i = 0; i < compact.length(); ++i) {
    if (i > 0 && (i % 2) == 0) {
      *normalized += ':';
    }
    *normalized += compact.charAt(i);
  }

  return true;
}

bool isValidStatusPollIntervalMinutes(uint16_t value) {
  return value == 0 || value == 1 || value == 3 || value == 10 || value == 30 || value == 60;
}

uint16_t normalizeStatusPollIntervalMinutes(uint16_t value) {
  if (!isValidStatusPollIntervalMinutes(value)) {
    return kDefaultStatusPollIntervalMinutes;
  }
  return value;
}

uint16_t parseStatusPollIntervalMinutes(const String& value, uint16_t defaultValue) {
  String normalized = value;
  normalized.trim();
  normalized.toLowerCase();

  if (normalized == "manual") {
    return 0;
  }

  const int parsedValue = normalized.toInt();
  if (parsedValue >= 0 && parsedValue <= 65535) {
    const uint16_t parsedInterval = static_cast<uint16_t>(parsedValue);
    if (isValidStatusPollIntervalMinutes(parsedInterval)) {
      return parsedInterval;
    }
  }

  return normalizeStatusPollIntervalMinutes(defaultValue);
}

String normalizeDdnsProvider(const String& provider) {
  return DdnsProviderRegistry::normalizeProvider(provider);
}

uint32_t normalizeDdnsIntervalSeconds(uint32_t value) {
  if (value < kMinDdnsIntervalSeconds || value > kMaxDdnsIntervalSeconds) {
    return kDefaultDdnsIntervalSeconds;
  }
  return value;
}
}  // namespace

WebPortal::WebPortal(uint16_t port,
                     AuthService& authService,
                     WifiService& wifiService,
                     ConfigStore& configStore,
                     PowerOnService& powerOnService,
                     BemfaService& bemfaService,
                     DdnsService& ddnsService,
                     FirmwareUpgradeService& firmwareUpgradeService)
    : _server(port),
      _authService(authService),
      _wifiService(wifiService),
      _configStore(configStore),
      _powerOnService(powerOnService),
      _bemfaService(bemfaService),
      _ddnsService(ddnsService),
      _firmwareUpgradeService(firmwareUpgradeService) {}

void WebPortal::begin() {
  registerRoutes();
  _server.begin();
}

void WebPortal::registerRoutes() {
  _server.on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, false)) {
      return;
    }
    request->send(200, "text/html; charset=utf-8", dashboardPage());
  });

  _server.on("/login", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (_authService.isAuthorized(request)) {
      AsyncWebServerResponse* response = request->beginResponse(302);
      response->addHeader("Location", "/");
      request->send(response);
      return;
    }
    request->send(200, "text/html; charset=utf-8", loginPage());
  });

  _server.on("/login", HTTP_POST, [this](AsyncWebServerRequest* request) {
    const String username =
        request->hasParam("username", true) ? request->getParam("username", true)->value() : "";
    const String password =
        request->hasParam("password", true) ? request->getParam("password", true)->value() : "";

    if (_authService.validateCredentials(username, password)) {
      const String token = _authService.issueSessionToken();
      AsyncWebServerResponse* response = request->beginResponse(302);
      response->addHeader("Location", "/");
      response->addHeader("Set-Cookie",
                          "ESPSESSION=" + token +
                              "; Path=/; HttpOnly; SameSite=Lax; Max-Age=" +
                              String(_authService.sessionTtlSeconds()));
      request->send(response);
      return;
    }

    request->send(401, "text/html; charset=utf-8", loginPage("用户名或密码错误"));
  });

  _server.on("/logout", HTTP_GET, [this](AsyncWebServerRequest* request) {
    _authService.clearSession();
    AsyncWebServerResponse* response = request->beginResponse(302);
    response->addHeader("Location", "/login");
    response->addHeader("Set-Cookie", "ESPSESSION=deleted; Path=/; Max-Age=0");
    request->send(response);
  });

  _server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    const ComputerConfig config = _configStore.loadComputerConfig();
    const BemfaConfig bemfaConfig = _configStore.loadBemfaConfig();
    const DdnsConfig ddnsConfig = _configStore.loadDdnsConfig();
    const SystemConfig systemConfig = _configStore.loadSystemConfig();
    const BemfaRuntimeStatus bemfaStatus = _bemfaService.getStatus();
    const DdnsRuntimeStatus ddnsStatus = _ddnsService.getStatus();
    const PowerOnStatus power = _powerOnService.getStatus();
    const String otaCurrentVersion =
        systemConfig.otaInstalledVersionCode >= 0 ? String(systemConfig.otaInstalledVersionCode) : "-";

    String body = "{";
    body += "\"computerIp\":\"" + jsonEscape(config.ip) + "\",";
    body += "\"computerMac\":\"" + jsonEscape(config.mac) + "\",";
    body += "\"computerPort\":" + String(config.port) + ",";
    body += "\"powerState\":\"" + jsonEscape(power.stateText) + "\",";
    body += "\"powerMessage\":\"" + jsonEscape(power.message) + "\",";
    body += "\"powerBusy\":" + String(power.busy ? "true" : "false") + ",";
    body += "\"wifiConnected\":" + String(_wifiService.isConnected() ? "true" : "false") + ",";
    body += "\"wifiSsid\":\"" + jsonEscape(_wifiService.currentSsid()) + "\",";
    body += "\"wifiIp\":\"" + jsonEscape(_wifiService.ipAddress()) + "\",";
    body += "\"bemfaEnabled\":" + String(bemfaConfig.enabled ? "true" : "false") + ",";
    body += "\"bemfaHost\":\"" + jsonEscape(bemfaConfig.host) + "\",";
    body += "\"bemfaPort\":" + String(bemfaConfig.port) + ",";
    body += "\"bemfaUid\":\"" + jsonEscape(bemfaConfig.uid) + "\",";
    body += "\"bemfaKey\":\"" + jsonEscape(bemfaConfig.key) + "\",";
    body += "\"bemfaTopic\":\"" + jsonEscape(bemfaConfig.topic) + "\",";
    body += "\"bemfaConnected\":" + String(bemfaStatus.mqttConnected ? "true" : "false") + ",";
    body += "\"bemfaState\":\"" + jsonEscape(bemfaStatus.state) + "\",";
    body += "\"bemfaMessage\":\"" + jsonEscape(bemfaStatus.message) + "\",";
    body += "\"statusPollIntervalMinutes\":" + String(systemConfig.statusPollIntervalMinutes) + ",";
    body += "\"otaCurrentVersion\":\"" + jsonEscape(otaCurrentVersion) + "\",";
    body += "\"otaAutoCheckEnabled\":false,";
    body += "\"otaAutoCheckIntervalMinutes\":" +
            String(systemConfig.otaAutoCheckIntervalMinutes) + ",";
    body += "\"ddnsEnabled\":" + String(ddnsConfig.enabled ? "true" : "false") + ",";
    body += "\"ddnsState\":\"" + jsonEscape(ddnsStatus.state) + "\",";
    body += "\"ddnsMessage\":\"" + jsonEscape(ddnsStatus.message) + "\",";
    body += "\"ddnsActiveRecordCount\":" + String(ddnsStatus.activeRecordCount) + ",";
    body += "\"ddnsTotalUpdateCount\":" + String(ddnsStatus.totalUpdateCount) + ",";
    body += "\"ddnsRecords\":[";
    for (size_t index = 0; index < ddnsConfig.records.size(); ++index) {
      const DdnsRecordConfig& record = ddnsConfig.records[index];
      body += "{";
      body += "\"enabled\":" + String(record.enabled ? "true" : "false") + ",";
      body += "\"provider\":\"" + jsonEscape(record.provider) + "\",";
      body += "\"domain\":\"" + jsonEscape(record.domain) + "\",";
      body += "\"username\":\"" + jsonEscape(record.username) + "\",";
      body += "\"password\":\"" + jsonEscape(record.password) + "\",";
      body += "\"updateIntervalSeconds\":" + String(record.updateIntervalSeconds) + ",";
      body += "\"useLocalIp\":" + String(record.useLocalIp ? "true" : "false");
      body += "}";
      if (index + 1 < ddnsConfig.records.size()) {
        body += ",";
      }
    }
    body += "]";
    body += "}";

    request->send(200, "application/json", body);
  });

  _server.on("/api/config", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    ComputerConfig config = _configStore.loadComputerConfig();
    if (request->hasParam("computerIp", true)) {
      config.ip = request->getParam("computerIp", true)->value();
    }
    if (request->hasParam("computerPort", true)) {
      const int parsedPort = request->getParam("computerPort", true)->value().toInt();
      if (parsedPort > 0 && parsedPort <= 65535) {
        config.port = static_cast<uint16_t>(parsedPort);
      }
    }
    if (request->hasParam("computerMac", true)) {
      String normalizedMac;
      if (!normalizeMacAddress(request->getParam("computerMac", true)->value(), &normalizedMac)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"invalid_mac\"}");
        return;
      }
      config.mac = normalizedMac;
    }

    BemfaConfig bemfaConfig = _configStore.loadBemfaConfig();
    if (request->hasParam("bemfaEnabled", true)) {
      bemfaConfig.enabled = parseBoolValue(request->getParam("bemfaEnabled", true)->value(), false);
    }
    if (request->hasParam("bemfaHost", true)) {
      bemfaConfig.host = request->getParam("bemfaHost", true)->value();
      bemfaConfig.host.trim();
      if (bemfaConfig.host.isEmpty()) {
        bemfaConfig.host = "bemfa.com";
      }
    }
    if (request->hasParam("bemfaPort", true)) {
      const int parsedBemfaPort = request->getParam("bemfaPort", true)->value().toInt();
      if (parsedBemfaPort > 0 && parsedBemfaPort <= 65535) {
        bemfaConfig.port = static_cast<uint16_t>(parsedBemfaPort);
      }
    }
    if (request->hasParam("bemfaUid", true)) {
      bemfaConfig.uid = request->getParam("bemfaUid", true)->value();
      bemfaConfig.uid.trim();
    }
    if (request->hasParam("bemfaKey", true)) {
      bemfaConfig.key = request->getParam("bemfaKey", true)->value();
      bemfaConfig.key.trim();
    }
    if (request->hasParam("bemfaTopic", true)) {
      bemfaConfig.topic = request->getParam("bemfaTopic", true)->value();
      bemfaConfig.topic.trim();
      while (bemfaConfig.topic.endsWith("/")) {
        bemfaConfig.topic.remove(bemfaConfig.topic.length() - 1);
      }
    }

    SystemConfig systemConfig = _configStore.loadSystemConfig();
    if (request->hasParam("statusPollIntervalMinutes", true)) {
      const String rawInterval = request->getParam("statusPollIntervalMinutes", true)->value();
      systemConfig.statusPollIntervalMinutes =
          parseStatusPollIntervalMinutes(rawInterval, systemConfig.statusPollIntervalMinutes);
    }
    systemConfig.otaAutoCheckEnabled = false;

    DdnsConfig ddnsConfig = _configStore.loadDdnsConfig();
    if (request->hasParam("ddnsEnabled", true)) {
      ddnsConfig.enabled = parseBoolValue(request->getParam("ddnsEnabled", true)->value(), false);
    }
    if (request->hasParam("ddnsRecordCount", true)) {
      const int parsedRecordCount = request->getParam("ddnsRecordCount", true)->value().toInt();
      size_t recordCount = 0;
      if (parsedRecordCount > 0) {
        recordCount = static_cast<size_t>(parsedRecordCount);
      }
      if (recordCount > ConfigStore::kMaxDdnsRecords) {
        recordCount = ConfigStore::kMaxDdnsRecords;
      }

      ddnsConfig.records.clear();
      ddnsConfig.records.reserve(recordCount);
      for (size_t index = 0; index < recordCount; ++index) {
        DdnsRecordConfig record;
        String key = "ddns" + String(index) + "Enabled";
        if (request->hasParam(key, true)) {
          record.enabled = parseBoolValue(request->getParam(key, true)->value(), false);
        } else {
          record.enabled = true;
        }

        key = "ddns" + String(index) + "Provider";
        if (request->hasParam(key, true)) {
          record.provider = normalizeDdnsProvider(request->getParam(key, true)->value());
        }

        key = "ddns" + String(index) + "Domain";
        if (request->hasParam(key, true)) {
          record.domain = request->getParam(key, true)->value();
          record.domain.trim();
        }

        key = "ddns" + String(index) + "Username";
        if (request->hasParam(key, true)) {
          record.username = request->getParam(key, true)->value();
          record.username.trim();
        }

        key = "ddns" + String(index) + "Password";
        if (request->hasParam(key, true)) {
          record.password = request->getParam(key, true)->value();
          record.password.trim();
        }

        key = "ddns" + String(index) + "IntervalSeconds";
        if (request->hasParam(key, true)) {
          const int parsedInterval = request->getParam(key, true)->value().toInt();
          if (parsedInterval > 0) {
            record.updateIntervalSeconds = static_cast<uint32_t>(parsedInterval);
          }
        }
        record.updateIntervalSeconds =
            normalizeDdnsIntervalSeconds(record.updateIntervalSeconds);

        key = "ddns" + String(index) + "UseLocalIp";
        if (request->hasParam(key, true)) {
          record.useLocalIp = parseBoolValue(request->getParam(key, true)->value(), false);
        }

        ddnsConfig.records.push_back(record);
      }
    }

    const bool computerSaved = _configStore.saveComputerConfig(config);
    const bool bemfaSaved = _configStore.saveBemfaConfig(bemfaConfig);
    const bool systemSaved = _configStore.saveSystemConfig(systemConfig);
    const bool ddnsSaved = _configStore.saveDdnsConfig(ddnsConfig);
    if (!computerSaved || !bemfaSaved || !systemSaved || !ddnsSaved) {
      request->send(500, "application/json", "{\"success\":false,\"error\":\"save_failed\"}");
      return;
    }

    _bemfaService.updateConfig(bemfaConfig);
    _ddnsService.updateConfig(ddnsConfig);
    _firmwareUpgradeService.updateConfig(bemfaConfig);
    _firmwareUpgradeService.updateAutoCheckConfig(false,
                                                  systemConfig.otaAutoCheckIntervalMinutes);
    request->send(200, "application/json", "{\"success\":true}");
  });

  _server.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    const WifiScanResult scanResult = _wifiService.scanNetworks();
    const std::vector<WifiNetworkInfo>& networks = scanResult.networks;
    String body = "{\"networks\":[";
    for (size_t i = 0; i < networks.size(); ++i) {
      body += "{";
      body += "\"ssid\":\"" + jsonEscape(networks[i].ssid) + "\",";
      body += "\"rssi\":" + String(networks[i].rssi) + ",";
      body += "\"secured\":" + String(networks[i].secured ? "true" : "false");
      body += "}";
      if (i + 1 < networks.size()) {
        body += ",";
      }
    }
    body += "],";
    body += "\"fromCache\":" + String(scanResult.fromCache ? "true" : "false") + ",";
    body += "\"scanInProgress\":" + String(scanResult.scanInProgress ? "true" : "false") + ",";
    body += "\"ageMs\":" + String(scanResult.ageMs) + ",";
    body += "\"connected\":" + String(_wifiService.isConnected() ? "true" : "false") + ",";
    body += "\"currentSsid\":\"" + jsonEscape(_wifiService.currentSsid()) + "\",";
    body += "\"ip\":\"" + jsonEscape(_wifiService.ipAddress()) + "\"";
    body += "}";

    request->send(200, "application/json", body);
  });

  _server.on("/api/wifi/connect", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    const String ssid =
        request->hasParam("ssid", true) ? request->getParam("ssid", true)->value() : "";
    const String password =
        request->hasParam("password", true) ? request->getParam("password", true)->value() : "";

    if (ssid.isEmpty()) {
      request->send(400, "application/json", "{\"success\":false,\"error\":\"ssid_required\"}");
      return;
    }

    const bool connected = _wifiService.connectTo(ssid, password);
    String body = "{";
    body += "\"success\":" + String(connected ? "true" : "false") + ",";
    body += "\"message\":\"" + jsonEscape(_wifiService.lastMessage()) + "\",";
    body += "\"ip\":\"" + jsonEscape(_wifiService.ipAddress()) + "\"";
    body += "}";

    request->send(connected ? 200 : 500, "application/json", body);
  });

  _server.on("/api/power/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    const PowerOnStatus status = _powerOnService.getStatus();
    String body = "{";
    body += "\"state\":\"" + jsonEscape(status.stateText) + "\",";
    body += "\"message\":\"" + jsonEscape(status.message) + "\",";
    body += "\"error\":\"" + jsonEscape(status.errorCode) + "\",";
    body += "\"busy\":" + String(status.busy ? "true" : "false");
    body += "}";
    request->send(200, "application/json", body);
  });

  _server.on("/api/bemfa/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    const BemfaRuntimeStatus status = _bemfaService.getStatus();
    String body = "{";
    body += "\"enabled\":" + String(status.enabled ? "true" : "false") + ",";
    body += "\"configured\":" + String(status.configured ? "true" : "false") + ",";
    body += "\"wifiConnected\":" + String(status.wifiConnected ? "true" : "false") + ",";
    body += "\"connected\":" + String(status.mqttConnected ? "true" : "false") + ",";
    body += "\"state\":\"" + jsonEscape(status.state) + "\",";
    body += "\"message\":\"" + jsonEscape(status.message) + "\",";
    body += "\"subscribeTopic\":\"" + jsonEscape(status.subscribeTopic) + "\",";
    body += "\"publishTopic\":\"" + jsonEscape(status.publishTopic) + "\",";
    body += "\"lastCommand\":\"" + jsonEscape(status.lastCommand) + "\",";
    body += "\"lastPublish\":\"" + jsonEscape(status.lastPublish) + "\",";
    body += "\"reconnectCount\":" + String(status.reconnectCount) + ",";
    body += "\"lastConnectAtMs\":" + String(status.lastConnectAtMs) + ",";
    body += "\"lastCommandAtMs\":" + String(status.lastCommandAtMs) + ",";
    body += "\"lastPublishAtMs\":" + String(status.lastPublishAtMs);
    body += "}";

    request->send(200, "application/json", body);
  });

  _server.on("/api/ddns/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    const DdnsRuntimeStatus status = _ddnsService.getStatus();
    const std::vector<DdnsRecordRuntimeStatus> records = _ddnsService.getRecordStatuses();
    String body = "{";
    body += "\"enabled\":" + String(status.enabled ? "true" : "false") + ",";
    body += "\"configured\":" + String(status.configured ? "true" : "false") + ",";
    body += "\"wifiConnected\":" + String(status.wifiConnected ? "true" : "false") + ",";
    body += "\"state\":\"" + jsonEscape(status.state) + "\",";
    body += "\"message\":\"" + jsonEscape(status.message) + "\",";
    body += "\"activeRecordCount\":" + String(status.activeRecordCount) + ",";
    body += "\"totalUpdateCount\":" + String(status.totalUpdateCount) + ",";
    body += "\"records\":[";
    for (size_t index = 0; index < records.size(); ++index) {
      const DdnsRecordRuntimeStatus& record = records[index];
      body += "{";
      body += "\"enabled\":" + String(record.enabled ? "true" : "false") + ",";
      body += "\"configured\":" + String(record.configured ? "true" : "false") + ",";
      body += "\"provider\":\"" + jsonEscape(record.provider) + "\",";
      body += "\"domain\":\"" + jsonEscape(record.domain) + "\",";
      body += "\"username\":\"" + jsonEscape(record.username) + "\",";
      body += "\"updateIntervalSeconds\":" + String(record.updateIntervalSeconds) + ",";
      body += "\"useLocalIp\":" + String(record.useLocalIp ? "true" : "false") + ",";
      body += "\"state\":\"" + jsonEscape(record.state) + "\",";
      body += "\"message\":\"" + jsonEscape(record.message) + "\",";
      body += "\"lastOldIp\":\"" + jsonEscape(record.lastOldIp) + "\",";
      body += "\"lastNewIp\":\"" + jsonEscape(record.lastNewIp) + "\",";
      body += "\"updateCount\":" + String(record.updateCount) + ",";
      body += "\"lastUpdateAtMs\":" + String(record.lastUpdateAtMs);
      body += "}";
      if (index + 1 < records.size()) {
        body += ",";
      }
    }
    body += "]";
    body += "}";

    request->send(200, "application/json", body);
  });

  _server.on("/api/ota/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    const FirmwareUpgradeStatus status = _firmwareUpgradeService.getStatus();
    String body = "{";
    body += "\"configured\":" + String(status.configured ? "true" : "false") + ",";
    body += "\"wifiConnected\":" + String(status.wifiConnected ? "true" : "false") + ",";
    body += "\"busy\":" + String(status.busy ? "true" : "false") + ",";
    body += "\"pending\":" + String(status.pending ? "true" : "false") + ",";
    body += "\"updateAvailable\":" + String(status.updateAvailable ? "true" : "false") + ",";
    body += "\"autoCheckEnabled\":" + String(status.autoCheckEnabled ? "true" : "false") + ",";
    body += "\"autoCheckIntervalMinutes\":" + String(status.autoCheckIntervalMinutes) + ",";
    body += "\"nextAutoCheckInMs\":" + String(status.nextAutoCheckInMs) + ",";
    body += "\"progressPercent\":" + String(status.progressPercent) + ",";
    body += "\"progressBytes\":" + String(status.progressBytes) + ",";
    body += "\"progressTotalBytes\":" + String(status.progressTotalBytes) + ",";
    body += "\"trigger\":\"" + jsonEscape(status.trigger) + "\",";
    body += "\"state\":\"" + jsonEscape(status.state) + "\",";
    body += "\"message\":\"" + jsonEscape(status.message) + "\",";
    body += "\"error\":\"" + jsonEscape(status.lastError) + "\",";
    body += "\"currentVersion\":\"" + jsonEscape(status.currentVersion) + "\",";
    body += "\"targetVersion\":\"" + jsonEscape(status.targetVersion) + "\",";
    body += "\"targetTag\":\"" + jsonEscape(status.targetTag) + "\",";
    body += "\"lastAutoCheckAtMs\":" + String(status.lastAutoCheckAtMs) + ",";
    body += "\"lastProgressAtMs\":" + String(status.lastProgressAtMs) + ",";
    body += "\"lastCheckAtMs\":" + String(status.lastCheckAtMs) + ",";
    body += "\"lastStartAtMs\":" + String(status.lastStartAtMs) + ",";
    body += "\"lastFinishAtMs\":" + String(status.lastFinishAtMs);
    body += "}";

    request->send(200, "application/json", body);
  });

  _server.on("/api/ota/check", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    String errorCode;
    const bool accepted =
        _firmwareUpgradeService.requestManualCheck(_wifiService.isConnected(), &errorCode);
    const FirmwareUpgradeStatus status = _firmwareUpgradeService.getStatus();

    String body = "{";
    body += "\"success\":" + String(accepted ? "true" : "false") + ",";
    body += "\"state\":\"" + jsonEscape(status.state) + "\",";
    body += "\"message\":\"" + jsonEscape(status.message) + "\",";
    body += "\"error\":\"" + jsonEscape(errorCode.isEmpty() ? status.lastError : errorCode) + "\"";
    body += "}";

    request->send(accepted ? 202 : 400, "application/json", body);
  });

  _server.on("/api/ota/upgrade", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    String errorCode;
    const bool accepted =
        _firmwareUpgradeService.requestManualUpgrade(_wifiService.isConnected(), &errorCode);
    const FirmwareUpgradeStatus status = _firmwareUpgradeService.getStatus();

    String body = "{";
    body += "\"success\":" + String(accepted ? "true" : "false") + ",";
    body += "\"state\":\"" + jsonEscape(status.state) + "\",";
    body += "\"message\":\"" + jsonEscape(status.message) + "\",";
    body += "\"error\":\"" + jsonEscape(errorCode.isEmpty() ? status.lastError : errorCode) + "\"";
    body += "}";

    request->send(accepted ? 202 : 400, "application/json", body);
  });

  // Keep backward compatibility with old endpoint.
  _server.on("/api/ota/manual", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    String errorCode;
    const bool accepted =
        _firmwareUpgradeService.requestManualUpgrade(_wifiService.isConnected(), &errorCode);
    const FirmwareUpgradeStatus status = _firmwareUpgradeService.getStatus();

    String body = "{";
    body += "\"success\":" + String(accepted ? "true" : "false") + ",";
    body += "\"state\":\"" + jsonEscape(status.state) + "\",";
    body += "\"message\":\"" + jsonEscape(status.message) + "\",";
    body += "\"error\":\"" + jsonEscape(errorCode.isEmpty() ? status.lastError : errorCode) + "\"";
    body += "}";

    request->send(accepted ? 202 : 400, "application/json", body);
  });

  _server.on("/api/system/info", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    const uint32_t uptimeMs = millis();
    const uint32_t uptimeSeconds = uptimeMs / 1000UL;
    const uint32_t heapTotal = ESP.getHeapSize();
    const uint32_t heapFree = ESP.getFreeHeap();
    const uint32_t heapMinFree = ESP.getMinFreeHeap();
    const uint32_t heapMaxAlloc = ESP.getMaxAllocHeap();
    const uint32_t psramTotal = ESP.getPsramSize();
    const uint32_t psramFree = ESP.getFreePsram();
    const uint32_t flashTotal = ESP.getFlashChipSize();
    const uint32_t sketchUsed = ESP.getSketchSize();
    const uint32_t flashFree = flashTotal > sketchUsed ? (flashTotal - sketchUsed) : 0;

    String body = "{";
    body += "\"uptimeMs\":" + String(uptimeMs) + ",";
    body += "\"uptimeSeconds\":" + String(uptimeSeconds) + ",";
    body += "\"heapTotal\":" + String(heapTotal) + ",";
    body += "\"heapFree\":" + String(heapFree) + ",";
    body += "\"heapMinFree\":" + String(heapMinFree) + ",";
    body += "\"heapMaxAlloc\":" + String(heapMaxAlloc) + ",";
    body += "\"psramTotal\":" + String(psramTotal) + ",";
    body += "\"psramFree\":" + String(psramFree) + ",";
    body += "\"flashTotal\":" + String(flashTotal) + ",";
    body += "\"sketchUsed\":" + String(sketchUsed) + ",";
    body += "\"flashFree\":" + String(flashFree);
    body += "}";

    request->send(200, "application/json", body);
  });

  _server.on("/api/power/on", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    const ComputerConfig config = _configStore.loadComputerConfig();
    String errorCode;
    const bool accepted = _powerOnService.requestPowerOn(config, _wifiService.isConnected(), &errorCode);
    _powerOnService.tick(_wifiService.isConnected());

    const PowerOnStatus status = _powerOnService.getStatus();
    const String responseError = errorCode.isEmpty() ? status.errorCode : errorCode;
    String body = "{";
    body += "\"success\":" + String(accepted ? "true" : "false") + ",";
    body += "\"state\":\"" + jsonEscape(status.stateText) + "\",";
    body += "\"message\":\"" + jsonEscape(status.message) + "\",";
    body += "\"error\":\"" + jsonEscape(responseError) + "\",";
    body += "\"busy\":" + String(status.busy ? "true" : "false");
    body += "}";

    int statusCode = 200;
    if (!accepted) {
      if (responseError == "wol_send_failed" || responseError == "udp_begin_failed") {
        statusCode = 500;
      } else {
        statusCode = 400;
      }
    }
    request->send(statusCode, "application/json", body);
  });

  _server.on("/api/auth/password", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    const String currentPassword = request->hasParam("currentPassword", true)
                                       ? request->getParam("currentPassword", true)->value()
                                       : "";
    const String newPassword =
        request->hasParam("newPassword", true) ? request->getParam("newPassword", true)->value()
                                               : "";
    const String confirmPassword = request->hasParam("confirmPassword", true)
                                       ? request->getParam("confirmPassword", true)->value()
                                       : "";

    if (currentPassword.isEmpty() || newPassword.isEmpty() || confirmPassword.isEmpty()) {
      request->send(400, "application/json", "{\"success\":false,\"error\":\"missing_fields\"}");
      return;
    }

    if (newPassword != confirmPassword) {
      request->send(400, "application/json", "{\"success\":false,\"error\":\"confirm_not_match\"}");
      return;
    }

    String errorCode;
    const bool changed = _authService.updatePassword(currentPassword, newPassword, &errorCode);
    if (!changed) {
      const int statusCode = errorCode == "persist_failed" ? 500 : 400;
      request->send(statusCode,
                    "application/json",
                    "{\"success\":false,\"error\":\"" + jsonEscape(errorCode) + "\"}");
      return;
    }

    request->send(200, "application/json", "{\"success\":true,\"relogin\":true}");
  });

  _server.onNotFound([this](AsyncWebServerRequest* request) {
    if (request->url().startsWith("/api/")) {
      request->send(404, "application/json", "{\"error\":\"not_found\"}");
      return;
    }
    request->send(404, "text/plain", "Not Found");
  });
}

bool WebPortal::ensureAuthorized(AsyncWebServerRequest* request, bool apiRequest) const {
  if (_authService.isAuthorized(request)) {
    return true;
  }

  if (apiRequest) {
    AsyncWebServerResponse* response =
        request->beginResponse(401, "application/json", "{\"error\":\"unauthorized\"}");
    response->addHeader("Set-Cookie", "ESPSESSION=deleted; Path=/; Max-Age=0");
    request->send(response);
  } else {
    AsyncWebServerResponse* response = request->beginResponse(302);
    response->addHeader("Location", "/login");
    response->addHeader("Set-Cookie", "ESPSESSION=deleted; Path=/; Max-Age=0");
    request->send(response);
  }
  return false;
}

String WebPortal::loginPage(const String& errorMessage) const {
  String page = R"HTML(
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32 登录</title>
  <style>
    body { font-family: "Microsoft YaHei", Arial, sans-serif; background: #f3f4f6; margin: 0; }
    main { max-width: 360px; margin: 9vh auto; background: #fff; padding: 24px; border-radius: 12px; box-shadow: 0 8px 24px rgba(0,0,0,0.08); }
    h1 { margin-top: 0; font-size: 22px; }
    label { display: block; margin-top: 12px; font-size: 14px; }
    input { width: 100%; box-sizing: border-box; padding: 10px; margin-top: 6px; border: 1px solid #d1d5db; border-radius: 8px; }
    button { margin-top: 16px; width: 100%; padding: 10px; border: 0; border-radius: 8px; background: #2563eb; color: #fff; cursor: pointer; }
    .error { color: #b91c1c; margin: 8px 0 0; font-size: 13px; }
    .hint { margin-top: 14px; color: #6b7280; font-size: 12px; }
  </style>
</head>
<body>
  <main>
    <h1>ESP32 控制台登录</h1>
    __ERROR_BLOCK__
    <form method="post" action="/login">
      <label for="username">用户名</label>
      <input id="username" name="username" required>
      <label for="password">密码</label>
      <input id="password" name="password" type="password" required>
      <button type="submit">登录</button>
    </form>
    <p class="hint">默认账号：admin / admin123，建议首次登录后修改密码</p>
  </main>
</body>
</html>
)HTML";

  const String errorBlock = errorMessage.isEmpty() ? "" : "<p class=\"error\">" + errorMessage + "</p>";
  page.replace("__ERROR_BLOCK__", errorBlock);
  return page;
}

String WebPortal::dashboardPage() const {
  return R"HTML(
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32 管理页面</title>
  <style>
    :root {
      --bg: #f4f6f8;
      --card: #ffffff;
      --line: #d1d5db;
      --text: #111827;
      --accent: #0f766e;
      --danger: #b91c1c;
    }
    * { box-sizing: border-box; }
    body { margin: 0; font-family: "Microsoft YaHei", Arial, sans-serif; color: var(--text); background: linear-gradient(160deg, #ecfeff 0%, #f8fafc 45%, #fff 100%); }
    main { max-width: 900px; margin: 24px auto; padding: 0 16px; }
    header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 14px; }
    h1 { margin: 0; font-size: 24px; }
    .logout { color: #fff; text-decoration: none; padding: 8px 12px; background: #111827; border-radius: 8px; }
    section { background: var(--card); border: 1px solid var(--line); border-radius: 12px; padding: 16px; margin-bottom: 14px; box-shadow: 0 8px 24px rgba(0,0,0,0.04); }
    h2 { margin-top: 0; font-size: 18px; }
    .row { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    label { display: block; margin-top: 8px; font-size: 14px; }
    input, select { width: 100%; padding: 10px; border: 1px solid var(--line); border-radius: 8px; margin-top: 6px; }
    button { margin-top: 12px; padding: 10px 12px; border: none; border-radius: 8px; cursor: pointer; background: var(--accent); color: #fff; }
    button.secondary { background: #334155; }
    button:disabled { opacity: 0.7; cursor: not-allowed; }
    .muted { color: #4b5563; font-size: 13px; }
    .status { min-height: 20px; margin-top: 6px; font-size: 14px; }
    .inline-actions { display: flex; align-items: center; gap: 10px; flex-wrap: wrap; }
    .metrics { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 10px; }
    .metric { border: 1px solid var(--line); border-radius: 8px; padding: 10px; background: #f8fafc; }
    .metric-title { display: block; color: #4b5563; font-size: 12px; }
    .metric-value { display: block; margin-top: 6px; font-size: 15px; font-weight: 600; word-break: break-word; }
    .inline-check { display: flex; align-items: center; gap: 10px; margin-top: 8px; }
    .inline-check input[type="checkbox"] { width: auto; margin: 0; }
    .code { font-family: Consolas, Monaco, monospace; font-size: 12px; color: #374151; }
    .ddns-records { margin-top: 10px; display: flex; flex-direction: column; gap: 10px; }
    .ddns-record { border: 1px dashed var(--line); border-radius: 10px; background: #f8fafc; padding: 10px; }
    .ddns-record-header { display: flex; justify-content: space-between; align-items: center; gap: 8px; }
    .ddns-record-title { font-size: 14px; font-weight: 600; }
    @media (max-width: 720px) {
      .row { grid-template-columns: 1fr; }
      .metrics { grid-template-columns: 1fr 1fr; }
    }
    @media (max-width: 460px) {
      .metrics { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <main>
    <header>
      <h1>ESP32 控制面板</h1>
      <a class="logout" href="/logout">退出登录</a>
    </header>

    <section>
      <h2>远程开机（Wake-on-LAN）</h2>
      <p class="muted">开机状态：<strong id="powerState">待机</strong></p>
      <button id="powerButton" type="button">执行开机</button>
      <p id="powerStatus" class="status"></p>
    </section>

    <section>
      <h2>系统配置</h2>
      <form id="systemForm">
        <div class="row">
          <div>
            <label for="statusPollIntervalMinutes">状态轮询</label>
            <select id="statusPollIntervalMinutes" name="statusPollIntervalMinutes">
              <option value="manual">手动</option>
              <option value="1">1 分钟</option>
              <option value="3">3 分钟</option>
              <option value="10">10 分钟</option>
              <option value="30">30 分钟</option>
              <option value="60">60 分钟</option>
            </select>
          </div>
        </div>
        <div class="inline-actions">
          <button type="submit">保存系统配置</button>
          <button id="refreshAllButton" class="secondary" type="button" hidden>统一刷新状态</button>
        </div>
      </form>
      <p id="systemPollStatus" class="muted"></p>
      <p id="systemConfigStatus" class="status"></p>
    </section>

    <section>
      <h2>动态域名解析（DDNS）</h2>
      <form id="ddnsForm">
        <div class="inline-check">
          <input id="ddnsEnabled" name="ddnsEnabled" type="checkbox">
          <label for="ddnsEnabled" style="margin: 0;">启用 DDNS</label>
        </div>
        <div class="inline-actions">
          <button id="ddnsAddRecordButton" class="secondary" type="button">新增记录</button>
          <button type="submit">保存 DDNS 配置</button>
        </div>
        <div id="ddnsRecords" class="ddns-records"></div>
      </form>
      <p class="muted">状态：<strong id="ddnsState">-</strong>，活跃记录：<strong id="ddnsActiveCount">0</strong>，更新次数：<strong id="ddnsUpdateCount">0</strong></p>
      <p id="ddnsStatus" class="status"></p>
    </section>

    <section>
      <h2>WiFi 连接</h2>
      <div class="row">
        <div>
          <label for="wifiSelect">附近网络</label>
          <select id="wifiSelect"></select>
          <button id="scanButton" type="button">扫描 WiFi</button>
        </div>
        <div>
          <label for="wifiSsid">WiFi 名称（SSID）</label>
          <input id="wifiSsid" autocomplete="off">
          <label for="wifiPassword">WiFi 密码</label>
          <input id="wifiPassword" type="password" autocomplete="off">
          <button id="connectButton" type="button">连接</button>
        </div>
      </div>
      <p id="wifiStatus" class="status"></p>
    </section>

    <section>
      <h2>ESP 设备信息</h2>
      <div class="metrics">
        <div class="metric"><span class="metric-title">运行时长</span><span id="espUptime" class="metric-value">-</span></div>
        <div class="metric"><span class="metric-title">堆内存 空闲/总量</span><span id="espHeap" class="metric-value">-</span></div>
        <div class="metric"><span class="metric-title">堆内存最小空闲</span><span id="espHeapMin" class="metric-value">-</span></div>
        <div class="metric"><span class="metric-title">最大可分配块</span><span id="espHeapMaxAlloc" class="metric-value">-</span></div>
        <div class="metric"><span class="metric-title">Flash 已用/总量</span><span id="espFlash" class="metric-value">-</span></div>
        <div class="metric"><span class="metric-title">Flash 剩余</span><span id="espFlashFree" class="metric-value">-</span></div>
      </div>
      <p id="espInfoStatus" class="status"></p>
    </section>

    <section>
      <h2>计算机配置</h2>
      <form id="configForm">
        <div class="row">
          <div>
            <label for="computerIp">计算机 IP</label>
            <input id="computerIp" name="computerIp" autocomplete="off">
          </div>
          <div>
            <label for="computerMac">计算机 MAC 地址</label>
            <input id="computerMac" name="computerMac" placeholder="AA:BB:CC:DD:EE:FF" autocomplete="off">
          </div>
          <div>
            <label for="computerPort">探测端口</label>
            <input id="computerPort" name="computerPort" type="number" min="1" max="65535">
          </div>
        </div>
        <button type="submit">保存配置</button>
      </form>
      <p id="configStatus" class="status"></p>
    </section>

    <section>
      <h2>Bemfa Cloud</h2>
      <form id="bemfaForm">
        <div class="inline-check">
          <input id="bemfaEnabled" name="bemfaEnabled" type="checkbox">
          <label for="bemfaEnabled" style="margin: 0;">启用巴法云控制</label>
        </div>
        <div class="row">
          <div>
            <label for="bemfaTopic">主题 Topic</label>
            <input id="bemfaTopic" name="bemfaTopic" autocomplete="off" placeholder="esp32_switch">
          </div>
          <div>
            <label for="bemfaUid">私钥 UID</label>
            <input id="bemfaUid" name="bemfaUid" autocomplete="off">
          </div>
          <div>
            <label for="bemfaKey">Key（可选）</label>
            <input id="bemfaKey" name="bemfaKey" type="password" autocomplete="off">
          </div>
          <div>
            <label for="bemfaHost">主机</label>
            <input id="bemfaHost" name="bemfaHost" autocomplete="off" placeholder="bemfa.com">
          </div>
          <div>
            <label for="bemfaPort">端口</label>
            <input id="bemfaPort" name="bemfaPort" type="number" min="1" max="65535">
          </div>
        </div>
        <button type="submit">保存配置</button>
      </form>
      <p class="muted">状态：<strong id="bemfaState">-</strong>，连接：<strong id="bemfaConnected">-</strong></p>
      <p id="bemfaTopics" class="code">-</p>
      <p id="bemfaStatus" class="status"></p>
    </section>

    <section>
      <h2>固件 OTA 升级</h2>
      <p class="muted">状态：<strong id="otaState">-</strong></p>
      <p class="muted">当前版本：<span id="otaCurrentVersion" class="code">-</span></p>
      <p class="muted">目标版本：<span id="otaTargetVersion" class="code">-</span></p>
      <div class="inline-actions">
        <button id="otaCheckButton" class="secondary" type="button">检测新版本</button>
        <button id="otaUpgradeButton" type="button" hidden>升级固件</button>
      </div>
      <p id="otaStatus" class="status"></p>
    </section>

    <section>
      <h2>修改登录密码</h2>
      <form id="passwordForm">
        <div class="row">
          <div>
            <label for="currentPassword">当前密码</label>
            <input id="currentPassword" name="currentPassword" type="password" required>
          </div>
          <div>
            <label for="newPassword">新密码（至少 6 位）</label>
            <input id="newPassword" name="newPassword" type="password" required>
          </div>
          <div>
            <label for="confirmPassword">确认新密码</label>
            <input id="confirmPassword" name="confirmPassword" type="password" required>
          </div>
        </div>
        <button type="submit">更新密码</button>
      </form>
      <p id="passwordStatus" class="status"></p>
    </section>
  </main>

  <script>
    function setText(id, text) {
      const element = document.getElementById(id);
      if (!element) {
        return;
      }
      element.textContent = text || "";
    }

    const defaultStatusPollIntervalMinutes = 3;
    const allowedStatusPollIntervals = [1, 3, 10, 30, 60];
    let statusPollTimer = 0;
    let otaActionPollTimer = 0;
    const ddnsProviders = ["duckdns"];
    const maxDdnsRecords = 5;
    let ddnsRecordsCache = [];

    function normalizeStatusPollIntervalMinutes(value) {
      const minutes = Number(value);
      if (!Number.isFinite(minutes)) {
        return defaultStatusPollIntervalMinutes;
      }
      const normalized = Math.max(0, Math.floor(minutes));
      if (normalized === 0) {
        return 0;
      }
      return allowedStatusPollIntervals.indexOf(normalized) >= 0
          ? normalized
          : defaultStatusPollIntervalMinutes;
    }

    function statusPollIntervalLabel(minutes) {
      if (minutes === 0) {
        return "手动";
      }
      return "每 " + minutes + " 分钟自动刷新";
    }

    function statusPollIntervalSelectValue(minutes) {
      return minutes === 0 ? "manual" : String(minutes);
    }

    function stopStatusPollTimer() {
      if (statusPollTimer) {
        clearInterval(statusPollTimer);
        statusPollTimer = 0;
      }
    }

    function startOtaActionPolling() {
      if (otaActionPollTimer) {
        return;
      }
      otaActionPollTimer = setInterval(refreshOtaStatus, 2000);
    }

    function stopOtaActionPolling() {
      if (!otaActionPollTimer) {
        return;
      }
      clearInterval(otaActionPollTimer);
      otaActionPollTimer = 0;
    }

    function applyStatusPolling(minutes) {
      const normalizedMinutes = normalizeStatusPollIntervalMinutes(minutes);
      const select = document.getElementById("statusPollIntervalMinutes");
      const refreshButton = document.getElementById("refreshAllButton");

      select.value = statusPollIntervalSelectValue(normalizedMinutes);
      refreshButton.hidden = normalizedMinutes !== 0;

      stopStatusPollTimer();
      if (normalizedMinutes > 0) {
        statusPollTimer = setInterval(refreshAllStatus, normalizedMinutes * 60 * 1000);
      }

      setText("systemPollStatus", "状态轮询：" + statusPollIntervalLabel(normalizedMinutes));
    }

    function escapeHtml(value) {
      return String(value || "")
          .replace(/&/g, "&amp;")
          .replace(/</g, "&lt;")
          .replace(/>/g, "&gt;")
          .replace(/"/g, "&quot;")
          .replace(/'/g, "&#39;");
    }

    function normalizeDdnsProvider(value) {
      const normalized = String(value || "").trim().toLowerCase();
      if (ddnsProviders.indexOf(normalized) >= 0) {
        return normalized;
      }
      return "duckdns";
    }

    function normalizeDdnsIntervalSeconds(value) {
      const interval = Number(value);
      if (!Number.isFinite(interval)) {
        return 300;
      }
      const normalized = Math.floor(interval);
      if (normalized < 30 || normalized > 86400) {
        return 300;
      }
      return normalized;
    }

    function normalizeDdnsRecord(record) {
      return {
        enabled: !!record.enabled,
        provider: normalizeDdnsProvider(record.provider),
        domain: String(record.domain || "").trim(),
        username: String(record.username || "").trim(),
        password: String(record.password || "").trim(),
        updateIntervalSeconds: normalizeDdnsIntervalSeconds(record.updateIntervalSeconds),
        useLocalIp: !!record.useLocalIp
      };
    }

    function createDefaultDdnsRecord() {
      return {
        enabled: true,
        provider: "duckdns",
        domain: "",
        username: "",
        password: "",
        updateIntervalSeconds: 300,
        useLocalIp: false
      };
    }

    function ddnsProviderOptions(selectedProvider) {
      return ddnsProviders
          .map(function (provider) {
            const selected = provider === selectedProvider ? " selected" : "";
            return "<option value=\"" + provider + "\"" + selected + ">" + provider + "</option>";
          })
          .join("");
    }

    function collectDdnsRecordsFromForm() {
      const cards = Array.from(document.querySelectorAll("#ddnsRecords .ddns-record"));
      return cards.map(function (card) {
        const enabledElement = card.querySelector(".ddns-enabled");
        const providerElement = card.querySelector(".ddns-provider");
        const domainElement = card.querySelector(".ddns-domain");
        const usernameElement = card.querySelector(".ddns-username");
        const passwordElement = card.querySelector(".ddns-password");
        const intervalElement = card.querySelector(".ddns-interval");
        const localIpElement = card.querySelector(".ddns-local-ip");
        return normalizeDdnsRecord({
          enabled: enabledElement ? !!enabledElement.checked : false,
          provider: providerElement ? providerElement.value : "duckdns",
          domain: domainElement ? domainElement.value : "",
          username: usernameElement ? usernameElement.value : "",
          password: passwordElement ? passwordElement.value : "",
          updateIntervalSeconds: intervalElement ? intervalElement.value : 300,
          useLocalIp: localIpElement ? !!localIpElement.checked : false
        });
      });
    }

    function renderDdnsRecords(records) {
      ddnsRecordsCache =
          (Array.isArray(records) ? records : []).slice(0, maxDdnsRecords).map(normalizeDdnsRecord);

      const container = document.getElementById("ddnsRecords");
      if (!container) {
        return;
      }

      if (ddnsRecordsCache.length === 0) {
        container.innerHTML = "<p class=\"muted\">暂无 DDNS 记录，点击“新增记录”开始配置。</p>";
        return;
      }

      container.innerHTML = ddnsRecordsCache
                                .map(function (record, index) {
                                  return (
                                      "<div class=\"ddns-record\" data-index=\"" + index + "\">" +
                                      "<div class=\"ddns-record-header\">" +
                                      "<span class=\"ddns-record-title\">记录 #" + (index + 1) + "</span>" +
                                      "<button class=\"secondary ddns-remove-button\" type=\"button\" data-index=\"" + index + "\">删除</button>" +
                                      "</div>" +
                                      "<div class=\"inline-check\">" +
                                      "<input class=\"ddns-enabled\" type=\"checkbox\"" + (record.enabled ? " checked" : "") + ">" +
                                      "<label style=\"margin:0;\">启用此记录</label>" +
                                      "</div>" +
                                      "<div class=\"row\">" +
                                      "<div><label>服务商</label><select class=\"ddns-provider\">" + ddnsProviderOptions(record.provider) + "</select></div>" +
                                      "<div><label>域名/主机</label><input class=\"ddns-domain\" autocomplete=\"off\" value=\"" + escapeHtml(record.domain) + "\" placeholder=\"example.ddns.net\"></div>" +
                                      "<div><label>用户名/Token</label><input class=\"ddns-username\" autocomplete=\"off\" value=\"" + escapeHtml(record.username) + "\"></div>" +
                                      "<div><label>密码/Key</label><input class=\"ddns-password\" type=\"password\" autocomplete=\"off\" value=\"" + escapeHtml(record.password) + "\"></div>" +
                                      "<div><label>更新间隔(秒)</label><input class=\"ddns-interval\" type=\"number\" min=\"30\" max=\"86400\" value=\"" + String(record.updateIntervalSeconds) + "\"></div>" +
                                      "</div>" +
                                      "<div class=\"inline-check\">" +
                                      "<input class=\"ddns-local-ip\" type=\"checkbox\"" + (record.useLocalIp ? " checked" : "") + ">" +
                                      "<label style=\"margin:0;\">使用局域网 IP（仅内网解析场景）</label>" +
                                      "</div>" +
                                      "<p class=\"muted\">运行状态：<span class=\"ddns-record-state\">-</span>；最新 IP：<span class=\"ddns-record-ip\">-</span></p>" +
                                      "</div>");
                                })
                                .join("");

      Array.from(container.querySelectorAll(".ddns-remove-button")).forEach(function (button) {
        button.addEventListener("click", function () {
          const index = Number(this.dataset.index);
          const currentRecords = collectDdnsRecordsFromForm();
          if (Number.isFinite(index) && index >= 0 && index < currentRecords.length) {
            currentRecords.splice(index, 1);
            renderDdnsRecords(currentRecords);
          }
        });
      });
    }

    function addDdnsRecord() {
      const currentRecords = collectDdnsRecordsFromForm();
      if (currentRecords.length >= maxDdnsRecords) {
        setText("ddnsStatus", "最多支持 5 条 DDNS 记录。");
        return;
      }
      currentRecords.push(createDefaultDdnsRecord());
      renderDdnsRecords(currentRecords);
      setText("ddnsStatus", "");
    }

    function ddnsStateLabel(state) {
      if (state === "DISABLED") return "已禁用";
      if (state === "WAIT_CONFIG") return "待配置";
      if (state === "WAIT_WIFI") return "等待 WiFi";
      if (state === "READY") return "就绪";
      if (state === "RUNNING") return "运行中";
      if (state === "UPDATED") return "已更新";
      if (state === "ERROR") return "异常";
      return state || "-";
    }

    function updateDdnsStatus(data) {
      const state = data.state || data.ddnsState || "-";
      const message = data.message || data.ddnsMessage || "";
      const activeRecordCount = Number(
          data.activeRecordCount !== undefined ? data.activeRecordCount : data.ddnsActiveRecordCount);
      const totalUpdateCount = Number(
          data.totalUpdateCount !== undefined ? data.totalUpdateCount : data.ddnsTotalUpdateCount);

      setText("ddnsState", ddnsStateLabel(state));
      setText("ddnsActiveCount", Number.isFinite(activeRecordCount) ? String(activeRecordCount) : "0");
      setText("ddnsUpdateCount", Number.isFinite(totalUpdateCount) ? String(totalUpdateCount) : "0");
      if (message) {
        setText("ddnsStatus", message);
      }

      const recordStates = Array.isArray(data.records) ? data.records : [];
      const cards = Array.from(document.querySelectorAll("#ddnsRecords .ddns-record"));
      cards.forEach(function (card, index) {
        const record = recordStates[index];
        if (!record) {
          return;
        }

        const stateElement = card.querySelector(".ddns-record-state");
        const ipElement = card.querySelector(".ddns-record-ip");
        if (stateElement) {
          stateElement.textContent = ddnsStateLabel(record.state || "-");
        }
        if (ipElement) {
          const ip = record.lastNewIp || record.lastOldIp || "-";
          ipElement.textContent = ip;
        }
      });
    }

    async function api(url, options) {
      const response = await fetch(url, Object.assign({ credentials: "same-origin" }, options || {}));
      const text = await response.text();
      let payload = {};
      if (text) {
        try { payload = JSON.parse(text); } catch (_) { payload = { message: text }; }
      }
      if (!response.ok) {
        throw new Error(payload.error || payload.message || ("HTTP " + response.status));
      }
      return payload;
    }

    function toMessage(code) {
      if (code === "invalid_mac") return "MAC 地址格式错误，请使用 AA:BB:CC:DD:EE:FF。";
      if (code === "missing_fields") return "请完整填写密码字段。";
      if (code === "confirm_not_match") return "两次输入的新密码不一致。";
      if (code === "current_password_incorrect") return "当前密码不正确。";
      if (code === "password_too_short") return "新密码长度不能少于 6 位。";
      if (code === "password_not_changed") return "新密码不能与当前密码相同。";
      if (code === "persist_failed") return "保存失败，请稍后重试。";
      if (code === "unauthorized") return "登录已失效，请重新登录。";
      if (code === "ssid_required") return "SSID 不能为空。";
      if (code === "wifi_not_connected") return "WiFi 未连接，无法执行该操作。";
      if (code === "config_mac_required") return "请先配置目标电脑 MAC 地址。";
      if (code === "config_ip_invalid") return "请先配置正确的目标电脑 IP 地址。";
      if (code === "wol_send_failed") return "发送开机包失败，请检查网络环境。";
      if (code === "udp_begin_failed") return "UDP 通道初始化失败。";
      if (code === "boot_timeout") return "等待主机上线超时。";
      if (code === "already_booting") return "开机流程进行中，请稍候。";
      if (code === "already_on") return "主机已经在线。";
      if (code === "ota_busy") return "OTA 任务执行中，请稍后再试。";
      if (code === "ota_too_frequent") return "手动 OTA 操作触发过于频繁，请稍后重试。";
      if (code === "ota_check_too_frequent") return "手动检测触发过于频繁，请稍后重试。";
      if (code === "ota_upgrade_too_frequent") return "手动升级触发过于频繁，请稍后重试。";
      if (code === "ota_config_incomplete") return "OTA 配置不完整，请先填写巴法云 UID 和 Topic。";
      if (code === "ota_no_update") return "未发布可用新固件。";
      if (code === "ota_lookup_http_failed") return "查询 OTA 失败，请检查网络。";
      if (code === "ota_lookup_http_status") return "OTA 接口返回异常状态码。";
      if (code === "ota_lookup_parse_failed") return "解析 OTA 响应失败。";
      if (code === "ota_lookup_rejected") return "OTA 拒绝了请求。";
      if (code === "ota_version_invalid") return "OTA 返回的版本号无效。";
      if (code === "ota_url_missing") return "OTA 响应缺少固件地址。";
      if (code === "ota_update_failed") return "固件更新失败，请查看设备日志。";
      return code || "请求失败";
    }

    function stateLabel(state) {
      if (state === "BOOTING") return "开机中";
      if (state === "ON") return "已开机";
      if (state === "FAILED") return "失败";
      return "待机";
    }

    function updatePowerStatus(data) {
      const state = data.state || data.powerState || "IDLE";
      const message = data.message || data.powerMessage || "";
      const busy = !!data.busy || !!data.powerBusy;

      setText("powerState", stateLabel(state));
      setText("powerStatus", message);

      const button = document.getElementById("powerButton");
      button.disabled = busy;
      button.textContent = busy ? "开机中..." : "执行开机";
    }

    function bemfaStateLabel(state) {
      if (state === "DISABLED") return "已禁用";
      if (state === "WAIT_CONFIG") return "待配置";
      if (state === "WAIT_WIFI") return "等待 WiFi";
      if (state === "READY") return "准备连接";
      if (state === "CONNECTING") return "连接中";
      if (state === "SUSPENDED") return "已暂停";
      if (state === "ONLINE") return "在线";
      if (state === "OFFLINE") return "离线";
      if (state === "ERROR") return "异常";
      return state || "-";
    }

    function bemfaMessageLabel(message) {
      if (!message) return "";
      if (message === "Disconnected on logout.") return "退出断开";
      if (message === "Bemfa suspended after logout.") return "已暂停";
      if (message === "Waiting to reconnect.") return "等待重连";
      return message;
    }

    function updateBemfaStatus(data) {
      const state = data.state || data.bemfaState || "-";
      const connected = data.connected !== undefined ? !!data.connected : !!data.bemfaConnected;
      const message = data.message || data.bemfaMessage || "";
      const subscribeTopic = data.subscribeTopic || ((data.bemfaTopic || "") ? ((data.bemfaTopic || "") + "/set") : "");
      const publishTopic = data.publishTopic || ((data.bemfaTopic || "") ? ((data.bemfaTopic || "") + "/up") : "");

      setText("bemfaState", bemfaStateLabel(state));
      setText("bemfaConnected", connected ? "已连接" : "未连接");
      setText("bemfaStatus", bemfaMessageLabel(message));
      if (subscribeTopic || publishTopic) {
        setText("bemfaTopics", "订阅: " + (subscribeTopic || "-") + " | 上报: " + (publishTopic || "-"));
      } else {
        setText("bemfaTopics", "-");
      }
    }

    function otaStateLabel(state) {
      if (state === "WAIT_CONFIG") return "配置不完整";
      if (state === "READY") return "可手动升级";
      if (state === "QUEUED") return "已排队";
      if (state === "CHECKING") return "检测中";
      if (state === "UPDATE_AVAILABLE") return "发现新版本";
      if (state === "DOWNLOADING") return "升级中";
      if (state === "UPDATED") return "升级成功";
      if (state === "NO_UPDATE") return "无新版本";
      if (state === "FAILED") return "失败";
      return state || "-";
    }
    function otaMessageLabel(message) {
      if (!message) return "";
      if (message === "OTA requires Bemfa UID and Topic.") return "OTA 配置不完整，请先填写巴法云 UID 和主题。";
      if (message === "Manual OTA is ready.") return "可手动升级。";
      if (message === "Auto OTA check is enabled.") return "已启用 OTA 自动检测。";
      if (message === "Auto OTA settings updated.") return "OTA 自动检测配置已更新。";
      if (message === "WiFi is disconnected.") return "WiFi 未连接。";
      if (message === "Checking firmware metadata from Bemfa.") return "正在查询巴法云固件信息。";
      if (message === "Downloading and applying firmware.") return "正在下载并更新固件。";
      if (message === "Auto OTA request queued.") return "已加入自动 OTA 升级检测队列。";
      if (message === "Manual OTA request queued.") return "已加入手动 OTA 升级检测队列。";
      if (message === "Auto OTA check request queued.") return "已加入自动 OTA 检测队列。";
      if (message === "Manual OTA check request queued.") return "已加入手动 OTA 检测队列。";
      if (message === "Auto OTA upgrade request queued.") return "已加入自动 OTA 升级队列。";
      if (message === "Manual OTA upgrade request queued.") return "已加入手动 OTA 升级队列。";
      if (message === "No OTA package is published on Bemfa.") return "巴法云未发布可用新固件。";
      if (message === "OTA package found on Bemfa.") return "检测到巴法云可用固件。";
      if (message === "Firmware update completed, rebooting.") return "固件升级完成，设备即将重启。";
      if (message === "No new firmware to apply.") return "没有可应用的新固件。";
      if (message === "Failed to parse Bemfa OTA response code.") return "解析巴法云 OTA 响应失败。";
      if (message === "Failed to open Bemfa OTA endpoint.") return "无法连接巴法云 OTA 接口。";
      if (message === "Bemfa OTA response does not contain firmware URL.") return "巴法云 OTA 响应缺少固件下载地址。";
      if (message.indexOf("New firmware available. Local=") === 0 ||
          message.indexOf("Firmware package available. Local=") === 0) {
        return "检测到新固件：" +
            message.replace("New firmware available. ", "")
                   .replace("Firmware package available. ", "")
                   .replace("Local=", "本地=")
                   .replace("remote=", "远端=");
      }
      if (message.indexOf("No newer firmware version. Local=") === 0 ||
          message.indexOf("No firmware version change. Local=") === 0) {
        return "当前已是最新版本：" +
            message.replace("No newer firmware version. ", "")
                   .replace("No firmware version change. ", "")
                   .replace("Local=", "本地=")
                   .replace("remote=", "远端=");
      }
      if (message.indexOf("Downloading firmware (") === 0) {
        const start = message.indexOf("(");
        const end = message.indexOf(")", start + 1);
        const percent = (start >= 0 && end > start) ? message.substring(start + 1, end) : "";
        return percent ? ("正在下载固件（" + percent + "）") : "正在下载固件...";
      }
      if (message.indexOf("Firmware update failed: ") === 0) {
        return "固件升级失败：" + message.replace("Firmware update failed: ", "");
      }
      if (message.indexOf("Bemfa OTA request failed, code=") === 0) {
        return "查询巴法云 OTA 失败，错误码：" + message.replace("Bemfa OTA request failed, code=", "");
      }
      if (message.indexOf("Bemfa OTA rejected request, code=") === 0) {
        return "巴法云 OTA 拒绝请求，错误码：" + message.replace("Bemfa OTA rejected request, code=", "");
      }
      if (message.indexOf("Bemfa OTA HTTP status is ") === 0) {
        return "巴法云 OTA 接口状态异常：" + message.replace("Bemfa OTA HTTP status is ", "");
      }
      return message;
    }
    function updateOtaStatus(data) {
      const state = data.state || "-";
      const message = data.message || "";
      const error = data.error || "";
      const targetVersion = data.targetVersion || "";
      const targetTag = data.targetTag || "";
      const progressPercent = Math.max(0, Math.min(100, Number(data.progressPercent) || 0));
      const progressBytes = Number(data.progressBytes) || 0;
      const progressTotalBytes = Number(data.progressTotalBytes) || 0;
      const busy = !!data.busy || !!data.pending;
      const updateAvailable = !!data.updateAvailable;

      setText("otaState", otaStateLabel(state));
      if (progressTotalBytes > 0) {
        setText("otaProgress", String(progressPercent) + "% (" + formatBytes(progressBytes) + " / " + formatBytes(progressTotalBytes) + ")");
      } else {
        setText("otaProgress", String(progressPercent) + "%");
      }
      setText(
          "otaTargetVersion",
          (targetVersion || targetTag) ? ((targetVersion || "-") + (targetTag ? (" (" + targetTag + ")") : "")) : "-");
      setText("otaStatus", error ? toMessage(error) : otaMessageLabel(message));

      if (busy) {
        startOtaActionPolling();
      } else {
        stopOtaActionPolling();
      }

      const checkButton = document.getElementById("otaCheckButton");
      const upgradeButton = document.getElementById("otaUpgradeButton");
      checkButton.disabled = busy;
      checkButton.textContent = busy ? "处理中..." : "手动检测新版本";

      upgradeButton.hidden = !updateAvailable;
      upgradeButton.disabled = busy;
      upgradeButton.textContent = busy ? "升级中..." : "升级固件";
    }
    function formatBytes(value) {
      const bytes = Number(value);
      if (!Number.isFinite(bytes) || bytes < 0) {
        return "-";
      }
      if (bytes < 1024) {
        return Math.round(bytes) + " B";
      }

      const units = ["KB", "MB", "GB"];
      let size = bytes / 1024;
      let unitIndex = 0;
      while (size >= 1024 && unitIndex < (units.length - 1)) {
        size /= 1024;
        unitIndex += 1;
      }
      const precision = size >= 100 ? 0 : (size >= 10 ? 1 : 2);
      return size.toFixed(precision) + " " + units[unitIndex];
    }

    function formatUptime(secondsValue) {
      const seconds = Math.max(0, Number(secondsValue) || 0);
      const days = Math.floor(seconds / 86400);
      const hours = Math.floor((seconds % 86400) / 3600);
      const minutes = Math.floor((seconds % 3600) / 60);
      const secs = Math.floor(seconds % 60);
      const hhmmss =
          String(hours).padStart(2, "0") + ":" + String(minutes).padStart(2, "0") + ":" + String(secs).padStart(2, "0");
      if (days > 0) {
        return days + "天 " + hhmmss;
      }
      return hhmmss;
    }

    function updateSystemInfo(data) {
      setText("espUptime", formatUptime(data.uptimeSeconds));
      setText("espHeap", formatBytes(data.heapFree) + " / " + formatBytes(data.heapTotal));
      setText("espHeapMin", formatBytes(data.heapMinFree));
      setText("espHeapMaxAlloc", formatBytes(data.heapMaxAlloc));
      setText("espFlash", formatBytes(data.sketchUsed) + " / " + formatBytes(data.flashTotal));
      setText("espFlashFree", formatBytes(data.flashFree));

      const psramTotal = Number(data.psramTotal) || 0;
      if (psramTotal > 0) {
        setText("espPsram", formatBytes(data.psramFree) + " / " + formatBytes(psramTotal));
      } else {
        setText("espPsram", "不支持");
      }

      setText("espInfoStatus", "最后更新：" + new Date().toLocaleTimeString());
    }

    let wifiScanRequesting = false;
    let wifiScanInProgress = false;
    let wifiScanTimer = 0;

    function updateScanButtonState() {
      const button = document.getElementById("scanButton");
      button.disabled = wifiScanRequesting || wifiScanInProgress;
      if (wifiScanRequesting) {
        button.textContent = "请求中...";
      } else if (wifiScanInProgress) {
        button.textContent = "扫描中...";
      } else {
        button.textContent = "扫描 WiFi";
      }
    }

    function scheduleWifiScan(delayMs) {
      if (wifiScanTimer) {
        clearTimeout(wifiScanTimer);
      }
      wifiScanTimer = setTimeout(function () {
        wifiScanTimer = 0;
        scanWifi();
      }, delayMs);
    }

    function renderWifiOptions(networks) {
      const select = document.getElementById("wifiSelect");
      const ssidInput = document.getElementById("wifiSsid");
      const previousSelection = ssidInput.value || select.value || "";
      select.innerHTML = "";

      if (networks.length === 0) {
        const emptyOption = document.createElement("option");
        emptyOption.value = "";
        emptyOption.textContent = "未扫描到网络";
        select.appendChild(emptyOption);
        return 0;
      }

      networks.forEach(function (network) {
        const option = document.createElement("option");
        option.value = network.ssid;
        option.textContent =
            network.ssid + " (" + network.rssi + " dBm" + (network.secured ? "，加密" : "，开放") + ")";
        select.appendChild(option);
      });

      if (previousSelection) {
        const options = Array.from(select.options);
        const matched = options.find(function (item) { return item.value === previousSelection; });
        if (matched) {
          select.value = previousSelection;
        }
      }

      ssidInput.value = select.value || previousSelection;
      return networks.length;
    }

    async function loadConfig() {
      const data = await api("/api/config");
      document.getElementById("computerIp").value = data.computerIp || "";
      document.getElementById("computerMac").value = data.computerMac || "";
      document.getElementById("computerPort").value = data.computerPort || "";

      document.getElementById("bemfaEnabled").checked = !!data.bemfaEnabled;
      document.getElementById("bemfaTopic").value = data.bemfaTopic || "";
      document.getElementById("bemfaUid").value = data.bemfaUid || "";
      document.getElementById("bemfaKey").value = data.bemfaKey || "";
      document.getElementById("bemfaHost").value = data.bemfaHost || "bemfa.com";
      document.getElementById("bemfaPort").value = data.bemfaPort || 9501;
      document.getElementById("ddnsEnabled").checked = !!data.ddnsEnabled;
      renderDdnsRecords(Array.isArray(data.ddnsRecords) ? data.ddnsRecords : []);
      setText("otaCurrentVersion", data.otaCurrentVersion || "-");

      updatePowerStatus(data);
      updateBemfaStatus(data);
      updateDdnsStatus(data);
      applyStatusPolling(data.statusPollIntervalMinutes);

      if (data.wifiConnected) {
        setText("wifiStatus", "已连接：" + (data.wifiSsid || "") + "，IP：" + (data.wifiIp || ""));
      } else {
        setText("wifiStatus", "未连接 WiFi");
      }
    }

    async function refreshPowerStatus() {
      try {
        const data = await api("/api/power/status");
        updatePowerStatus(data);
      } catch (error) {
        setText("powerStatus", toMessage(error.message));
      }
    }

    async function refreshSystemInfo() {
      try {
        const data = await api("/api/system/info");
        updateSystemInfo(data);
      } catch (error) {
        setText("espInfoStatus", toMessage(error.message));
      }
    }

    async function refreshBemfaStatus() {
      try {
        const data = await api("/api/bemfa/status");
        updateBemfaStatus(data);
      } catch (error) {
        setText("bemfaStatus", toMessage(error.message));
      }
    }

    async function refreshDdnsStatus() {
      try {
        const data = await api("/api/ddns/status");
        updateDdnsStatus(data);
      } catch (error) {
        setText("ddnsStatus", toMessage(error.message));
      }
    }

    async function refreshOtaStatus() {
      try {
        const data = await api("/api/ota/status");
        updateOtaStatus(data);
      } catch (error) {
        setText("otaStatus", toMessage(error.message));
      }
    }

    async function refreshAllStatus() {
      await Promise.all([refreshPowerStatus(),
                         refreshBemfaStatus(),
                         refreshDdnsStatus(),
                         refreshOtaStatus(),
                         refreshSystemInfo()]);
    }

    async function powerOn() {
      try {
        const data = await api("/api/power/on", { method: "POST" });
        updatePowerStatus(data);
        if (data.error) {
          setText("powerStatus", toMessage(data.error));
        }
      } catch (error) {
        setText("powerStatus", toMessage(error.message));
      }
    }

    async function triggerManualOtaCheck() {
      const button = document.getElementById("otaCheckButton");
      button.disabled = true;
      button.textContent = "提交中...";

      try {
        const data = await api("/api/ota/check", { method: "POST" });
        setText("otaStatus", otaMessageLabel(data.message) || "已提交 OTA 检测请求。");
        startOtaActionPolling();
        await refreshOtaStatus();
      } catch (error) {
        setText("otaStatus", toMessage(error.message));
        await refreshOtaStatus();
      }
    }

    async function triggerManualOtaUpgrade() {
      const button = document.getElementById("otaUpgradeButton");
      button.disabled = true;
      button.textContent = "提交中...";

      try {
        const data = await api("/api/ota/upgrade", { method: "POST" });
        setText("otaStatus", otaMessageLabel(data.message) || "已提交 OTA 升级请求。");
        startOtaActionPolling();
        await refreshOtaStatus();
      } catch (error) {
        setText("otaStatus", toMessage(error.message));
        await refreshOtaStatus();
      }
    }
    async function scanWifi() {
      if (wifiScanRequesting) {
        return;
      }

      if (wifiScanTimer) {
        clearTimeout(wifiScanTimer);
        wifiScanTimer = 0;
      }

      wifiScanRequesting = true;
      updateScanButtonState();
      try {
        const data = await api("/api/wifi/scan");
        const networks = Array.isArray(data.networks) ? data.networks : [];
        const networkCount = renderWifiOptions(networks);
        const ageMs = Number(data.ageMs) || 0;
        const ageSeconds = Math.floor(ageMs / 1000);

        wifiScanInProgress = !!data.scanInProgress;
        updateScanButtonState();

        if (wifiScanInProgress) {
          if (networkCount > 0 && data.fromCache) {
            setText("wifiStatus", "正在刷新附近 WiFi，先显示缓存结果（" + ageSeconds + " 秒前）。");
          } else {
            setText("wifiStatus", "正在扫描附近 WiFi，请稍候...");
          }
          scheduleWifiScan(1200);
        } else if (networkCount > 0) {
          if (data.fromCache && ageSeconds > 0) {
            setText("wifiStatus", "已加载最近扫描结果（" + ageSeconds + " 秒前），共 " + networkCount + " 个网络。");
          } else {
            setText("wifiStatus", "扫描完成，共找到 " + networkCount + " 个网络。");
          }
        } else {
          setText("wifiStatus", "未扫描到附近 WiFi。");
        }
      } catch (error) {
        wifiScanInProgress = false;
        setText("wifiStatus", toMessage(error.message));
      } finally {
        wifiScanRequesting = false;
        updateScanButtonState();
      }
    }

    async function connectWifi() {
      const params = new URLSearchParams();
      params.set("ssid", (document.getElementById("wifiSsid").value || "").trim());
      params.set("password", document.getElementById("wifiPassword").value || "");
      try {
        const data = await api("/api/wifi/connect", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: params.toString()
        });
        setText("wifiStatus", (data.message || "连接成功") + (data.ip ? ("，IP：" + data.ip) : ""));
      } catch (error) {
        setText("wifiStatus", toMessage(error.message));
      }
    }

    async function saveConfig(event) {
      event.preventDefault();
      const params = new URLSearchParams(new FormData(event.target));
      try {
        await api("/api/config", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: params.toString()
        });
        setText("configStatus", "计算机配置已保存。");
      } catch (error) {
        setText("configStatus", toMessage(error.message));
      }
    }

    async function saveBemfaConfig(event) {
      event.preventDefault();
      const params = new URLSearchParams();
      params.set("bemfaEnabled", document.getElementById("bemfaEnabled").checked ? "1" : "0");
      params.set("bemfaTopic", (document.getElementById("bemfaTopic").value || "").trim());
      params.set("bemfaUid", (document.getElementById("bemfaUid").value || "").trim());
      params.set("bemfaKey", (document.getElementById("bemfaKey").value || "").trim());
      params.set("bemfaHost", (document.getElementById("bemfaHost").value || "").trim());
      params.set("bemfaPort", (document.getElementById("bemfaPort").value || "").trim());

      try {
        await api("/api/config", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: params.toString()
        });
        setText("bemfaStatus", "巴法云配置已保存。");
        await refreshBemfaStatus();
      } catch (error) {
        setText("bemfaStatus", toMessage(error.message));
      }
    }

    async function saveDdnsConfig(event) {
      event.preventDefault();

      const records = collectDdnsRecordsFromForm().slice(0, maxDdnsRecords);
      const params = new URLSearchParams();
      params.set("ddnsEnabled", document.getElementById("ddnsEnabled").checked ? "1" : "0");
      params.set("ddnsRecordCount", String(records.length));

      records.forEach(function (record, index) {
        params.set("ddns" + String(index) + "Enabled", record.enabled ? "1" : "0");
        params.set("ddns" + String(index) + "Provider", normalizeDdnsProvider(record.provider));
        params.set("ddns" + String(index) + "Domain", record.domain || "");
        params.set("ddns" + String(index) + "Username", record.username || "");
        params.set("ddns" + String(index) + "Password", record.password || "");
        params.set(
            "ddns" + String(index) + "IntervalSeconds",
            String(normalizeDdnsIntervalSeconds(record.updateIntervalSeconds)));
        params.set("ddns" + String(index) + "UseLocalIp", record.useLocalIp ? "1" : "0");
      });

      try {
        await api("/api/config", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: params.toString()
        });
        await loadConfig();
        await refreshDdnsStatus();
        setText("ddnsStatus", "DDNS 配置已保存。");
      } catch (error) {
        setText("ddnsStatus", toMessage(error.message));
      }
    }

    async function saveSystemConfig(event) {
      event.preventDefault();
      const params = new URLSearchParams();
      params.set(
          "statusPollIntervalMinutes",
          document.getElementById("statusPollIntervalMinutes").value || String(defaultStatusPollIntervalMinutes));

      try {
        await api("/api/config", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: params.toString()
        });
        await loadConfig();
        setText("systemConfigStatus", "系统配置已保存。");
      } catch (error) {
        setText("systemConfigStatus", toMessage(error.message));
      }
    }

    async function refreshAllStatusByButton() {
      const button = document.getElementById("refreshAllButton");
      const originalLabel = button.textContent;
      button.disabled = true;
      button.textContent = "刷新中...";
      try {
        await refreshAllStatus();
        setText("systemConfigStatus", "状态已刷新。");
      } finally {
        button.disabled = false;
        button.textContent = originalLabel;
      }
    }

    async function savePassword(event) {
      event.preventDefault();
      const params = new URLSearchParams(new FormData(event.target));
      try {
        const data = await api("/api/auth/password", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: params.toString()
        });
        setText("passwordStatus", "密码已更新，正在跳转到登录页...");
        event.target.reset();
        if (data.relogin) {
          setTimeout(function () {
            window.location.href = "/login";
          }, 1000);
        }
      } catch (error) {
        setText("passwordStatus", toMessage(error.message));
      }
    }

    document.getElementById("wifiSelect").addEventListener("change", function () {
      document.getElementById("wifiSsid").value = this.value;
    });
    document.getElementById("scanButton").addEventListener("click", scanWifi);
    document.getElementById("connectButton").addEventListener("click", connectWifi);
    document.getElementById("powerButton").addEventListener("click", powerOn);
    document.getElementById("otaCheckButton").addEventListener("click", triggerManualOtaCheck);
    document.getElementById("otaUpgradeButton").addEventListener("click", triggerManualOtaUpgrade);
    document.getElementById("configForm").addEventListener("submit", saveConfig);
    document.getElementById("bemfaForm").addEventListener("submit", saveBemfaConfig);
    document.getElementById("ddnsAddRecordButton").addEventListener("click", addDdnsRecord);
    document.getElementById("ddnsForm").addEventListener("submit", saveDdnsConfig);
    document.getElementById("systemForm").addEventListener("submit", saveSystemConfig);
    document.getElementById("refreshAllButton").addEventListener("click", refreshAllStatusByButton);
    document.getElementById("passwordForm").addEventListener("submit", savePassword);

    window.addEventListener("DOMContentLoaded", async function () {
      updateScanButtonState();
      renderDdnsRecords([]);
      try {
        await loadConfig();
      } catch (error) {
        setText("configStatus", toMessage(error.message));
      }
      await refreshAllStatus();
      await scanWifi();
    });
  </script>
</body>
</html>
)HTML";
}

String WebPortal::jsonEscape(const String& value) {
  String escaped;
  escaped.reserve(value.length() + 8);

  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value.charAt(i);
    switch (c) {
      case '\"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default: {
        if (static_cast<unsigned char>(c) < 0x20) {
          char buffer[7];
          std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned char>(c));
          escaped += buffer;
        } else {
          escaped += c;
        }
        break;
      }
    }
  }

  return escaped;
}

bool WebPortal::parseBoolValue(const String& value, bool defaultValue) {
  String normalized = value;
  normalized.trim();
  normalized.toLowerCase();

  if (normalized == "1" || normalized == "true" || normalized == "on" || normalized == "yes") {
    return true;
  }
  if (normalized == "0" || normalized == "false" || normalized == "off" || normalized == "no") {
    return false;
  }
  return defaultValue;
}
