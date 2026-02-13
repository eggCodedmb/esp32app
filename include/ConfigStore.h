#pragma once

#include <Arduino.h>

struct ComputerConfig {
  String ip;
  String mac;
  uint16_t port;
};

struct BemfaConfig {
  bool enabled = false;
  String host = "bemfa.com";
  uint16_t port = 9501;
  String uid = "";
  String key = "";
  String topic = "";
};

struct SystemConfig {
  // 0 means manual refresh mode.
  uint16_t statusPollIntervalMinutes = 3;
  bool otaAutoCheckEnabled = false;
  // Default auto-check interval when enabled.
  uint16_t otaAutoCheckIntervalMinutes = 60;
  // Persisted installed OTA version code from Bemfa API field `v`.
  int32_t otaInstalledVersionCode = -1;
};

class ConfigStore {
 public:
  ConfigStore() = default;

  ComputerConfig loadComputerConfig() const;
  bool saveComputerConfig(const ComputerConfig& config) const;
  BemfaConfig loadBemfaConfig() const;
  bool saveBemfaConfig(const BemfaConfig& config) const;
  SystemConfig loadSystemConfig() const;
  bool saveSystemConfig(const SystemConfig& config) const;

 private:
  static constexpr const char* kNamespace = "esp32app";
};
