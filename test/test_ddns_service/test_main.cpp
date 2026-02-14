#include <Arduino.h>
#include <unity.h>

#include "DdnsService.h"

namespace {
DdnsService service;
}

void setUp() {}

void tearDown() {}

void test_default_status_is_disabled() {
  const DdnsRuntimeStatus status = service.getStatus();
  TEST_ASSERT_FALSE(status.enabled);
  TEST_ASSERT_EQUAL_STRING("DISABLED", status.state.c_str());
}

void test_configured_record_is_exposed() {
  DdnsConfig config;
  config.enabled = true;

  DdnsRecordConfig record;
  record.enabled = true;
  record.provider = "duckdns";
  record.domain = "demo";
  record.username = "token";
  record.updateIntervalSeconds = 300;
  config.records.push_back(record);

  service.updateConfig(config);
  service.tick(false);

  const DdnsRuntimeStatus status = service.getStatus();
  TEST_ASSERT_TRUE(status.enabled);
  TEST_ASSERT_TRUE(status.configured);
  TEST_ASSERT_EQUAL_UINT32(1, status.activeRecordCount);
  TEST_ASSERT_EQUAL_STRING("WAIT_WIFI", status.state.c_str());

  const std::vector<DdnsRecordRuntimeStatus> records = service.getRecordStatuses();
  TEST_ASSERT_EQUAL_UINT32(1, static_cast<uint32_t>(records.size()));
  TEST_ASSERT_EQUAL_STRING("duckdns", records[0].provider.c_str());
  TEST_ASSERT_EQUAL_UINT32(300, records[0].updateIntervalSeconds);
}

void test_non_duckdns_provider_and_interval_are_normalized() {
  DdnsConfig config;
  config.enabled = true;

  DdnsRecordConfig record;
  record.enabled = true;
  record.provider = "noip";
  record.domain = "demo";
  record.username = "token";
  record.updateIntervalSeconds = 1;
  config.records.push_back(record);

  service.updateConfig(config);
  const std::vector<DdnsRecordRuntimeStatus> records = service.getRecordStatuses();
  TEST_ASSERT_EQUAL_UINT32(1, static_cast<uint32_t>(records.size()));
  TEST_ASSERT_EQUAL_STRING("duckdns", records[0].provider.c_str());
  TEST_ASSERT_EQUAL_UINT32(300, records[0].updateIntervalSeconds);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  UNITY_BEGIN();
  RUN_TEST(test_default_status_is_disabled);
  RUN_TEST(test_configured_record_is_exposed);
  RUN_TEST(test_non_duckdns_provider_and_interval_are_normalized);
  UNITY_END();
}

void loop() {}
