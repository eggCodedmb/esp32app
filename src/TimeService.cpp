#include "TimeService.h"

#include <WiFi.h>
#include <WiFiUdp.h>
#include <time.h>
#include <sys/time.h>

#include <cstring>
#include <stdlib.h>

namespace {
constexpr const char* kAliyunNtpServers[] = {
    "ntp1.aliyun.com",
    "ntp2.aliyun.com",
    "ntp3.aliyun.com",
    "ntp4.aliyun.com",
    "ntp5.aliyun.com",
    "ntp6.aliyun.com",
    "ntp7.aliyun.com"};
constexpr size_t kAliyunNtpServerCount = sizeof(kAliyunNtpServers) / sizeof(kAliyunNtpServers[0]);
constexpr uint32_t kNtpEpochOffset = 2208988800UL;
constexpr uint32_t kMillisPerSecond = 1000UL;
}  // namespace

void TimeService::begin() {
  if (_begun) {
    return;
  }
  _begun = true;

  // POSIX timezone rule uses an inverted sign. CST-8 means UTC+8.
  setenv("TZ", "CST-8", 1);
  tzset();

  time_t testTime = 1700000000;  // 2023-11-14 22:13:20 UTC
  struct tm* localTm = localtime(&testTime);
  if (localTm != nullptr) {
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S %Z", localTm);
  }

  setState("READY", "Time service ready.");
}

void TimeService::tick(bool wifiConnected) {
  if (!_begun) {
    begin();
  }

  if (!wifiConnected) {
    if (_synced) {
      setState("SYNCED", "Time synced, WiFi disconnected.");
    } else {
      setState("WAIT_WIFI", "Waiting for WiFi connection.");
    }
    return;
  }

  const uint32_t now = millis();

  if (!_synced) {
    if (_lastSyncAttemptMs == 0 ||
        static_cast<int32_t>(now - _lastSyncAttemptMs) >=
            static_cast<int32_t>(kRetryIntervalSeconds * kMillisPerSecond)) {
      sync(wifiConnected);
    }
    return;
  }

  if (_nextSyncDueMs == 0) {
    _nextSyncDueMs = now + _syncIntervalSeconds * kMillisPerSecond;
  }

  if (static_cast<int32_t>(now - _nextSyncDueMs) >= 0) {
    sync(wifiConnected);
    _nextSyncDueMs = now + _syncIntervalSeconds * kMillisPerSecond;
  }
}

bool TimeService::sync(bool wifiConnected) {
  if (!wifiConnected) {
    setState("WAIT_WIFI", "Cannot sync: WiFi disconnected.");
    return false;
  }

  setState("SYNCING", "Syncing time with Aliyun NTP server...");
  _lastSyncAttemptMs = millis();

  bool success = false;
  uint32_t unixTime = 0;

  for (size_t i = 0; i < kAliyunNtpServerCount; ++i) {
    const size_t index = (_serverIndex + i) % kAliyunNtpServerCount;
    const char* server = kAliyunNtpServers[index];

    if (queryNtpServer(server, &unixTime)) {
      success = true;
      _serverIndex = index;
      break;
    }

    delay(100);
  }

  if (!success) {
    setState("SYNC_FAILED", "Failed to sync with all NTP servers.");
    return false;
  }

  const uint32_t nowSeconds = millis() / kMillisPerSecond;
  _timeOffset = unixTime - nowSeconds;
  _synced = true;
  _lastSyncTime = unixTime;

  struct timeval tv;
  tv.tv_sec = static_cast<time_t>(unixTime);
  tv.tv_usec = 0;
  if (settimeofday(&tv, nullptr) != 0) {
    Serial.println("[TimeService] Warning: failed to set system clock.");
  }

  char stateBuffer[64];
  snprintf(stateBuffer,
           sizeof(stateBuffer),
           "Time synced: %u (offset %d seconds)",
           unixTime,
           static_cast<int>(_timeOffset));
  setState("SYNCED", stateBuffer);


  time_t rawtime = static_cast<time_t>(unixTime);
  struct tm* utcTimeInfo = gmtime(&rawtime);
  struct tm* localTimeInfo = localtime(&rawtime);
  if (utcTimeInfo != nullptr && localTimeInfo != nullptr) {
    char utcBuffer[32];
    char localBuffer[32];
    strftime(utcBuffer, sizeof(utcBuffer), "%Y-%m-%d %H:%M:%S", utcTimeInfo);
    strftime(localBuffer, sizeof(localBuffer), "%Y-%m-%d %H:%M:%S", localTimeInfo);
  }

  return true;
}

