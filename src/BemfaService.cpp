#include "BemfaService.h"

#include <ESP.h>
#include <cstdio>

BemfaService* g_bemfaServiceInstance = nullptr;

void bemfaMqttCallbackBridge(char* topic, uint8_t* payload, unsigned int length) {
  if (g_bemfaServiceInstance != nullptr) {
    g_bemfaServiceInstance->onMqttMessage(topic, payload, length);
  }
}

BemfaService::BemfaService() : _mqttClient(_netClient) {}

void BemfaService::begin() {
  g_bemfaServiceInstance = this;
  _mqttClient.setServer(_config.host.c_str(), _config.port);
  _mqttClient.setKeepAlive(kKeepAliveSeconds);
  _mqttClient.setSocketTimeout(kSocketTimeoutSeconds);
  _mqttClient.setCallback(bemfaMqttCallbackBridge);
  _mqttConnected = false;
  _begun = true;
}

void BemfaService::updateConfig(const BemfaConfig& config) {
  BemfaConfig normalized = config;
  normalized.host.trim();
  normalized.uid.trim();
  normalized.key.trim();
  normalized.topic = normalizeTopic(normalized.topic);

  if (normalized.host.isEmpty()) {
    normalized.host = "bemfa.com";
  }
  if (normalized.port == 0) {
    normalized.port = 9501;
  }

  if (configsEqual(_config, normalized)) {
    return;
  }

  _config = normalized;
  _mqttClient.setServer(_config.host.c_str(), _config.port);

  if (_mqttClient.connected()) {
    disconnect("Config updated.");
  }

  _lastReconnectAttemptMs = 0;

  if (!_config.enabled) {
    setState("DISABLED", "Bemfa disabled.");
    return;
  }

  if (!isConfigValid()) {
    setState("WAIT_CONFIG", "Bemfa config incomplete.");
    return;
  }

  setState("READY", "Bemfa config updated.");
}

void BemfaService::tick(bool wifiConnected) {
  _wifiConnected = wifiConnected;

  if (!_begun) {
    begin();
  }

  if (!_config.enabled) {
    _mqttConnected = _mqttClient.connected();
    if (_mqttConnected) {
      disconnect("Bemfa disabled.");
    }
    setState("DISABLED", "Bemfa disabled.");
    return;
  }

  if (!isConfigValid()) {
    _mqttConnected = _mqttClient.connected();
    if (_mqttConnected) {
      disconnect("Bemfa config incomplete.");
    }
    setState("WAIT_CONFIG", "Bemfa config incomplete.");
    return;
  }

  if (!wifiConnected) {
    _mqttConnected = _mqttClient.connected();
    if (_mqttConnected) {
      disconnect("WiFi disconnected.");
    }
    setState("WAIT_WIFI", "WiFi disconnected.");
    return;
  }

  _mqttConnected = _mqttClient.connected();
  if (_mqttConnected) {
    if (!_mqttClient.loop()) {
      disconnect("MQTT loop failed.");
      setState("ERROR", "MQTT loop failed.");
    } else {
      setState("ONLINE", "Connected to Bemfa.");
    }
    return;
  }

  const uint32_t now = millis();
  if (_lastReconnectAttemptMs == 0 || (now - _lastReconnectAttemptMs) >= kReconnectIntervalMs) {
    _lastReconnectAttemptMs = now;
    setState("CONNECTING", "Connecting to Bemfa...");
    connectBroker();
  }
}

bool BemfaService::takeCommand(String* command) {
  if (command == nullptr || !_hasPendingCommand) {
    return false;
  }

  *command = _pendingCommand;
  _pendingCommand = "";
  _hasPendingCommand = false;
  return true;
}

bool BemfaService::publishStatus(const String& payload) {
  _mqttConnected = _mqttClient.connected();
  if (!_mqttConnected) {
    return false;
  }

  String normalizedPayload = payload;
  normalizedPayload.trim();
  if (normalizedPayload.isEmpty()) {
    return false;
  }

  const String topic = publishTopic();
  const bool published = _mqttClient.publish(topic.c_str(), normalizedPayload.c_str());
  if (published) {
    _lastPublish = normalizedPayload;
    _lastPublishAtMs = millis();
  }
  return published;
}

bool BemfaService::isConnected() const {
  return _mqttConnected;
}

