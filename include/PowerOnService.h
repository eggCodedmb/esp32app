#pragma once

#include <Arduino.h>

#include "ConfigStore.h"
#include "HostProbeService.h"
#include "WakeOnLanService.h"

enum class PowerOnState { Idle, Booting, On, Failed };

struct PowerOnStatus {
  PowerOnState state = PowerOnState::Idle;
  String stateText = "IDLE";
  String message = "待机";
  String errorCode = "";
  bool busy = false;
};

class PowerOnService {
 public:
  PowerOnService(WakeOnLanService& wakeOnLanService, HostProbeService& hostProbeService);

  bool requestPowerOn(const ComputerConfig& config, bool wifiConnected, String* errorCode = nullptr);
  void tick(bool wifiConnected);
  PowerOnStatus getStatus() const;

 private:
  static String stateToText(PowerOnState state);
  void setState(PowerOnState state, const String& message, const String& errorCode = "");
  bool hasRequiredConfig(const ComputerConfig& config, String* errorCode) const;

  WakeOnLanService& _wakeOnLanService;
  HostProbeService& _hostProbeService;

  PowerOnState _state = PowerOnState::Idle;
  String _message = "待机";
  String _errorCode = "";

  ComputerConfig _targetConfig;
  uint32_t _startMs = 0;
  uint32_t _lastWakeSendMs = 0;
  uint32_t _lastProbeMs = 0;
  uint8_t _wakeSendCount = 0;
};
