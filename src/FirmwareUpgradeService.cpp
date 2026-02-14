#include "FirmwareUpgradeService.h"

#include <cctype>
#include <cstdio>

#include <ESP.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClient.h>

namespace {
constexpr long kLookupSuccessCode = 5723007;
constexpr long kLookupNoPackageCode = 5724009;
constexpr const char* kLookupApiHost = "http://api.bemfa.com";
constexpr const char* kFixedOtaUid = "d5bea3ae08914dc3b1e146b9e1f3b9ca";
constexpr const char* kFixedOtaTopic = "OTA";
}  // namespace

void FirmwareUpgradeService::begin() {
  _uid = kFixedOtaUid;
  _topic = kFixedOtaTopic;
  _installedVersionCode = loadInstalledVersionCode();
  _begun = true;
}

void FirmwareUpgradeService::updateConfig(const BemfaConfig& config) {
  (void)config;
  _uid = kFixedOtaUid;
  _topic = kFixedOtaTopic;

  if (!isConfigValid()) {
    setState("WAIT_CONFIG", "OTA requires Bemfa UID and Topic.");
    _lastError = "ota_config_incomplete";
    return;
  }

  if (_busy || _pendingRequest) {
    return;
  }

  setState("READY", "Manual OTA is ready.");
  _lastError = "";
}

void FirmwareUpgradeService::updateAutoCheckConfig(bool enabled, uint16_t intervalMinutes) {
  (void)enabled;
  const uint16_t normalizedInterval = normalizeAutoCheckIntervalMinutes(intervalMinutes);
  const bool changed = _autoCheckEnabled || _autoCheckIntervalMinutes != normalizedInterval;
  _autoCheckEnabled = false;
  _autoCheckIntervalMinutes = normalizedInterval;

  if (!changed) {
    return;
  }

  if (!isConfigValid()) {
    setState("WAIT_CONFIG", "OTA requires Bemfa UID and Topic.");
    _lastError = "ota_config_incomplete";
    return;
  }

  if (!_busy && !_pendingRequest && isConfigValid()) {
    setState("READY", "Manual OTA is ready.");
    _lastError = "";
  }
}

void FirmwareUpgradeService::setEventCallback(FirmwareUpgradeEventCallback callback,
                                              void* context) {
  _eventCallback = callback;
  _eventContext = context;
}

