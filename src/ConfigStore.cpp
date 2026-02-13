#include "ConfigStore.h"

#include <Preferences.h>

namespace {
constexpr const char* kComputerIpKey = "pc_ip";
constexpr const char* kComputerMacKey = "pc_mac";
constexpr const char* kComputerPortKey = "pc_port";

constexpr const char* kBemfaEnabledKey = "bemfa_en";
constexpr const char* kBemfaHostKey = "bemfa_host";
constexpr const char* kBemfaPortKey = "bemfa_port";
constexpr const char* kBemfaUidKey = "bemfa_uid";
constexpr const char* kBemfaKeyKey = "bemfa_key";
constexpr const char* kBemfaTopicKey = "bemfa_topic";
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
