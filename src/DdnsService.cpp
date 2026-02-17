#include "DdnsService.h"
#include "AliyunDdnsClient.h"
#include "ConfigStore.h"
#include "PublicIpService.h"

DdnsService::DdnsService(ConfigStore& configStore) : _configStore(configStore) {}

#include <algorithm>

namespace
{
  constexpr const char *kProviderId = "aliyun";
  constexpr uint32_t kDefaultDdnsIntervalSeconds = 300;
  constexpr uint32_t kMinDdnsIntervalSeconds = 30;
  constexpr uint32_t kMaxDdnsIntervalSeconds = 86400;

  bool isValidIntervalSeconds(uint32_t value)
  {
    return value >= kMinDdnsIntervalSeconds && value <= kMaxDdnsIntervalSeconds;
  }

  String normalizeDdnsProvider(const String &provider)
  {
    // 现在只支持阿里云DDNS
    return String(kProviderId);
  }

  bool ddnsProviderRequiresDomain(const String &provider)
  {
    (void)provider;
    return true;
  }

  bool ddnsProviderRequiresPassword(const String &provider)
  {
    // 阿里云需要AccessKeySecret作为密码
    return provider == kProviderId;
  }

  void splitDomain(const String& fullDomain, String& rootDomain, String& subDomain) {
    const int firstDotPos = fullDomain.indexOf('.');
    const int lastDotPos = fullDomain.lastIndexOf('.');
    if (firstDotPos == -1) {
      rootDomain = fullDomain;
      subDomain = "@";
      return;
    }

    // one dot: treat as apex domain, RR should be '@'
    if (firstDotPos == lastDotPos) {
      rootDomain = fullDomain;
      subDomain = "@";
      return;
    }

    subDomain = fullDomain.substring(0, firstDotPos);
    rootDomain = fullDomain.substring(firstDotPos + 1);
  }
} // namespace

void DdnsService::configureRuntimeRecord(RuntimeRecord* runtime) {
  if (runtime == nullptr) {
    return;
  }
  
  // For Aliyun, username = AccessKeyId, password = AccessKeySecret
  // domain is the full domain (e.g., "www.hupokeji.top")
  String rootDomain, subDomain;
  splitDomain(runtime->config.domain, rootDomain, subDomain);
  runtime->client.begin(
    runtime->config.username,
    runtime->config.password,
    rootDomain,
    subDomain,
    runtime->config.useIpv6
  );
}


void DdnsService::begin()
{
  if (_begun)
  {
    return;
  }
  _begun = true;
}

void DdnsService::updateConfig(const DdnsConfig &config)
{
  const DdnsConfig normalized = normalizeConfig(config);
  if (configsEqual(_config, normalized))
  {
    return;
  }

  _config = normalized;
  _totalUpdateCount = 0;
  rebuildRuntimeRecords();

  if (!_config.enabled)
  {
    setState("DISABLED", "DDNS disabled.");
    return;
  }

  bool hasConfiguredRecord = false;
  for (size_t index = 0; index < _runtimeRecords.size(); ++index)
  {
    if (_runtimeRecords[index].config.enabled &&
        isRecordConfigured(_runtimeRecords[index].config))
    {
      hasConfiguredRecord = true;
      break;
    }
  }

  if (!hasConfiguredRecord)
  {
    setState("WAIT_CONFIG", "DDNS config incomplete.");
    return;
  }

  setState("READY", "DDNS config updated.");
}

