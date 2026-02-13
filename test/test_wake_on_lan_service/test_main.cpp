#include <Arduino.h>
#include <unity.h>

#include "WakeOnLanService.h"

void setUp() {}

void tearDown() {}

void test_send_magic_packet_rejects_invalid_mac() {
  WakeOnLanService service;
  String errorCode;

  TEST_ASSERT_FALSE(service.sendMagicPacket("INVALID-MAC", IPAddress(255, 255, 255, 255), 9, &errorCode));
  TEST_ASSERT_EQUAL_STRING("invalid_mac", errorCode.c_str());
}

void test_send_magic_packet_accepts_mac_format_check() {
  WakeOnLanService service;
  String errorCode;

  const bool sent =
      service.sendMagicPacket("AA:BB:CC:DD:EE:FF", IPAddress(255, 255, 255, 255), 9, &errorCode);
  if (sent) {
    TEST_ASSERT_EQUAL_STRING("", errorCode.c_str());
  } else {
    TEST_ASSERT_FALSE(errorCode == "invalid_mac");
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  UNITY_BEGIN();
  RUN_TEST(test_send_magic_packet_rejects_invalid_mac);
  RUN_TEST(test_send_magic_packet_accepts_mac_format_check);
  UNITY_END();
}

void loop() {}
