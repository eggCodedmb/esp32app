#include "WebPortal.h"

#include <cctype>
#include <cstring>
#include <cstdio>
#include <ESP.h>

namespace {
constexpr const char* kProviderId = "aliyun";
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
  String normalized = provider;
  normalized.trim();
  normalized.toLowerCase();
  if (normalized == kProviderId) {
    return normalized;
  }
  return String(kProviderId);
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
                     TimeService& timeService,
                     FirmwareUpgradeService& firmwareUpgradeService)
    : _server(port),
      _authService(authService),
      _wifiService(wifiService),
      _configStore(configStore),
      _powerOnService(powerOnService),
      _bemfaService(bemfaService),
      _ddnsService(ddnsService),
      _timeService(timeService),
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
    const char* page = dashboardPage();
    request->send(request->beginResponse(200,
                                         "text/html; charset=utf-8",
                                         reinterpret_cast<const uint8_t*>(page),
                                         std::strlen(page)));
  });

  _server.on("/login", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (_authService.isAuthorized(request)) {
      AsyncWebServerResponse* response = request->beginResponse(302);
      response->addHeader("Location", "/");
      request->send(response);
      return;
    }
    String message = "";
    if (request->hasParam("message", false)) {
      String msgParam = request->getParam("message", false)->value();
      if (msgParam == "upgrade_success") {
        message = "固件升级成功，请重新登录";
      }
    }
    request->send(200, "text/html; charset=utf-8", loginPage(message));
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
    body += "\"flashFree\":" + String(flashFree) + ",";
    body += "\"systemTime\":\"" + jsonEscape(_timeService.getFormattedTime()) + "\",";
    body += "\"systemTimeUnix\":" + String(_timeService.getUnixTime());
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