void DdnsService::tick(bool wifiConnected)
{
  if (!_begun)
  {
    begin();
  }

  _wifiConnected = wifiConnected;

  if (!_config.enabled)
  {
    setState("DISABLED", "DDNS disabled.");
    for (size_t index = 0; index < _runtimeRecords.size(); ++index)
    {
      setRecordState(&_runtimeRecords[index], "DISABLED", "Record disabled.");
    }
    return;
  }

  bool hasConfiguredRecord = false;
  for (size_t index = 0; index < _runtimeRecords.size(); ++index)
  {
    if (_runtimeRecords[index].config.enabled &&
        isRecordConfigured(_runtimeRecords[index].config))
    {
      hasConfiguredRecord = true;
      break;
    }
  }

  if (!hasConfiguredRecord)
  {
    setState("WAIT_CONFIG", "DDNS config incomplete.");
    for (size_t index = 0; index < _runtimeRecords.size(); ++index)
    {
      RuntimeRecord &record = _runtimeRecords[index];
      if (!record.config.enabled)
      {
        setRecordState(&record, "DISABLED", "Record disabled.");
      }
      else if (!isRecordConfigured(record.config))
      {
        setRecordState(&record, "WAIT_CONFIG", "Record config incomplete.");
      }
    }
    return;
  }

  if (!wifiConnected)
  {
    setState("WAIT_WIFI", "WiFi disconnected.");
    for (size_t index = 0; index < _runtimeRecords.size(); ++index)
    {
      RuntimeRecord &record = _runtimeRecords[index];
      if (!record.config.enabled)
      {
        setRecordState(&record, "DISABLED", "Record disabled.");
      }
      else if (!isRecordConfigured(record.config))
      {
        setRecordState(&record, "WAIT_CONFIG", "Record config incomplete.");
      }
      else
      {
        setRecordState(&record, "WAIT_WIFI", "Waiting WiFi connection.");
      }
    }
    return;
  }

  const uint32_t now = millis();
  for (size_t index = 0; index < _runtimeRecords.size(); ++index)
  {
    RuntimeRecord &record = _runtimeRecords[index];
    if (!record.config.enabled)
    {
      setRecordState(&record, "DISABLED", "Record disabled.");
      continue;
    }

    if (!isRecordConfigured(record.config))
    {
      setRecordState(&record, "WAIT_CONFIG", "Record config incomplete.");
      continue;
    }

    const uint32_t intervalMs = record.config.updateIntervalSeconds * 1000UL;
    if (record.nextSyncDueAtMs == 0)
    {
      record.nextSyncDueAtMs = now;
    }
    const bool shouldSync =
        record.firstSyncPending || static_cast<int32_t>(now - record.nextSyncDueAtMs) >= 0;

    if (!record.lastNewIp.isEmpty())
    {
      setRecordState(&record, "RUNNING", "Last synced IP: " + record.lastNewIp + ".");
    }
    else if (record.firstSyncPending)
    {
      setRecordState(&record, "RUNNING", "Running first sync...");
    }
    else
    {
      setRecordState(&record, "RUNNING", "Monitoring public IP changes.");
    }

    if (!shouldSync)
    {
      continue;
    }

    const String observedIp = PublicIpService::resolve(record.config.useLocalIp, record.config.useIpv6);
    if (!observedIp.isEmpty())
    {
      record.lastNewIp = observedIp;
    }

    const uint32_t updateCountBefore = record.updateCount;
    record.client.update(now, record.config.useLocalIp);
    record.firstSyncPending = false;
    record.nextSyncDueAtMs = now + intervalMs;

    if (record.updateCount == updateCountBefore)
    {
      if (record.lastNewIp.isEmpty())
      {
        setRecordState(&record, "RUNNING", "Sync attempted; waiting provider callback for latest IP.");
      }
      else
      {
        setRecordState(&record,
                       "RUNNING",
                       "Sync attempted; provider callback pending. Detected IP: " + record.lastNewIp + ".");
      }
    }
  }

  setState("RUNNING", "DDNS running.");
}

DdnsRuntimeStatus DdnsService::getStatus() const
{
  DdnsRuntimeStatus status;
  status.enabled = _config.enabled;
  status.wifiConnected = _wifiConnected;
  status.state = _state;
  status.message = _message;
  status.totalUpdateCount = _totalUpdateCount;

  uint32_t activeRecordCount = 0;
  for (size_t index = 0; index < _runtimeRecords.size(); ++index)
  {
    const RuntimeRecord &record = _runtimeRecords[index];
    if (record.config.enabled && isRecordConfigured(record.config))
    {
      activeRecordCount += 1;
    }
  }
  status.activeRecordCount = activeRecordCount;
  status.configured = activeRecordCount > 0;
  return status;
}

std::vector<DdnsRecordRuntimeStatus> DdnsService::getRecordStatuses() const
{
  std::vector<DdnsRecordRuntimeStatus> records;
  records.reserve(_runtimeRecords.size());
  for (size_t index = 0; index < _runtimeRecords.size(); ++index)
  {
    const RuntimeRecord &runtime = _runtimeRecords[index];
    DdnsRecordRuntimeStatus status;
    status.enabled = runtime.config.enabled;
    status.configured = isRecordConfigured(runtime.config);
    status.provider = runtime.config.provider;
    status.domain = runtime.config.domain;
    status.username = runtime.config.username;
    status.updateIntervalSeconds = runtime.config.updateIntervalSeconds;
    status.useLocalIp = runtime.config.useLocalIp;
    status.useIpv6 = runtime.config.useIpv6;
    status.state = runtime.state;
    status.message = runtime.message;
    status.lastOldIp = runtime.lastOldIp;
    status.lastNewIp = runtime.lastNewIp;
    status.updateCount = runtime.updateCount;
    status.lastUpdateAtMs = runtime.lastUpdateAtMs;
    records.push_back(status);
  }
  return records;
}