void FirmwareUpgradeService::tick(bool wifiConnected) {
  _wifiConnected = wifiConnected;

  if (!_begun) {
    begin();
  }

  if (!_busy && !_pendingRequest && _autoCheckEnabled && isConfigValid() && _wifiConnected) {
    const uint32_t now = millis();
    const uint32_t intervalMs = autoCheckIntervalMs();
    if (_lastAutoCheckAtMs == 0 || (now - _lastAutoCheckAtMs) >= intervalMs) {
      queueRequest(RequestAction::CheckOnly, TriggerSource::Auto, true, nullptr);
    }
  }

  if (!_pendingRequest || _busy) {
    return;
  }

  _pendingRequest = false;
  _busy = true;
  _activeTrigger = _pendingTrigger;
  _pendingTrigger = TriggerSource::None;
  const RequestAction action = _pendingAction;
  _pendingAction = RequestAction::None;
  _lastError = "";
  _lastStartAtMs = millis();
  _lastAutoCheckAtMs = _lastStartAtMs;
  resetProgress();
  notifyStatusChanged(true);

  if (action == RequestAction::None) {
    _busy = false;
    _lastFinishAtMs = millis();
    _lastError = "ota_internal_error";
    _activeTrigger = TriggerSource::None;
    setState("FAILED", "Invalid OTA request action.");
    return;
  }

  if (!_wifiConnected) {
    _busy = false;
    _lastFinishAtMs = millis();
    _lastError = "wifi_not_connected";
    _activeTrigger = TriggerSource::None;
    setState("FAILED", "WiFi is disconnected.");
    return;
  }

  if (!isConfigValid()) {
    _busy = false;
    _lastFinishAtMs = millis();
    _lastError = "ota_config_incomplete";
    _activeTrigger = TriggerSource::None;
    setState("WAIT_CONFIG", "OTA requires Bemfa UID and Topic.");
    return;
  }

  FirmwarePackageInfo packageInfo;
  bool packageReady = false;
  String errorCode;
  String detailMessage;

  if (action == RequestAction::Upgrade && _hasCachedPackage && _updateAvailable &&
      isNewerThanInstalledVersion(_cachedPackageInfo)) {
    packageInfo = _cachedPackageInfo;
    packageReady = true;
  } else {
    setState("CHECKING", "Checking firmware metadata from Bemfa.");
    packageReady = queryPackageFromBemfa(&packageInfo, &errorCode, &detailMessage);
    _lastCheckAtMs = millis();
  }

  if (!packageReady) {
    _busy = false;
    _lastFinishAtMs = millis();
    _lastError = errorCode;
    _activeTrigger = TriggerSource::None;
    if (errorCode == "ota_no_update") {
      _updateAvailable = false;
      _hasCachedPackage = false;
      setState("NO_UPDATE", detailMessage);
    } else {
      setState("FAILED", detailMessage);
    }
    return;
  }

  _targetVersion = packageInfo.version;
  _targetVersionCode = packageInfo.versionCode;
  _targetTag = packageInfo.tag;

  if (!isNewerThanInstalledVersion(packageInfo)) {
    _busy = false;
    _lastFinishAtMs = millis();
    _lastError = "ota_no_update";
    _activeTrigger = TriggerSource::None;
    _updateAvailable = false;
    _hasCachedPackage = false;
    setState("NO_UPDATE",
             "No newer firmware version. Local=" + String(_installedVersionCode) +
                 ", remote=" + String(packageInfo.versionCode) + ".");
    return;
  }

  _cachedPackageInfo = packageInfo;
  _hasCachedPackage = true;
  _updateAvailable = true;

  if (action == RequestAction::CheckOnly) {
    _busy = false;
    _lastFinishAtMs = millis();
    _lastError = "";
    _activeTrigger = TriggerSource::None;
    setState("UPDATE_AVAILABLE",
             "New firmware available. Local=" + String(_installedVersionCode) +
                 ", remote=" + String(packageInfo.versionCode) + ".");
    return;
  }

  setState("DOWNLOADING", "Downloading and applying firmware.");

  const bool updated = executeUpgrade(packageInfo, &errorCode, &detailMessage);
  _busy = false;
  _lastFinishAtMs = millis();
  _activeTrigger = TriggerSource::None;
  if (!updated) {
    _lastError = errorCode;
    if (errorCode == "ota_no_update") {
      _updateAvailable = false;
      _hasCachedPackage = false;
      setState("NO_UPDATE", detailMessage);
    } else {
      setState("FAILED", detailMessage);
    }
    return;
  }

  _lastError = "";
  _progressPercent = 100;
  _progressBytes = _progressTotalBytes;
  _lastProgressAtMs = millis();

  if (packageInfo.versionCode >= 0) {
    _installedVersionCode = packageInfo.versionCode;
    saveInstalledVersionCode(packageInfo.versionCode);
  }
  _updateAvailable = false;
  _hasCachedPackage = false;

  setState("UPDATED", detailMessage);
#ifndef UNIT_TEST
  delay(200);
  ESP.restart();
#endif
}

bool FirmwareUpgradeService::requestManualCheck(bool wifiConnected, String* errorCode) {
  const bool accepted =
      queueRequest(RequestAction::CheckOnly, TriggerSource::Manual, wifiConnected, errorCode);
  if (accepted) {
    _lastManualRequestAtMs = millis();
  }
  return accepted;
}

bool FirmwareUpgradeService::requestManualUpgrade(bool wifiConnected, String* errorCode) {
  const bool accepted =
      queueRequest(RequestAction::Upgrade, TriggerSource::Manual, wifiConnected, errorCode);
  if (accepted) {
    _lastManualRequestAtMs = millis();
  }
  return accepted;
}

