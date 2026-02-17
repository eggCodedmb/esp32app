#include "ConfigStore.h"

#include <Preferences.h>

namespace {
constexpr const char* kProviderId = "aliyun";
constexpr const char* kComputerIpKey = "pc_ip";
constexpr const char* kComputerMacKey = "pc_mac";
constexpr const char* kComputerPortKey = "pc_port";

constexpr const char* kBemfaEnabledKey = "bemfa_en";
constexpr const char* kBemfaHostKey = "bemfa_host";
constexpr const char* kBemfaPortKey = "bemfa_port";
constexpr const char* kBemfaUidKey = "bemfa_uid";
constexpr const char* kBemfaKeyKey = "bemfa_key";
constexpr const char* kBemfaTopicKey = "bemfa_topic";

constexpr const char* kStatusPollIntervalMinutesKey = "poll_min";
constexpr const char* kOtaAutoCheckEnabledKey = "ota_auto_en";
constexpr const char* kOtaAutoCheckIntervalMinutesKey = "ota_auto_min";
constexpr const char* kOtaInstalledVersionCodeKey = "ota_ver";

constexpr const char* kDdnsEnabledKey = "ddns_en";
constexpr const char* kDdnsCountKey = "ddns_cnt";

constexpr uint32_t kDefaultDdnsIntervalSeconds = 300;
constexpr uint32_t kMinDdnsIntervalSeconds = 30;
constexpr uint32_t kMaxDdnsIntervalSeconds = 86400;

bool isValidStatusPollIntervalMinutes(uint16_t value) {
  return value == 0 || value == 1 || value == 3 || value == 10 || value == 30 || value == 60;
}

bool isValidOtaAutoCheckIntervalMinutes(uint16_t value) {
  return value == 5 || value == 10 || value == 30 || value == 60 || value == 180 ||
         value == 360 || value == 720 || value == 1440;
}

bool isValidDdnsIntervalSeconds(uint32_t value) {
  return value >= kMinDdnsIntervalSeconds && value <= kMaxDdnsIntervalSeconds;
}

uint32_t normalizeDdnsIntervalSeconds(uint32_t value) {
  if (!isValidDdnsIntervalSeconds(value)) {
    return kDefaultDdnsIntervalSeconds;
  }
  return value;
}

String normalizeDdnsProvider(const String& provider) {
  // 现在只支持阿里云DDNS
  return String(kProviderId);
}

void normalizeDdnsRecord(DdnsRecordConfig* record) {
  if (record == nullptr) {
    return;
  }

  record->provider = normalizeDdnsProvider(record->provider);
  record->domain.trim();
  record->username.trim();
  record->password.trim();
  record->updateIntervalSeconds = normalizeDdnsIntervalSeconds(record->updateIntervalSeconds);
}

String ddnsRecordKey(size_t index, const char* suffix) {
  return "dd" + String(index) + "_" + String(suffix);
}
}  // namespace

ComputerConfig ConfigStore::loadComputerConfig() const {
  Preferences preferences;
  ComputerConfig config{"192.168.1.100", "00:11:22:33:44:55", 3389};

  if (!preferences.begin(kNamespace, true)) {
    return config;
  }

  config.ip = preferences.getString(kComputerIpKey, config.ip);
  config.mac = preferences.getString(kComputerMacKey, config.mac);
  config.port = preferences.getUShort(kComputerPortKey, config.port);

  preferences.end();
  return config;
}

bool ConfigStore::saveComputerConfig(const ComputerConfig& config) const {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    return false;
  }

  preferences.putString(kComputerIpKey, config.ip);
  preferences.putString(kComputerMacKey, config.mac);
  preferences.putUShort(kComputerPortKey, config.port);

  preferences.end();
  return true;
}

BemfaConfig ConfigStore::loadBemfaConfig() const {
  Preferences preferences;
  BemfaConfig config;

  if (!preferences.begin(kNamespace, true)) {
    return config;
  }

  config.enabled = preferences.getBool(kBemfaEnabledKey, config.enabled);
  config.host = preferences.getString(kBemfaHostKey, config.host);
  config.port = preferences.getUShort(kBemfaPortKey, config.port);
  config.uid = preferences.getString(kBemfaUidKey, config.uid);
  config.key = preferences.getString(kBemfaKeyKey, config.key);
  config.topic = preferences.getString(kBemfaTopicKey, config.topic);

  config.host.trim();
  if (config.host.isEmpty()) {
    config.host = "bemfa.com";
  }
  if (config.port == 0) {
    config.port = 9501;
  }

  preferences.end();
  return config;
}

bool ConfigStore::saveBemfaConfig(const BemfaConfig& config) const {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    return false;
  }

  preferences.putBool(kBemfaEnabledKey, config.enabled);
  preferences.putString(kBemfaHostKey, config.host);
  preferences.putUShort(kBemfaPortKey, config.port);
  preferences.putString(kBemfaUidKey, config.uid);
  preferences.putString(kBemfaKeyKey, config.key);
  preferences.putString(kBemfaTopicKey, config.topic);

  preferences.end();
  return true;
}

