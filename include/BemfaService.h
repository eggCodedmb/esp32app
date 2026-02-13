#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

#include "ConfigStore.h"

struct BemfaRuntimeStatus {
  bool enabled = false;
  bool configured = false;
  bool wifiConnected = false;
  bool mqttConnected = false;
  String state = "DISABLED";
  String message = "Bemfa disabled.";
  String subscribeTopic = "";
  String publishTopic = "";
  String lastCommand = "";
  String lastPublish = "";
  uint32_t reconnectCount = 0;
  uint32_t lastConnectAtMs = 0;
  uint32_t lastCommandAtMs = 0;
  uint32_t lastPublishAtMs = 0;
};

class BemfaService {
 public:
  BemfaService();

  void begin();
  void updateConfig(const BemfaConfig& config);
  void tick(bool wifiConnected);

  bool takeCommand(String* command);
  bool publishStatus(const String& payload);

  bool isConnected() const;
  BemfaRuntimeStatus getStatus() const;

 private:
  static constexpr uint32_t kReconnectIntervalMs = 5000;
  static constexpr uint32_t kKeepAliveSeconds = 30;
  static constexpr uint32_t kSocketTimeoutSeconds = 15;

  bool configsEqual(const BemfaConfig& lhs, const BemfaConfig& rhs) const;
  bool isConfigValid() const;
  String normalizeTopic(const String& topic) const;
  String subscribeTopic() const;
  String legacySubscribeTopic() const;
  String publishTopic() const;
  String buildClientId() const;
  void setState(const String& state, const String& message);
  bool connectBroker();
  void disconnect(const String& reason);
  void onMqttMessage(char* topic, const uint8_t* payload, unsigned int length);

  WiFiClient _netClient;
  PubSubClient _mqttClient;
  BemfaConfig _config;

  bool _begun = false;
  bool _wifiConnected = false;
  bool _mqttConnected = false;
  bool _hasPendingCommand = false;

  String _pendingCommand = "";
  String _state = "DISABLED";
  String _message = "Bemfa disabled.";
  String _lastCommand = "";
  String _lastPublish = "";

  uint32_t _lastReconnectAttemptMs = 0;
  uint32_t _reconnectCount = 0;
  uint32_t _lastConnectAtMs = 0;
  uint32_t _lastCommandAtMs = 0;
  uint32_t _lastPublishAtMs = 0;

  friend void bemfaMqttCallbackBridge(char* topic, uint8_t* payload, unsigned int length);
};