uint32_t TimeService::getUnixTime() const {
  if (!_synced) {
    return 0;
  }

  const uint32_t nowSeconds = millis() / kMillisPerSecond;
  return nowSeconds + _timeOffset;
}

uint64_t TimeService::getUnixTimeMs() const {
  if (!_synced) {
    return 0;
  }

  const uint64_t nowMs = millis();
  const uint64_t nowSeconds = nowMs / kMillisPerSecond;
  const uint64_t unixSeconds = nowSeconds + _timeOffset;
  return unixSeconds * kMillisPerSecond + (nowMs % kMillisPerSecond);
}

String TimeService::getFormattedTime() const {
  if (!_synced) {
    return "Not synced";
  }

  const uint32_t unixTime = getUnixTime();
  const time_t rawtime = static_cast<time_t>(unixTime);
  struct tm* timeInfo = localtime(&rawtime);
  if (timeInfo == nullptr) {
    return "Not synced";
  }

  char buffer[32];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeInfo);
  return String(buffer);
}

void TimeService::setSyncInterval(uint32_t intervalSeconds) {
  if (intervalSeconds < kMinSyncIntervalSeconds) {
    intervalSeconds = kMinSyncIntervalSeconds;
  }

  _syncIntervalSeconds = intervalSeconds;
  _nextSyncDueMs = 0;
}

bool TimeService::queryNtpServer(const char* server, uint32_t* unixTime) {
  if (unixTime == nullptr) {
    return false;
  }

  WiFiUDP udp;
  if (!udp.begin(0)) {
    return false;
  }

  uint8_t ntpPacket[48];
  memset(ntpPacket, 0, sizeof(ntpPacket));
  ntpPacket[0] = 0b00100011;  // LI=0, Version=4, Mode=3 (client)

  if (!udp.beginPacket(server, kNtpPort)) {
    udp.stop();
    return false;
  }

  if (udp.write(ntpPacket, sizeof(ntpPacket)) != sizeof(ntpPacket)) {
    udp.stop();
    return false;
  }

  if (!udp.endPacket()) {
    udp.stop();
    return false;
  }

  const uint32_t startWait = millis();
  while (millis() - startWait < kNtpTimeoutMs) {
    const int packetSize = udp.parsePacket();
    if (packetSize >= 48) {
      uint8_t response[48];
      udp.read(response, sizeof(response));
      const uint32_t ntpTime = parseNtpPacket(response, sizeof(response));
      if (ntpTime != 0) {
        *unixTime = ntpTime;
        udp.stop();
        return true;
      }
    }
    delay(10);
  }

  udp.stop();
  return false;
}

uint32_t TimeService::ntpToUnix(uint32_t ntpSeconds, uint32_t ntpFraction) {
  if (ntpSeconds < kNtpEpochOffset) {
    return 0;
  }

  (void)ntpFraction;
  return ntpSeconds - kNtpEpochOffset;
}

uint32_t TimeService::parseNtpPacket(const uint8_t* packet, size_t length) {
  if (packet == nullptr || length < 48) {
    return 0;
  }

  const uint8_t mode = packet[0] & 0x07;
  if (mode != 4 && mode != 5) {
    return 0;
  }

  const uint32_t ntpSeconds = (static_cast<uint32_t>(packet[40]) << 24) |
                              (static_cast<uint32_t>(packet[41]) << 16) |
                              (static_cast<uint32_t>(packet[42]) << 8) |
                              static_cast<uint32_t>(packet[43]);

  const uint32_t ntpFraction = (static_cast<uint32_t>(packet[44]) << 24) |
                               (static_cast<uint32_t>(packet[45]) << 16) |
                               (static_cast<uint32_t>(packet[46]) << 8) |
                               static_cast<uint32_t>(packet[47]);

  const uint32_t unixTime = ntpToUnix(ntpSeconds, ntpFraction);
  return unixTime;
}

void TimeService::setState(const String& state, const String& message) {
  _state = state;
  _message = message;
}
