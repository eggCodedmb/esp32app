#include <Arduino.h>
#include <Preferences.h>
#include <unity.h>

#include "ConfigStore.h"

namespace {
constexpr const char* kConfigNamespace = "esp32app";

void clearConfigNamespace() {
  Preferences preferences;
  const bool opened = preferences.begin(kConfigNamespace, false);
  TEST_ASSERT_TRUE(opened);
  if (opened) {
    preferences.clear();
    preferences.end();
  }
}
}  // namespace

void setUp() {
  clearConfigNamespace();
}

void tearDown() {}

void test_load_default_config_values() {
  ConfigStore store;
  const ComputerConfig config = store.loadComputerConfig();

  TEST_ASSERT_EQUAL_STRING("192.168.1.100", config.ip.c_str());
  TEST_ASSERT_EQUAL_STRING("00:11:22:33:44:55", config.mac.c_str());
  TEST_ASSERT_EQUAL_UINT16(3389, config.port);
}

void test_save_and_load_config_roundtrip() {
  ConfigStore store;
  ComputerConfig expected;
  expected.ip = "10.0.0.123";
  expected.mac = "AA:BB:CC:DD:EE:FF";
  expected.port = 8080;

  TEST_ASSERT_TRUE(store.saveComputerConfig(expected));

  const ComputerConfig actual = store.loadComputerConfig();
  TEST_ASSERT_EQUAL_STRING(expected.ip.c_str(), actual.ip.c_str());
  TEST_ASSERT_EQUAL_STRING(expected.mac.c_str(), actual.mac.c_str());
  TEST_ASSERT_EQUAL_UINT16(expected.port, actual.port);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  UNITY_BEGIN();
  RUN_TEST(test_load_default_config_values);
  RUN_TEST(test_save_and_load_config_roundtrip);
  UNITY_END();
}

void loop() {}
