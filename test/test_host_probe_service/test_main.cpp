#include <Arduino.h>
#include <unity.h>

#include "HostProbeService.h"

void setUp() {}

void tearDown() {}

void test_invalid_ip_is_not_reachable() {
  HostProbeService service;
  TEST_ASSERT_FALSE(service.isHostReachable("not-an-ip", 3389));
}

void test_zero_port_is_not_reachable() {
  HostProbeService service;
  TEST_ASSERT_FALSE(service.isHostReachable("192.168.1.10", 0));
}

void setup() {
  Serial.begin(115200);
  delay(200);

  UNITY_BEGIN();
  RUN_TEST(test_invalid_ip_is_not_reachable);
  RUN_TEST(test_zero_port_is_not_reachable);
  UNITY_END();
}

void loop() {}
