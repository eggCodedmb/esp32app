#include <Arduino.h>
#include <WiFi.h>
#include <unity.h>

#include "WifiService.h"

void setUp() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);
}

void tearDown() {}

void test_connect_with_empty_ssid_is_rejected() {
  WifiService service;

  TEST_ASSERT_FALSE(service.connectTo("", ""));
  TEST_ASSERT_TRUE(service.lastMessage().indexOf("SSID") >= 0);
}

void test_scan_networks_returns_a_valid_container() {
  WifiService service;
  const std::vector<WifiNetworkInfo> networks = service.scanNetworks();

  TEST_ASSERT_TRUE(networks.size() >= 0);
}

void test_connection_accessors_are_safe_to_call() {
  WifiService service;
  const bool connected = service.isConnected();

  if (!connected) {
    TEST_ASSERT_EQUAL_STRING("", service.currentSsid().c_str());
    TEST_ASSERT_EQUAL_STRING("", service.ipAddress().c_str());
  } else {
    TEST_ASSERT_TRUE(service.currentSsid().length() > 0);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  UNITY_BEGIN();
  RUN_TEST(test_connect_with_empty_ssid_is_rejected);
  RUN_TEST(test_scan_networks_returns_a_valid_container);
  RUN_TEST(test_connection_accessors_are_safe_to_call);
  UNITY_END();
}

void loop() {}
