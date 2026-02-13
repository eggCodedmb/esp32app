#pragma once

#include <Arduino.h>

struct ComputerConfig {
  String name;
  String ip;
  uint16_t port;
  String owner;
};

class ConfigStore {
 public:
  ConfigStore() = default;

  ComputerConfig loadComputerConfig() const;
  bool saveComputerConfig(const ComputerConfig& config) const;

  bool loadPowerState() const;
  bool savePowerState(bool poweredOn) const;

 private:
  static constexpr const char* kNamespace = "esp32app";
};