SystemConfig ConfigStore::loadSystemConfig() const {
  Preferences preferences;
  SystemConfig config;

  if (!preferences.begin(kNamespace, true)) {
    return config;
  }

  config.statusPollIntervalMinutes =
      preferences.getUShort(kStatusPollIntervalMinutesKey, config.statusPollIntervalMinutes);
  if (!isValidStatusPollIntervalMinutes(config.statusPollIntervalMinutes)) {
    config.statusPollIntervalMinutes = 3;
  }
  config.otaAutoCheckEnabled =
      preferences.getBool(kOtaAutoCheckEnabledKey, config.otaAutoCheckEnabled);
  config.otaAutoCheckIntervalMinutes = preferences.getUShort(kOtaAutoCheckIntervalMinutesKey,
                                                             config.otaAutoCheckIntervalMinutes);
  if (!isValidOtaAutoCheckIntervalMinutes(config.otaAutoCheckIntervalMinutes)) {
    config.otaAutoCheckIntervalMinutes = 60;
  }
  config.otaInstalledVersionCode =
      preferences.getInt(kOtaInstalledVersionCodeKey, config.otaInstalledVersionCode);

  preferences.end();
  return config;
}

bool ConfigStore::saveSystemConfig(const SystemConfig& config) const {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    return false;
  }

  uint16_t normalizedIntervalMinutes = config.statusPollIntervalMinutes;
  if (!isValidStatusPollIntervalMinutes(normalizedIntervalMinutes)) {
    normalizedIntervalMinutes = 3;
  }
  preferences.putUShort(kStatusPollIntervalMinutesKey, normalizedIntervalMinutes);

  uint16_t normalizedOtaAutoIntervalMinutes = config.otaAutoCheckIntervalMinutes;
  if (!isValidOtaAutoCheckIntervalMinutes(normalizedOtaAutoIntervalMinutes)) {
    normalizedOtaAutoIntervalMinutes = 60;
  }
  preferences.putBool(kOtaAutoCheckEnabledKey, config.otaAutoCheckEnabled);
  preferences.putUShort(kOtaAutoCheckIntervalMinutesKey, normalizedOtaAutoIntervalMinutes);
  preferences.putInt(kOtaInstalledVersionCodeKey, config.otaInstalledVersionCode);

  preferences.end();
  return true;
}

DdnsConfig ConfigStore::loadDdnsConfig() const {
  Preferences preferences;
  DdnsConfig config;

  if (!preferences.begin(kNamespace, true)) {
    return config;
  }

  config.enabled = preferences.getBool(kDdnsEnabledKey, config.enabled);
  const uint8_t rawCount = preferences.getUChar(kDdnsCountKey, 0);
  const size_t count =
      rawCount <= kMaxDdnsRecords ? static_cast<size_t>(rawCount) : kMaxDdnsRecords;

  config.records.reserve(count);
  for (size_t index = 0; index < count; ++index) {
    DdnsRecordConfig record;
    record.enabled = preferences.getBool(ddnsRecordKey(index, "en").c_str(), record.enabled);
    record.provider =
        preferences.getString(ddnsRecordKey(index, "pv").c_str(), record.provider);
    record.domain = preferences.getString(ddnsRecordKey(index, "dm").c_str(), record.domain);
    record.username = preferences.getString(ddnsRecordKey(index, "ur").c_str(), record.username);
    record.password = preferences.getString(ddnsRecordKey(index, "pw").c_str(), record.password);
    record.updateIntervalSeconds =
        preferences.getUInt(ddnsRecordKey(index, "iv").c_str(), record.updateIntervalSeconds);
    record.useLocalIp =
        preferences.getBool(ddnsRecordKey(index, "li").c_str(), record.useLocalIp);
    record.useIpv6 = preferences.getBool(ddnsRecordKey(index, "v6").c_str(), record.useIpv6);
    normalizeDdnsRecord(&record);
    config.records.push_back(record);
  }

  preferences.end();
  return config;
}

bool ConfigStore::saveDdnsConfig(const DdnsConfig& config) const {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    return false;
  }

  const size_t recordCount =
      config.records.size() <= kMaxDdnsRecords ? config.records.size() : kMaxDdnsRecords;

  preferences.putBool(kDdnsEnabledKey, config.enabled);
  preferences.putUChar(kDdnsCountKey, static_cast<uint8_t>(recordCount));

  for (size_t index = 0; index < recordCount; ++index) {
    DdnsRecordConfig record = config.records[index];
    normalizeDdnsRecord(&record);
    preferences.putBool(ddnsRecordKey(index, "en").c_str(), record.enabled);
    preferences.putString(ddnsRecordKey(index, "pv").c_str(), record.provider);
    preferences.putString(ddnsRecordKey(index, "dm").c_str(), record.domain);
    preferences.putString(ddnsRecordKey(index, "ur").c_str(), record.username);
    preferences.putString(ddnsRecordKey(index, "pw").c_str(), record.password);
    // RecordId is now auto-discovered at runtime and no longer persisted.
    preferences.remove(ddnsRecordKey(index, "ri").c_str());
    preferences.putUInt(ddnsRecordKey(index, "iv").c_str(), record.updateIntervalSeconds);
    preferences.putBool(ddnsRecordKey(index, "li").c_str(), record.useLocalIp);
    preferences.putBool(ddnsRecordKey(index, "v6").c_str(), record.useIpv6);
  }

  for (size_t index = recordCount; index < kMaxDdnsRecords; ++index) {
    preferences.remove(ddnsRecordKey(index, "en").c_str());
    preferences.remove(ddnsRecordKey(index, "pv").c_str());
    preferences.remove(ddnsRecordKey(index, "dm").c_str());
    preferences.remove(ddnsRecordKey(index, "ur").c_str());
    preferences.remove(ddnsRecordKey(index, "pw").c_str());
    preferences.remove(ddnsRecordKey(index, "ri").c_str());
    preferences.remove(ddnsRecordKey(index, "iv").c_str());
    preferences.remove(ddnsRecordKey(index, "li").c_str());
    preferences.remove(ddnsRecordKey(index, "v6").c_str());
  }

  preferences.end();
  return true;
}