FirmwareUpgradeStatus FirmwareUpgradeService::getStatus() const {
  FirmwareUpgradeStatus status;
  status.configured = isConfigValid();
  status.wifiConnected = _wifiConnected;
  status.busy = _busy;
  status.pending = _pendingRequest;
  status.updateAvailable = _updateAvailable;
  status.autoCheckEnabled = _autoCheckEnabled;
  status.autoCheckIntervalMinutes = _autoCheckIntervalMinutes;
  status.progressPercent = _progressPercent;
  status.progressBytes = _progressBytes;
  status.progressTotalBytes = _progressTotalBytes;
  status.trigger = triggerToText(_busy ? _activeTrigger : _lastTrigger);
  status.state = _state;
  status.message = _message;
  status.lastError = _lastError;
  status.currentVersion = _installedVersionCode >= 0 ? String(_installedVersionCode) : "-";
  status.targetVersion = _targetVersion;
  status.targetTag = _targetTag;
  status.lastAutoCheckAtMs = _lastAutoCheckAtMs;
  status.lastProgressAtMs = _lastProgressAtMs;
  status.lastCheckAtMs = _lastCheckAtMs;
  status.lastStartAtMs = _lastStartAtMs;
  status.lastFinishAtMs = _lastFinishAtMs;

  if (_autoCheckEnabled && isConfigValid() && !_busy && _wifiConnected) {
    const uint32_t intervalMs = autoCheckIntervalMs();
    const uint32_t now = millis();
    if (_lastAutoCheckAtMs == 0 || (now - _lastAutoCheckAtMs) >= intervalMs) {
      status.nextAutoCheckInMs = 0;
    } else {
      status.nextAutoCheckInMs = intervalMs - (now - _lastAutoCheckAtMs);
    }
  }
  return status;
}

bool FirmwareUpgradeService::isValidAutoCheckIntervalMinutes(uint16_t intervalMinutes) const {
  return intervalMinutes == 5 || intervalMinutes == 10 || intervalMinutes == 30 ||
         intervalMinutes == 60 || intervalMinutes == 180 || intervalMinutes == 360 ||
         intervalMinutes == 720 || intervalMinutes == 1440;
}

uint16_t FirmwareUpgradeService::normalizeAutoCheckIntervalMinutes(uint16_t intervalMinutes) const {
  if (!isValidAutoCheckIntervalMinutes(intervalMinutes)) {
    return kDefaultAutoCheckIntervalMinutes;
  }
  return intervalMinutes;
}

bool FirmwareUpgradeService::isConfigValid() const {
  return !_uid.isEmpty() && !_topic.isEmpty();
}

bool FirmwareUpgradeService::queueRequest(RequestAction action,
                                          TriggerSource source,
                                          bool wifiConnected,
                                          String* errorCode) {
  if (_busy || _pendingRequest) {
    if (errorCode != nullptr) {
      *errorCode = "ota_busy";
    }
    return false;
  }

  if (!isConfigValid()) {
    if (errorCode != nullptr) {
      *errorCode = "ota_config_incomplete";
    }
    setState("WAIT_CONFIG", "OTA requires Bemfa UID and Topic.");
    _lastError = "ota_config_incomplete";
    return false;
  }

  if (!wifiConnected) {
    if (errorCode != nullptr) {
      *errorCode = "wifi_not_connected";
    }
    setState("FAILED", "WiFi is disconnected.");
    _lastError = "wifi_not_connected";
    return false;
  }

  if (source == TriggerSource::Manual) {
    const uint32_t now = millis();
    if (_lastManualRequestAtMs != 0 &&
        (now - _lastManualRequestAtMs) < kMinManualTriggerIntervalMs) {
      if (errorCode != nullptr) {
        *errorCode = "ota_too_frequent";
      }
      setState("FAILED", "Manual OTA request is too frequent.");
      _lastError = "ota_too_frequent";
      return false;
    }
  }

  _pendingRequest = true;
  _pendingAction = action;
  _pendingTrigger = source;
  _lastTrigger = source;
  if (action == RequestAction::CheckOnly) {
    if (source == TriggerSource::Auto) {
      setState("QUEUED", "Auto OTA check request queued.");
    } else {
      setState("QUEUED", "Manual OTA check request queued.");
    }
  } else if (source == TriggerSource::Auto) {
    setState("QUEUED", "Auto OTA upgrade request queued.");
  } else {
    setState("QUEUED", "Manual OTA upgrade request queued.");
  }

  if (errorCode != nullptr) {
    *errorCode = "";
  }
  return true;
}

