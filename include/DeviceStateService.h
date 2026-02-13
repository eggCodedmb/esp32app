#pragma once

#include "ConfigStore.h"

class DeviceStateService {
 public:
  explicit DeviceStateService(ConfigStore& configStore);

  void load();
  bool isPoweredOn() const;
  bool setPowerState(bool poweredOn);
  bool togglePowerState();

 private:
  ConfigStore& _configStore;
  bool _poweredOn = false;
};
