#pragma once

#include <Arduino.h>

#include "ConfigStore.h"

struct FirmwareUpgradeStatus;
using FirmwareUpgradeEventCallback = void (*)(const FirmwareUpgradeStatus& status, void* context);

struct FirmwareUpgradeStatus {
  bool configured = false;
  bool wifiConnected = false;
  bool busy = false;
  bool pending = false;
  bool updateAvailable = false;
  bool autoCheckEnabled = false;
  uint16_t autoCheckIntervalMinutes = 60;
  uint32_t nextAutoCheckInMs = 0;
  uint8_t progressPercent = 0;
  uint32_t progressBytes = 0;
  uint32_t progressTotalBytes = 0;
  String trigger = "NONE";
  String state = "IDLE";
  String message = "Manual OTA is idle.";
  String lastError = "";
  String currentVersion = "";
  String targetVersion = "";
  String targetTag = "";
  uint32_t lastAutoCheckAtMs = 0;
  uint32_t lastProgressAtMs = 0;
  uint32_t lastCheckAtMs = 0;
  uint32_t lastStartAtMs = 0;
  uint32_t lastFinishAtMs = 0;
};

class FirmwareUpgradeService {
 public:
  FirmwareUpgradeService() = default;

  void begin();
  void updateConfig(const BemfaConfig& config);
  void updateAutoCheckConfig(bool enabled, uint16_t intervalMinutes);
  void setEventCallback(FirmwareUpgradeEventCallback callback, void* context = nullptr);
  void tick(bool wifiConnected);

  bool requestManualCheck(bool wifiConnected, String* errorCode = nullptr);
  bool requestManualUpgrade(bool wifiConnected, String* errorCode = nullptr);
  FirmwareUpgradeStatus getStatus() const;

 private:
  enum class TriggerSource { None, Manual, Auto };
  enum class RequestAction { None, CheckOnly, Upgrade };

  static constexpr uint32_t kMinManualTriggerIntervalMs = 10000;
  static constexpr uint16_t kDefaultAutoCheckIntervalMinutes = 60;
  static constexpr uint16_t kHttpTimeoutMs = 12000;
  static constexpr uint8_t kBemfaDeviceType = 1;
  static constexpr uint32_t kProgressNotifyIntervalMs = 2000;
  static constexpr uint8_t kProgressNotifyStepPercent = 5;

  struct FirmwarePackageInfo {
    String url = "";
    String version = "";
    int32_t versionCode = -1;
    String tag = "";
    uint32_t size = 0;
    String releasedAt = "";
  };

  bool isValidAutoCheckIntervalMinutes(uint16_t intervalMinutes) const;
  uint16_t normalizeAutoCheckIntervalMinutes(uint16_t intervalMinutes) const;
  bool isConfigValid() const;
  bool queueRequest(RequestAction action,
                    TriggerSource source,
                    bool wifiConnected,
                    String* errorCode = nullptr);
  bool isVersionDifferentFromInstalled(const FirmwarePackageInfo& packageInfo) const;
  int32_t loadInstalledVersionCode() const;
  bool saveInstalledVersionCode(int32_t versionCode) const;
  uint32_t autoCheckIntervalMs() const;
  String triggerToText(TriggerSource source) const;
  void resetProgress();
  void onHttpUpdateProgress(int current, int total);
  void notifyStatusChanged(bool force = false);
  String urlEncode(const String& value) const;
  String buildLookupUrl() const;
  bool parseJsonNumber(const String& json, const String& key, long* value) const;
  bool parseJsonString(const String& json, const String& key, String* value) const;
  bool parseLookupResponse(const String& body,
                           FirmwarePackageInfo* packageInfo,
                           String* errorCode,
                           String* detailMessage) const;
  bool queryPackageFromBemfa(FirmwarePackageInfo* packageInfo,
                             String* errorCode,
                             String* detailMessage) const;
  bool executeUpgrade(const FirmwarePackageInfo& packageInfo,
                      String* errorCode,
                      String* detailMessage);
  void setState(const String& state, const String& message);

  bool _begun = false;
  bool _wifiConnected = false;
  bool _busy = false;
  bool _pendingRequest = false;
  bool _updateAvailable = false;
  bool _autoCheckEnabled = false;

  String _uid = "";
  String _topic = "";
  String _state = "IDLE";
  String _message = "Manual OTA is idle.";
  String _lastError = "";
  String _targetVersion = "";
  String _targetTag = "";
  int32_t _installedVersionCode = -1;
  int32_t _targetVersionCode = -1;

  uint16_t _autoCheckIntervalMinutes = kDefaultAutoCheckIntervalMinutes;
  uint8_t _progressPercent = 0;
  uint32_t _progressBytes = 0;
  uint32_t _progressTotalBytes = 0;
  uint8_t _lastNotifiedProgressPercent = 0;

  TriggerSource _pendingTrigger = TriggerSource::None;
  TriggerSource _activeTrigger = TriggerSource::None;
  TriggerSource _lastTrigger = TriggerSource::None;
  RequestAction _pendingAction = RequestAction::None;
  FirmwarePackageInfo _cachedPackageInfo;
  bool _hasCachedPackage = false;

  FirmwareUpgradeEventCallback _eventCallback = nullptr;
  void* _eventContext = nullptr;

  uint32_t _lastAutoCheckAtMs = 0;
  uint32_t _lastProgressAtMs = 0;
  uint32_t _lastNotifyAtMs = 0;
  uint32_t _lastCheckAtMs = 0;
  uint32_t _lastStartAtMs = 0;
  uint32_t _lastFinishAtMs = 0;
  uint32_t _lastManualCheckRequestAtMs = 0;
  uint32_t _lastManualUpgradeRequestAtMs = 0;
};