bool FirmwareUpgradeService::isNewerThanInstalledVersion(
    const FirmwarePackageInfo& packageInfo) const {
  if (packageInfo.versionCode < 0) {
    return true;
  }
  if (_installedVersionCode < 0) {
    return true;
  }
  return packageInfo.versionCode > _installedVersionCode;
}

int32_t FirmwareUpgradeService::loadInstalledVersionCode() const {
  ConfigStore store;
  const SystemConfig config = store.loadSystemConfig();
  return config.otaInstalledVersionCode;
}

bool FirmwareUpgradeService::saveInstalledVersionCode(int32_t versionCode) const {
  ConfigStore store;
  SystemConfig config = store.loadSystemConfig();
  config.otaInstalledVersionCode = versionCode;
  return store.saveSystemConfig(config);
}

uint32_t FirmwareUpgradeService::autoCheckIntervalMs() const {
  const uint32_t intervalMinutes = normalizeAutoCheckIntervalMinutes(_autoCheckIntervalMinutes);
  return intervalMinutes * 60UL * 1000UL;
}

String FirmwareUpgradeService::triggerToText(TriggerSource source) const {
  switch (source) {
    case TriggerSource::Manual:
      return "MANUAL";
    case TriggerSource::Auto:
      return "AUTO";
    case TriggerSource::None:
    default:
      return "NONE";
  }
}

void FirmwareUpgradeService::resetProgress() {
  _progressPercent = 0;
  _progressBytes = 0;
  _progressTotalBytes = 0;
  _lastProgressAtMs = 0;
  _lastNotifiedProgressPercent = 0;
}

void FirmwareUpgradeService::onHttpUpdateProgress(int current, int total) {
  if (total <= 0 || current < 0) {
    return;
  }

  const uint8_t percent =
      static_cast<uint8_t>((static_cast<uint64_t>(current) * 100ULL) / static_cast<uint64_t>(total));

  _progressBytes = static_cast<uint32_t>(current);
  _progressTotalBytes = static_cast<uint32_t>(total);
  _progressPercent = percent > 100 ? 100 : percent;
  _lastProgressAtMs = millis();
  _message = "Downloading firmware (" + String(_progressPercent) + "%).";

  bool shouldNotify = false;
  if (_progressPercent == 0 || _progressPercent == 100) {
    shouldNotify = true;
  } else if (_progressPercent >= (_lastNotifiedProgressPercent + kProgressNotifyStepPercent)) {
    shouldNotify = true;
  } else if (_lastNotifyAtMs == 0 || (millis() - _lastNotifyAtMs) >= kProgressNotifyIntervalMs) {
    shouldNotify = true;
  }

  if (shouldNotify) {
    notifyStatusChanged(true);
  }
}

void FirmwareUpgradeService::notifyStatusChanged(bool force) {
  if (_eventCallback == nullptr) {
    return;
  }

  const uint32_t now = millis();
  if (!force && _lastNotifyAtMs != 0 && (now - _lastNotifyAtMs) < 300) {
    return;
  }

  _lastNotifyAtMs = now;
  _lastNotifiedProgressPercent = _progressPercent;
  const FirmwareUpgradeStatus status = getStatus();
  _eventCallback(status, _eventContext);
}

