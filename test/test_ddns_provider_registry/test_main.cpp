#include <Arduino.h>
#include <unity.h>

#include "DdnsProviderRegistry.h"

void setUp() {}

void tearDown() {}

void test_duckdns_provider_metadata_is_exposed() {
  const DdnsProviderSpec& provider = DdnsProviderRegistry::resolve("duckdns");
  TEST_ASSERT_EQUAL_STRING("duckdns", provider.id);
  TEST_ASSERT_TRUE(provider.requiresDomain);
  TEST_ASSERT_FALSE(provider.requiresPassword);
}

void test_unknown_provider_falls_back_to_duckdns() {
  const String normalized = DdnsProviderRegistry::normalizeProvider("noip");
  TEST_ASSERT_EQUAL_STRING("duckdns", normalized.c_str());
}

void setup() {
  Serial.begin(115200);
  delay(200);

  UNITY_BEGIN();
  RUN_TEST(test_duckdns_provider_metadata_is_exposed);
  RUN_TEST(test_unknown_provider_falls_back_to_duckdns);
  UNITY_END();
}

void loop() {}
