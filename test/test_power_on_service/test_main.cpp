#include <Arduino.h>
#include <unity.h>

#include "PowerOnService.h"

namespace {
WakeOnLanService wol;
HostProbeService probe;

ComputerConfig makeValidConfig() {
  ComputerConfig config;
  config.ip = "192.168.1.100";
  config.mac = "AA:BB:CC:DD:EE:FF";
  config.port = 3389;
  return config;
}
}  // namespace

void setUp() {}

void tearDown() {}

void test_request_power_on_requires_wifi_connected() {
  PowerOnService service(wol, probe);
  const ComputerConfig config = makeValidConfig();
  String errorCode;

  TEST_ASSERT_FALSE(service.requestPowerOn(config, false, &errorCode));
  TEST_ASSERT_EQUAL_STRING("wifi_not_connected", errorCode.c_str());
}

void test_request_power_on_requires_mac() {
  PowerOnService service(wol, probe);
  ComputerConfig config = makeValidConfig();
  config.mac = "";
  String errorCode;

  TEST_ASSERT_FALSE(service.requestPowerOn(config, true, &errorCode));
  TEST_ASSERT_EQUAL_STRING("config_mac_required", errorCode.c_str());
}

void test_request_power_on_requires_valid_ip() {
  PowerOnService service(wol, probe);
  ComputerConfig config = makeValidConfig();
  config.ip = "invalid-ip";
  String errorCode;

  TEST_ASSERT_FALSE(service.requestPowerOn(config, true, &errorCode));
  TEST_ASSERT_EQUAL_STRING("config_ip_invalid", errorCode.c_str());
}

void test_request_power_on_enters_booting_state() {
  PowerOnService service(wol, probe);
  const ComputerConfig config = makeValidConfig();
  String errorCode;

  TEST_ASSERT_TRUE(service.requestPowerOn(config, true, &errorCode));
  TEST_ASSERT_EQUAL_STRING("", errorCode.c_str());

  const PowerOnStatus status = service.getStatus();
  TEST_ASSERT_EQUAL_STRING("BOOTING", status.stateText.c_str());
  TEST_ASSERT_TRUE(status.busy);
}

void test_tick_marks_failed_when_wifi_drops_during_boot() {
  PowerOnService service(wol, probe);
  const ComputerConfig config = makeValidConfig();
  String errorCode;
  TEST_ASSERT_TRUE(service.requestPowerOn(config, true, &errorCode));

  service.tick(false);
  const PowerOnStatus status = service.getStatus();
  TEST_ASSERT_EQUAL_STRING("FAILED", status.stateText.c_str());
  TEST_ASSERT_EQUAL_STRING("wifi_not_connected", status.errorCode.c_str());
}

void setup() {
  Serial.begin(115200);
  delay(200);

  UNITY_BEGIN();
  RUN_TEST(test_request_power_on_requires_wifi_connected);
  RUN_TEST(test_request_power_on_requires_mac);
  RUN_TEST(test_request_power_on_requires_valid_ip);
  RUN_TEST(test_request_power_on_enters_booting_state);
  RUN_TEST(test_tick_marks_failed_when_wifi_drops_during_boot);
  UNITY_END();
}

void loop() {}