String FirmwareUpgradeService::urlEncode(const String& value) const {
  String encoded;
  encoded.reserve(value.length() * 3);

  for (size_t i = 0; i < value.length(); ++i) {
    const unsigned char c = static_cast<unsigned char>(value.charAt(i));
    const bool isAlphaNumeric =
        (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
    const bool isSafeSymbol = c == '-' || c == '_' || c == '.' || c == '~';
    if (isAlphaNumeric || isSafeSymbol) {
      encoded += static_cast<char>(c);
      continue;
    }

    char buffer[4];
    std::snprintf(buffer, sizeof(buffer), "%%%02X", c);
    encoded += buffer;
  }

  return encoded;
}

String FirmwareUpgradeService::buildLookupUrl() const {
  String url = String(kLookupApiHost) + "/api/device/v1/bin/?uid=" + urlEncode(_uid);
  url += "&topic=" + urlEncode(_topic);
  url += "&type=1";
  return url;
}

bool FirmwareUpgradeService::parseJsonNumber(const String& json,
                                             const String& key,
                                             long* value) const {
  if (value == nullptr) {
    return false;
  }

  const String token = "\"" + key + "\"";
  const int keyPos = json.indexOf(token);
  if (keyPos < 0) {
    return false;
  }

  const int colonPos = json.indexOf(':', keyPos + token.length());
  if (colonPos < 0) {
    return false;
  }

  int startPos = colonPos + 1;
  const int jsonLength = static_cast<int>(json.length());
  while (startPos < jsonLength) {
    const char c = json.charAt(startPos);
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
      break;
    }
    ++startPos;
  }
  if (startPos >= jsonLength) {
    return false;
  }

  int endPos = startPos;
  if (json.charAt(endPos) == '-') {
    ++endPos;
  }

  bool hasDigit = false;
  while (endPos < jsonLength) {
    const char c = json.charAt(endPos);
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      break;
    }
    hasDigit = true;
    ++endPos;
  }
  if (!hasDigit) {
    return false;
  }

  *value = json.substring(startPos, endPos).toInt();
  return true;
}

bool FirmwareUpgradeService::parseJsonString(const String& json,
                                             const String& key,
                                             String* value) const {
  if (value == nullptr) {
    return false;
  }

  const String token = "\"" + key + "\"";
  const int keyPos = json.indexOf(token);
  if (keyPos < 0) {
    return false;
  }

  const int colonPos = json.indexOf(':', keyPos + token.length());
  if (colonPos < 0) {
    return false;
  }

  const int quoteStart = json.indexOf('\"', colonPos + 1);
  if (quoteStart < 0) {
    return false;
  }

  const int jsonLength = static_cast<int>(json.length());
  int quoteEnd = quoteStart + 1;
  bool escaped = false;
  while (quoteEnd < jsonLength) {
    const char c = json.charAt(quoteEnd);
    if (c == '\"' && !escaped) {
      break;
    }
    if (c == '\\' && !escaped) {
      escaped = true;
    } else {
      escaped = false;
    }
    ++quoteEnd;
  }
  if (quoteEnd >= jsonLength) {
    return false;
  }

  *value = json.substring(quoteStart + 1, quoteEnd);
  value->replace("\\/", "/");
  value->replace("\\\"", "\"");
  value->replace("\\\\", "\\");
  return true;
}

bool FirmwareUpgradeService::parseLookupResponse(const String& body,
                                                 FirmwarePackageInfo* packageInfo,
                                                 String* errorCode,
                                                 String* detailMessage) const {
  if (packageInfo == nullptr) {
    return false;
  }

  long apiCode = 0;
  if (!parseJsonNumber(body, "code", &apiCode)) {
    if (errorCode != nullptr) {
      *errorCode = "ota_lookup_parse_failed";
    }
    if (detailMessage != nullptr) {
      *detailMessage = "Failed to parse Bemfa OTA response code.";
    }
    return false;
  }

  if (apiCode == kLookupNoPackageCode) {
    if (errorCode != nullptr) {
      *errorCode = "ota_no_update";
    }
    if (detailMessage != nullptr) {
      *detailMessage = "No OTA package is published on Bemfa.";
    }
    return false;
  }

  if (apiCode != kLookupSuccessCode) {
    if (errorCode != nullptr) {
      *errorCode = "ota_lookup_rejected";
    }
    if (detailMessage != nullptr) {
      *detailMessage = "Bemfa OTA rejected request, code=" + String(apiCode) + ".";
    }
    return false;
  }

  if (!parseJsonString(body, "url", &packageInfo->url) || packageInfo->url.isEmpty()) {
    if (errorCode != nullptr) {
      *errorCode = "ota_url_missing";
    }
    if (detailMessage != nullptr) {
      *detailMessage = "Bemfa OTA response does not contain firmware URL.";
    }
    return false;
  }

  long versionNumber = 0;
  if (!parseJsonNumber(body, "v", &versionNumber) || versionNumber < 0) {
    if (errorCode != nullptr) {
      *errorCode = "ota_version_invalid";
    }
    if (detailMessage != nullptr) {
      *detailMessage = "Bemfa OTA response has invalid version code.";
    }
    return false;
  }
  packageInfo->version = String(versionNumber);
  packageInfo->versionCode = static_cast<int32_t>(versionNumber);

  String tag;
  if (parseJsonString(body, "tag", &tag)) {
    packageInfo->tag = tag;
  }

  long sizeValue = 0;
  if (parseJsonNumber(body, "size", &sizeValue) && sizeValue > 0) {
    packageInfo->size = static_cast<uint32_t>(sizeValue);
  }

  String releasedAt;
  if (parseJsonString(body, "time", &releasedAt)) {
    packageInfo->releasedAt = releasedAt;
  }

  if (errorCode != nullptr) {
    *errorCode = "";
  }
  if (detailMessage != nullptr) {
    *detailMessage = "OTA package found on Bemfa.";
  }
  return true;
}

