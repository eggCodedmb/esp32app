#include "PowerOnService.h"

namespace {
constexpr uint32_t kPowerOnTimeoutMs = 60000;
constexpr uint32_t kWakeSendIntervalMs = 400;
constexpr uint32_t kProbeIntervalMs = 2000;
constexpr uint8_t kWakeSendMaxCount = 3;
constexpr uint16_t kDefaultProbePort = 3389;
}  // namespace

PowerOnService::PowerOnService(WakeOnLanService& wakeOnLanService, HostProbeService& hostProbeService)
    : _wakeOnLanService(wakeOnLanService), _hostProbeService(hostProbeService) {}

bool PowerOnService::requestPowerOn(const ComputerConfig& config,
                                    bool wifiConnected,
                                    String* errorCode) {
  if (_state == PowerOnState::Booting) {
    if (errorCode != nullptr) {
      *errorCode = "already_booting";
    }
    return true;
  }

  if (_state == PowerOnState::On) {
    if (errorCode != nullptr) {
      *errorCode = "already_on";
    }
    return true;
  }

  if (!wifiConnected) {
    setState(PowerOnState::Failed, "WiFi未连接，无法发送开机包。", "wifi_not_connected");
    if (errorCode != nullptr) {
      *errorCode = _errorCode;
    }
    return false;
  }

  String validationError;
  if (!hasRequiredConfig(config, &validationError)) {
    if (validationError == "config_mac_required") {
      setState(PowerOnState::Failed, "请先配置目标电脑MAC地址。", validationError);
    } else if (validationError == "config_ip_invalid") {
      setState(PowerOnState::Failed, "请先配置正确的目标电脑IP地址。", validationError);
    } else {
      setState(PowerOnState::Failed, "开机配置不完整。", validationError);
    }
    if (errorCode != nullptr) {
      *errorCode = validationError;
    }
    return false;
  }

  _targetConfig = config;
  _startMs = millis();
  _lastWakeSendMs = 0;
  _lastProbeMs = 0;
  _wakeSendCount = 0;
  setState(PowerOnState::Booting, "正在发送Wake-on-LAN开机包...");

  if (errorCode != nullptr) {
    *errorCode = "";
  }
  return true;
}

void PowerOnService::tick(bool wifiConnected) {
  if (_state != PowerOnState::Booting) {
    return;
  }

  if (!wifiConnected) {
    setState(PowerOnState::Failed, "WiFi连接已断开，开机流程终止。", "wifi_not_connected");
    return;
  }

  const uint32_t now = millis();
  if ((now - _startMs) > kPowerOnTimeoutMs) {
    setState(PowerOnState::Failed, "等待主机上线超时。", "boot_timeout");
    return;
  }

  if (_wakeSendCount < kWakeSendMaxCount &&
      (_wakeSendCount == 0 || (now - _lastWakeSendMs) >= kWakeSendIntervalMs)) {
    String sendError;
    const bool sent = _wakeOnLanService.sendMagicPacket(_targetConfig.mac,
                                                         IPAddress(255, 255, 255, 255),
                                                         9,
                                                         &sendError);
    if (!sent) {
      setState(PowerOnState::Failed, "发送开机包失败。", sendError.isEmpty() ? "wol_send_failed" : sendError);
      return;
    }

    ++_wakeSendCount;
    _lastWakeSendMs = now;
    _message = "已发送开机包(" + String(_wakeSendCount) + "/" + String(kWakeSendMaxCount) + ")，等待主机上线...";
  }

  if (_lastProbeMs == 0 || (now - _lastProbeMs) >= kProbeIntervalMs) {
    _lastProbeMs = now;
    const uint16_t probePort = _targetConfig.port == 0 ? kDefaultProbePort : _targetConfig.port;
    if (_hostProbeService.isHostReachable(_targetConfig.ip, probePort)) {
      setState(PowerOnState::On, "主机已上线。");
      return;
    }

    if (_wakeSendCount >= kWakeSendMaxCount) {
      _message = "主机尚未上线，持续检测中...";
    }
  }
}

PowerOnStatus PowerOnService::getStatus() const {
  PowerOnStatus status;
  status.state = _state;
  status.stateText = stateToText(_state);
  status.message = _message;
  status.errorCode = _errorCode;
  status.busy = _state == PowerOnState::Booting;
  return status;
}

String PowerOnService::stateToText(PowerOnState state) {
  switch (state) {
    case PowerOnState::Idle:
      return "IDLE";
    case PowerOnState::Booting:
      return "BOOTING";
    case PowerOnState::On:
      return "ON";
    case PowerOnState::Failed:
      return "FAILED";
    default:
      return "UNKNOWN";
  }
}

void PowerOnService::setState(PowerOnState state, const String& message, const String& errorCode) {
  _state = state;
  _message = message;
  _errorCode = errorCode;
}

bool PowerOnService::hasRequiredConfig(const ComputerConfig& config, String* errorCode) const {
  if (config.mac.isEmpty()) {
    if (errorCode != nullptr) {
      *errorCode = "config_mac_required";
    }
    return false;
  }

  IPAddress ip;
  if (!ip.fromString(config.ip)) {
    if (errorCode != nullptr) {
      *errorCode = "config_ip_invalid";
    }
    return false;
  }

  if (errorCode != nullptr) {
    *errorCode = "";
  }
  return true;
}
