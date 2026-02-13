#pragma once

#include <Arduino.h>

struct ComputerConfig {
  String name;
  String ip;
  String mac;
  uint16_t port;
  String owner;
};

class ConfigStore {
 public:
  ConfigStore() = default;

  ComputerConfig loadComputerConfig() const;
  bool saveComputerConfig(const ComputerConfig& config) const;

 private:
  static constexpr const char* kNamespace = "esp32app";
};
