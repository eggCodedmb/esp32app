#pragma once

#include <Arduino.h>
#include <vector>

#include "ConfigStore.h"
#include "AliyunDdnsClient.h"

struct DdnsRecordRuntimeStatus {
  bool enabled = false;
  bool configured = false;
  String provider = "";
  String domain = "";
  String username = "";
  uint32_t updateIntervalSeconds = 300;
  bool useLocalIp = false;
  bool useIpv6 = false;
  String state = "IDLE";
  String message = "";
  String lastOldIp = "";
  String lastNewIp = "";
  uint32_t updateCount = 0;
  uint32_t lastUpdateAtMs = 0;
};

struct DdnsRuntimeStatus {
  bool enabled = false;
  bool configured = false;
  bool wifiConnected = false;
  uint32_t activeRecordCount = 0;
  uint32_t totalUpdateCount = 0;
  String state = "DISABLED";
  String message = "DDNS disabled.";
};

class DdnsService {
  public:
   static constexpr const char* kProviderId = "aliyun";

   DdnsService(ConfigStore& configStore);

  void begin();
  void updateConfig(const DdnsConfig& config);
  void tick(bool wifiConnected);

  DdnsRuntimeStatus getStatus() const;
  std::vector<DdnsRecordRuntimeStatus> getRecordStatuses() const;

 private:
  struct RuntimeRecord {
    DdnsRecordConfig config;
    AliyunDdnsClient client{};
    String state = "IDLE";
    String message = "";
    String lastOldIp = "";
    String lastNewIp = "";
    uint32_t updateCount = 0;
    uint32_t lastUpdateAtMs = 0;
    bool firstSyncPending = true;
    uint32_t nextSyncDueAtMs = 0;
  };

  static void normalizeRecord(DdnsRecordConfig* record);
  static DdnsConfig normalizeConfig(const DdnsConfig& config);
  static bool isRecordConfigured(const DdnsRecordConfig& record);
  static bool recordsEqual(const DdnsRecordConfig& lhs, const DdnsRecordConfig& rhs);
  static bool configsEqual(const DdnsConfig& lhs, const DdnsConfig& rhs);

  void rebuildRuntimeRecords();
  void setState(const String& state, const String& message);
  void setRecordState(RuntimeRecord* record, const String& state, const String& message);
  void configureRuntimeRecord(RuntimeRecord* runtime);

   ConfigStore& _configStore;
   DdnsConfig _config;
   std::vector<RuntimeRecord> _runtimeRecords;

  bool _begun = false;
  bool _wifiConnected = false;
  uint32_t _totalUpdateCount = 0;
  String _state = "DISABLED";
  String _message = "DDNS disabled.";
};
