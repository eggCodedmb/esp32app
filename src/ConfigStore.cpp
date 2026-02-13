#include "ConfigStore.h"

#include <Preferences.h>

ComputerConfig ConfigStore::loadComputerConfig() const {
  Preferences preferences;
  ComputerConfig config{"Office-PC", "192.168.1.100", "00:11:22:33:44:55", 3389, "admin"};

  if (!preferences.begin(kNamespace, true)) {
    return config;
  }

  config.name = preferences.getString("pc_name", config.name);
  config.ip = preferences.getString("pc_ip", config.ip);
  config.mac = preferences.getString("pc_mac", config.mac);
  config.port = preferences.getUShort("pc_port", config.port);
  config.owner = preferences.getString("pc_owner", config.owner);

  preferences.end();
  return config;
}

bool ConfigStore::saveComputerConfig(const ComputerConfig& config) const {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    return false;
  }

  preferences.putString("pc_name", config.name);
  preferences.putString("pc_ip", config.ip);
  preferences.putString("pc_mac", config.mac);
  preferences.putUShort("pc_port", config.port);
  preferences.putString("pc_owner", config.owner);

  preferences.end();
  return true;
}
