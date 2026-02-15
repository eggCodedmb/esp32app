#pragma once

#include <Arduino.h>

class TimeService {
 public:
  TimeService() = default;

  void begin();
  void tick(bool wifiConnected);
  bool sync(bool wifiConnected);

  bool isSynced() const { return _synced; }
  uint32_t getUnixTime() const;
  uint64_t getUnixTimeMs() const;
  String getFormattedTime() const;
  uint32_t getLastSyncTime() const { return _lastSyncTime; }
  String getState() const { return _state; }
  String getMessage() const { return _message; }

  void setSyncInterval(uint32_t intervalSeconds);

 private:
  static constexpr uint16_t kNtpPort = 123;
  static constexpr uint32_t kNtpTimeoutMs = 5000;
  static constexpr uint32_t kDefaultSyncIntervalSeconds = 86400;
  static constexpr uint32_t kMinSyncIntervalSeconds = 3600;
  static constexpr uint32_t kRetryIntervalSeconds = 300;

  bool queryNtpServer(const char* server, uint32_t* unixTime);
  uint32_t ntpToUnix(uint32_t ntpSeconds, uint32_t ntpFraction);
  uint32_t parseNtpPacket(const uint8_t* packet, size_t length);
  void setState(const String& state, const String& message);

  uint32_t _syncIntervalSeconds = kDefaultSyncIntervalSeconds;

  bool _begun = false;
  bool _synced = false;
  uint32_t _lastSyncTime = 0;
  uint32_t _lastSyncAttemptMs = 0;
  uint32_t _nextSyncDueMs = 0;
  uint32_t _serverIndex = 0;
  uint32_t _timeOffset = 0;

  String _state = "DISABLED";
  String _message = "Time service not started.";
};
