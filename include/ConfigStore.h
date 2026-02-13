#pragma once

#include <Arduino.h>

struct ComputerConfig {
  String ip;
  String mac;
  uint16_t port;
};

class ConfigStore {
 public:
  ConfigStore() = default;

  ComputerConfig loadComputerConfig() const;
  bool saveComputerConfig(const ComputerConfig& config) const;

 private:
  static constexpr const char* kNamespace = "esp32app";
};