bool FirmwareUpgradeService::queryPackageFromBemfa(FirmwarePackageInfo* packageInfo,
                                                   String* errorCode,
                                                   String* detailMessage) const {
  if (!isConfigValid()) {
    if (errorCode != nullptr) {
      *errorCode = "ota_config_incomplete";
    }
    if (detailMessage != nullptr) {
      *detailMessage = "OTA requires Bemfa UID and Topic.";
    }
    return false;
  }

  HTTPClient http;
  http.setConnectTimeout(kHttpTimeoutMs);
  http.setTimeout(kHttpTimeoutMs);

  const String url = buildLookupUrl();
  if (!http.begin(url)) {
    if (errorCode != nullptr) {
      *errorCode = "ota_lookup_http_failed";
    }
    if (detailMessage != nullptr) {
      *detailMessage = "Failed to open Bemfa OTA endpoint.";
    }
    return false;
  }

  const int httpCode = http.GET();
  if (httpCode <= 0) {
    if (errorCode != nullptr) {
      *errorCode = "ota_lookup_http_failed";
    }
    if (detailMessage != nullptr) {
      *detailMessage = "Bemfa OTA request failed, code=" + String(httpCode) + ".";
    }
    http.end();
    return false;
  }

  const String body = http.getString();
  http.end();

  if (httpCode != 200) {
    if (errorCode != nullptr) {
      *errorCode = "ota_lookup_http_status";
    }
    if (detailMessage != nullptr) {
      *detailMessage = "Bemfa OTA HTTP status is " + String(httpCode) + ".";
    }
    return false;
  }

  return parseLookupResponse(body, packageInfo, errorCode, detailMessage);
}

bool FirmwareUpgradeService::executeUpgrade(const FirmwarePackageInfo& packageInfo,
                                            String* errorCode,
                                            String* detailMessage) {
  WiFiClient client;
  httpUpdate.rebootOnUpdate(false);
  httpUpdate.onProgress([this](int current, int total) { onHttpUpdateProgress(current, total); });

  const t_httpUpdate_return result = httpUpdate.update(client, packageInfo.url);

  switch (result) {
    case HTTP_UPDATE_OK:
      if (errorCode != nullptr) {
        *errorCode = "";
      }
      if (detailMessage != nullptr) {
        *detailMessage = "Firmware update completed, rebooting.";
      }
      return true;
    case HTTP_UPDATE_NO_UPDATES:
      if (errorCode != nullptr) {
        *errorCode = "ota_no_update";
      }
      if (detailMessage != nullptr) {
        *detailMessage = "No new firmware to apply.";
      }
      return false;
    case HTTP_UPDATE_FAILED:
    default:
      if (errorCode != nullptr) {
        *errorCode = "ota_update_failed";
      }
      if (detailMessage != nullptr) {
        *detailMessage = "Firmware update failed: " + httpUpdate.getLastErrorString();
      }
      return false;
  }
}

void FirmwareUpgradeService::setState(const String& state, const String& message) {
  _state = state;
  _message = message;
  notifyStatusChanged(true);
}