BemfaRuntimeStatus BemfaService::getStatus() const {
  BemfaRuntimeStatus status;
  status.enabled = _config.enabled;
  status.configured = isConfigValid();
  status.wifiConnected = _wifiConnected;
  status.mqttConnected = _mqttConnected;
  status.state = _state;
  status.message = _message;
  status.subscribeTopic = subscribeTopic();
  status.publishTopic = publishTopic();
  status.lastCommand = _lastCommand;
  status.lastPublish = _lastPublish;
  status.reconnectCount = _reconnectCount;
  status.lastConnectAtMs = _lastConnectAtMs;
  status.lastCommandAtMs = _lastCommandAtMs;
  status.lastPublishAtMs = _lastPublishAtMs;
  return status;
}

bool BemfaService::configsEqual(const BemfaConfig& lhs, const BemfaConfig& rhs) const {
  return lhs.enabled == rhs.enabled && lhs.host == rhs.host && lhs.port == rhs.port &&
         lhs.uid == rhs.uid && lhs.key == rhs.key && lhs.topic == rhs.topic;
}

bool BemfaService::isConfigValid() const {
  return !_config.uid.isEmpty() && !_config.topic.isEmpty() && !_config.host.isEmpty() &&
         _config.port > 0;
}

String BemfaService::normalizeTopic(const String& topic) const {
  String normalized = topic;
  normalized.trim();

  while (normalized.endsWith("/")) {
    normalized.remove(normalized.length() - 1);
  }

  return normalized;
}

String BemfaService::subscribeTopic() const {
  return _config.topic + "/set";
}

String BemfaService::legacySubscribeTopic() const {
  return _config.topic;
}

String BemfaService::publishTopic() const {
  return _config.topic + "/up";
}

String BemfaService::buildClientId() const {
  const uint64_t chipId = ESP.getEfuseMac();
  char buffer[24];
  std::snprintf(buffer, sizeof(buffer), "esp32app-%04X%08X",
                static_cast<uint16_t>(chipId >> 32), static_cast<uint32_t>(chipId));
  return String(buffer);
}

void BemfaService::setState(const String& state, const String& message) {
  _state = state;
  _message = message;
}

bool BemfaService::connectBroker() {
  if (!isConfigValid()) {
    setState("WAIT_CONFIG", "Bemfa config incomplete.");
    return false;
  }

  bool connected = false;
  int connectRc = -1;

  // Preferred by Bemfa docs: use UID as MQTT ClientID, username/password can be empty.
  const String clientIdByUid = _config.uid;
  connected = _mqttClient.connect(clientIdByUid.c_str());
  connectRc = _mqttClient.state();

  // Optional fallback: treat UID/Key as username/password when direct UID mode fails.
  if (!connected && !_config.key.isEmpty()) {
    const String fallbackClientId = buildClientId();
    connected = _mqttClient.connect(fallbackClientId.c_str(), _config.uid.c_str(), _config.key.c_str());
    connectRc = _mqttClient.state();
  }

  if (!connected) {
    _mqttConnected = false;
    setState("ERROR", "MQTT connect failed, rc=" + String(connectRc));
    return false;
  }

  const bool subscribedSet = _mqttClient.subscribe(subscribeTopic().c_str());
  const bool subscribedLegacy = _mqttClient.subscribe(legacySubscribeTopic().c_str());
  if (!subscribedSet && !subscribedLegacy) {
    disconnect("Subscribe failed.");
    setState("ERROR", "Bemfa subscribe failed.");
    return false;
  }

  _mqttConnected = true;
  _reconnectCount += 1;
  _lastConnectAtMs = millis();
  setState("ONLINE", "Connected to Bemfa.");
  publishStatus("online");
  return true;
}

void BemfaService::disconnect(const String& reason) {
  if (_mqttClient.connected()) {
    _mqttClient.disconnect();
  }
  _mqttConnected = false;
  setState("OFFLINE", reason);
}

void BemfaService::onMqttMessage(char* topic, const uint8_t* payload, unsigned int length) {
  if (topic == nullptr || payload == nullptr || length == 0) {
    return;
  }

  const String receivedTopic = String(topic);
  const String expectedSetTopic = subscribeTopic();
  const String expectedLegacyTopic = legacySubscribeTopic();
  if (receivedTopic != expectedSetTopic && receivedTopic != expectedLegacyTopic) {
    return;
  }

  String command;
  command.reserve(length);
  for (unsigned int index = 0; index < length; ++index) {
    const char c = static_cast<char>(payload[index]);
    if (c == '\r' || c == '\n' || c == '\0') {
      continue;
    }
    command += c;
  }
  command.trim();
  command.toLowerCase();
  if (command.isEmpty()) {
    return;
  }

  _pendingCommand = command;
  _hasPendingCommand = true;
  _lastCommand = command;
  _lastCommandAtMs = millis();
}
