#pragma once

#include <Arduino.h>

class TimeService {
 public:
  // 阿里云NTP服务器列表
  static constexpr const char* kAliyunNtpServers[] = {
    "ntp1.aliyun.com",
    "ntp2.aliyun.com", 
    "ntp3.aliyun.com",
    "ntp4.aliyun.com",
    "ntp5.aliyun.com",
    "ntp6.aliyun.com",
    "ntp7.aliyun.com"
  };
  static constexpr size_t kAliyunNtpServerCount = 7;
  static constexpr uint16_t kNtpPort = 123;
  static constexpr uint32_t kNtpTimeoutMs = 5000;
  static constexpr uint32_t kDefaultSyncIntervalSeconds = 86400; // 24小时
  static constexpr uint32_t kMinSyncIntervalSeconds = 3600; // 1小时
  static constexpr uint32_t kRetryIntervalSeconds = 300; // 5分钟

  TimeService() = default;

  // 初始化时间服务
  void begin();

  // 主循环调用，处理定期同步和重试
  void tick(bool wifiConnected);

  // 手动触发时间同步（忽略时间间隔）
  bool sync(bool wifiConnected);

  // 检查时间是否已同步
  bool isSynced() const { return _synced; }

  // 获取当前Unix时间戳（秒），如果未同步返回0
  uint32_t getUnixTime() const;

  // 获取当前Unix时间戳（毫秒），如果未同步返回0
  uint64_t getUnixTimeMs() const;

  // 获取格式化时间字符串（YYYY-MM-DD HH:MM:SS UTC）
  String getFormattedTime() const;

  // 获取上次同步的时间戳（Unix秒）
  uint32_t getLastSyncTime() const { return _lastSyncTime; }

  // 获取同步状态信息
  String getState() const { return _state; }
  String getMessage() const { return _message; }

  // 设置同步间隔（秒）
  void setSyncInterval(uint32_t intervalSeconds);

 private:
  // NTP协议相关方法
  bool queryNtpServer(const char* server, uint32_t* unixTime);
  uint32_t ntpToUnix(uint32_t ntpSeconds, uint32_t ntpFraction);
  uint32_t parseNtpPacket(const uint8_t* packet, size_t length);

  // 状态管理
  void setState(const String& state, const String& message);

  // 配置
  uint32_t _syncIntervalSeconds = kDefaultSyncIntervalSeconds;

  // 状态
  bool _begun = false;
  bool _synced = false;
  uint32_t _lastSyncTime = 0; // Unix秒
  uint32_t _lastSyncAttemptMs = 0;
  uint32_t _nextSyncDueMs = 0;
  uint32_t _serverIndex = 0;
  uint32_t _timeOffset = 0; // 从设备启动时间到NTP时间的偏移（毫秒）

  // 状态文本
  String _state = "DISABLED";
  String _message = "Time service not started.";
};