#include "DeviceStateService.h"

DeviceStateService::DeviceStateService(ConfigStore& configStore) : _configStore(configStore) {}

void DeviceStateService::load() {
  _poweredOn = _configStore.loadPowerState();
}

bool DeviceStateService::isPoweredOn() const {
  return _poweredOn;
}

bool DeviceStateService::setPowerState(bool poweredOn) {
  if (!_configStore.savePowerState(poweredOn)) {
    return false;
  }
  _poweredOn = poweredOn;
  return true;
}

bool DeviceStateService::togglePowerState() {
  return setPowerState(!_poweredOn);
}