void DdnsService::normalizeRecord(DdnsRecordConfig *record)
{
  if (record == nullptr)
  {
    return;
  }

  record->provider = normalizeDdnsProvider(record->provider);
  record->domain.trim();
  record->username.trim();
  record->password.trim();
  if (!isValidIntervalSeconds(record->updateIntervalSeconds))
  {
    record->updateIntervalSeconds = kDefaultDdnsIntervalSeconds;
  }
}

DdnsConfig DdnsService::normalizeConfig(const DdnsConfig &config)
{
  DdnsConfig normalized;
  normalized.enabled = config.enabled;

  const size_t maxRecords = ConfigStore::kMaxDdnsRecords;
  const size_t count = std::min(config.records.size(), maxRecords);
  normalized.records.reserve(count);

  for (size_t index = 0; index < count; ++index)
  {
    DdnsRecordConfig record = config.records[index];
    normalizeRecord(&record);
    normalized.records.push_back(record);
  }

  return normalized;
}

bool DdnsService::isRecordConfigured(const DdnsRecordConfig &record)
{
  if (!record.enabled)
  {
    return false;
  }

  if (record.provider.isEmpty() || record.username.isEmpty())
  {
    return false;
  }
  if (ddnsProviderRequiresDomain(record.provider) && record.domain.isEmpty())
  {
    return false;
  }
  if (ddnsProviderRequiresPassword(record.provider) && record.password.isEmpty())
  {
    return false;
  }
  return true;
}

bool DdnsService::recordsEqual(const DdnsRecordConfig &lhs, const DdnsRecordConfig &rhs)
{
  return lhs.enabled == rhs.enabled && lhs.provider == rhs.provider && lhs.domain == rhs.domain &&
         lhs.username == rhs.username && lhs.password == rhs.password &&
         lhs.updateIntervalSeconds == rhs.updateIntervalSeconds &&
         lhs.useLocalIp == rhs.useLocalIp && lhs.useIpv6 == rhs.useIpv6;
}

bool DdnsService::configsEqual(const DdnsConfig &lhs, const DdnsConfig &rhs)
{
  if (lhs.enabled != rhs.enabled || lhs.records.size() != rhs.records.size())
  {
    return false;
  }

  for (size_t index = 0; index < lhs.records.size(); ++index)
  {
    if (!recordsEqual(lhs.records[index], rhs.records[index]))
    {
      return false;
    }
  }

  return true;
}

void DdnsService::setRecordState(RuntimeRecord *record,
                                 const String &state,
                                 const String &message)
{
  if (record == nullptr)
  {
    return;
  }
  record->state = state;
  record->message = message;
}

void DdnsService::rebuildRuntimeRecords()
{
  _runtimeRecords.clear();
  _runtimeRecords.reserve(_config.records.size());

  for (size_t index = 0; index < _config.records.size(); ++index)
  {
    RuntimeRecord runtime{};
    runtime.config = _config.records[index];
    configureRuntimeRecord(&runtime);

    if (!runtime.config.enabled)
    {
      runtime.state = "DISABLED";
      runtime.message = "Record disabled.";
    }
    else if (!isRecordConfigured(runtime.config))
    {
      runtime.state = "WAIT_CONFIG";
      runtime.message = "Record config incomplete.";
    }
    else
    {
      runtime.state = "READY";
      runtime.message = "Ready to sync.";
    }

    _runtimeRecords.push_back(runtime);
  }

  for (size_t index = 0; index < _runtimeRecords.size(); ++index)
  {
    RuntimeRecord& record = _runtimeRecords[index];
    auto onUpdateCallback = [this, index](const char *oldIp, const char *newIp)
    {
      if (index >= _runtimeRecords.size()) {
        return;
      }

      RuntimeRecord& runtime = _runtimeRecords[index];
      runtime.updateCount += 1;
      runtime.lastUpdateAtMs = millis();
      runtime.lastOldIp = oldIp != nullptr ? String(oldIp) : "";
      runtime.lastNewIp = newIp != nullptr ? String(newIp) : "";
      runtime.firstSyncPending = false;
      if (!runtime.lastOldIp.isEmpty()) {
        setRecordState(&runtime,
                       "UPDATED",
                       "IP changed from " + runtime.lastOldIp + " to " + runtime.lastNewIp + ".");
      } else {
        setRecordState(&runtime, "UPDATED", "IP updated to " + runtime.lastNewIp + ".");
      }

      _totalUpdateCount += 1;
      setState("RUNNING", "DDNS update completed.");
    };

    record.client.onUpdate(onUpdateCallback);
  }
}

void DdnsService::setState(const String &state, const String &message)
{
  _state = state;
  _message = message;
}
