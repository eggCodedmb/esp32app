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

class ConfigStore {
 public:
  ConfigStore() = default;

  ComputerConfig loadComputerConfig() const;
  bool saveComputerConfig(const ComputerConfig& config) const;
  BemfaConfig loadBemfaConfig() const;
  bool saveBemfaConfig(const BemfaConfig& config) const;

 private:
  static constexpr const char* kNamespace = "esp32app";
};
