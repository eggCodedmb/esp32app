#include <Arduino.h>
#include <unity.h>

#define private public
#include "FirmwareUpgradeService.h"
#undef private

namespace {
constexpr const char* kFixedOtaUid = "c710930bedbb46b6b70a446e817ef66c";
constexpr const char* kFixedOtaTopic = "OTA";
}  // namespace

void setUp() {}

void tearDown() {}

void test_begin_sets_hardcoded_uid_and_topic() {
  FirmwareUpgradeService service;
  service.begin();

  TEST_ASSERT_EQUAL_STRING(kFixedOtaUid, service._uid.c_str());
  TEST_ASSERT_EQUAL_STRING(kFixedOtaTopic, service._topic.c_str());
}

void test_update_config_cannot_override_hardcoded_uid_and_topic() {
  FirmwareUpgradeService service;
  service.begin();

  BemfaConfig bemfaConfig;
  bemfaConfig.uid = "custom-uid";
  bemfaConfig.topic = "custom-topic";
  service.updateConfig(bemfaConfig);

  TEST_ASSERT_EQUAL_STRING(kFixedOtaUid, service._uid.c_str());
  TEST_ASSERT_EQUAL_STRING(kFixedOtaTopic, service._topic.c_str());
}

void test_lookup_url_uses_hardcoded_uid_and_topic() {
  FirmwareUpgradeService service;
  service.begin();
  service.updateConfig(BemfaConfig{});

  const String lookupUrl = service.buildLookupUrl();
  TEST_ASSERT_TRUE(lookupUrl.indexOf("uid=" + String(kFixedOtaUid)) >= 0);
  TEST_ASSERT_TRUE(lookupUrl.indexOf("topic=" + String(kFixedOtaTopic)) >= 0);
}

void test_manual_upgrade_uses_hardcoded_config_and_checks_wifi_first() {
  FirmwareUpgradeService service;
  service.begin();
  service.updateConfig(BemfaConfig{});

  String errorCode;
  TEST_ASSERT_FALSE(service.requestManualUpgrade(false, &errorCode));
  TEST_ASSERT_EQUAL_STRING("wifi_not_connected", errorCode.c_str());

  const FirmwareUpgradeStatus status = service.getStatus();
  TEST_ASSERT_TRUE(status.configured);
  TEST_ASSERT_EQUAL_STRING("FAILED", status.state.c_str());
}

void test_request_manual_upgrade_rejects_when_wifi_disconnected() {
  FirmwareUpgradeService service;
  service.begin();

  BemfaConfig config;
  config.uid = "uid-demo";
  config.topic = "topic-demo";
  service.updateConfig(config);

  String errorCode;
  TEST_ASSERT_FALSE(service.requestManualUpgrade(false, &errorCode));
  TEST_ASSERT_EQUAL_STRING("wifi_not_connected", errorCode.c_str());

  const FirmwareUpgradeStatus status = service.getStatus();
  TEST_ASSERT_EQUAL_STRING("FAILED", status.state.c_str());
}

void test_request_manual_upgrade_accepts_valid_manual_trigger() {
  FirmwareUpgradeService service;
  service.begin();

  BemfaConfig config;
  config.uid = "uid-demo";
  config.topic = "topic-demo";
  service.updateConfig(config);

  String errorCode;
  TEST_ASSERT_TRUE(service.requestManualUpgrade(true, &errorCode));
  TEST_ASSERT_EQUAL_STRING("", errorCode.c_str());

  const FirmwareUpgradeStatus status = service.getStatus();
  TEST_ASSERT_TRUE(status.pending);
  TEST_ASSERT_EQUAL_STRING("QUEUED", status.state.c_str());
  TEST_ASSERT_EQUAL_STRING("MANUAL", status.trigger.c_str());
}

void test_request_manual_check_accepts_valid_manual_trigger() {
  FirmwareUpgradeService service;
  service.begin();

  BemfaConfig config;
  config.uid = "uid-demo";
  config.topic = "topic-demo";
  service.updateConfig(config);

  String errorCode;
  TEST_ASSERT_TRUE(service.requestManualCheck(true, &errorCode));
  TEST_ASSERT_EQUAL_STRING("", errorCode.c_str());

  const FirmwareUpgradeStatus status = service.getStatus();
  TEST_ASSERT_TRUE(status.pending);
  TEST_ASSERT_EQUAL_STRING("QUEUED", status.state.c_str());
  TEST_ASSERT_EQUAL_STRING("MANUAL", status.trigger.c_str());
  TEST_ASSERT_EQUAL_STRING("Manual OTA check request queued.", status.message.c_str());
}

void test_manual_check_and_upgrade_use_independent_rate_limit() {
  FirmwareUpgradeService service;
  service.begin();
  service.updateConfig(BemfaConfig{});

  service._lastManualCheckRequestAtMs = millis();

  String errorCode;
  TEST_ASSERT_TRUE(service.requestManualUpgrade(true, &errorCode));
  TEST_ASSERT_EQUAL_STRING("", errorCode.c_str());
}

void test_manual_check_rate_limit_uses_check_error_code() {
  FirmwareUpgradeService service;
  service.begin();
  service.updateConfig(BemfaConfig{});
  service._lastManualCheckRequestAtMs = millis();

  String errorCode;
  TEST_ASSERT_FALSE(service.requestManualCheck(true, &errorCode));
  TEST_ASSERT_EQUAL_STRING("ota_check_too_frequent", errorCode.c_str());
}

void test_manual_upgrade_rate_limit_uses_upgrade_error_code() {
  FirmwareUpgradeService service;
  service.begin();
  service.updateConfig(BemfaConfig{});
  service._lastManualUpgradeRequestAtMs = millis();

  String errorCode;
  TEST_ASSERT_FALSE(service.requestManualUpgrade(true, &errorCode));
  TEST_ASSERT_EQUAL_STRING("ota_upgrade_too_frequent", errorCode.c_str());
}

void test_parse_lookup_response_reads_version_code_from_v_field() {
  FirmwareUpgradeService service;
  FirmwareUpgradeService::FirmwarePackageInfo packageInfo;
  String errorCode;
  String detailMessage;

  const String response =
      "{\"code\":5723007,\"data\":{\"url\":\"http://bin.bemfa.com/b/demo.bin\","
      "\"time\":\"2026-02-14 03:19:51\",\"v\":6,\"tag\":\"\",\"size\":1100912}}";

  TEST_ASSERT_TRUE(service.parseLookupResponse(response, &packageInfo, &errorCode, &detailMessage));
  TEST_ASSERT_EQUAL_STRING("", errorCode.c_str());
  TEST_ASSERT_EQUAL_STRING("OTA package found on Bemfa.", detailMessage.c_str());
  TEST_ASSERT_EQUAL_STRING("http://bin.bemfa.com/b/demo.bin", packageInfo.url.c_str());
  TEST_ASSERT_EQUAL_STRING("6", packageInfo.version.c_str());
  TEST_ASSERT_EQUAL_INT32(6, packageInfo.versionCode);
  TEST_ASSERT_EQUAL_UINT32(1100912, packageInfo.size);
}

void test_parse_lookup_response_fails_when_v_field_missing() {
  FirmwareUpgradeService service;
  FirmwareUpgradeService::FirmwarePackageInfo packageInfo;
  String errorCode;
  String detailMessage;

  const String response =
      "{\"code\":5723007,\"data\":{\"url\":\"http://bin.bemfa.com/b/demo.bin\","
      "\"time\":\"2026-02-14 03:19:51\",\"tag\":\"\",\"size\":1100912}}";

  TEST_ASSERT_FALSE(service.parseLookupResponse(response, &packageInfo, &errorCode, &detailMessage));
  TEST_ASSERT_EQUAL_STRING("ota_version_invalid", errorCode.c_str());
  TEST_ASSERT_EQUAL_STRING("Bemfa OTA response has invalid version code.", detailMessage.c_str());
}

void test_update_auto_check_config_forces_disabled() {
  FirmwareUpgradeService service;
  service.begin();
  service.updateAutoCheckConfig(true, 180);

  const FirmwareUpgradeStatus status = service.getStatus();
  TEST_ASSERT_FALSE(status.autoCheckEnabled);
  TEST_ASSERT_EQUAL_UINT16(180, status.autoCheckIntervalMinutes);
}

void test_update_auto_check_config_forces_disabled_and_normalizes_invalid_interval() {
  FirmwareUpgradeService service;
  service.begin();
  service.updateAutoCheckConfig(true, 7);

  const FirmwareUpgradeStatus status = service.getStatus();
  TEST_ASSERT_FALSE(status.autoCheckEnabled);
  TEST_ASSERT_EQUAL_UINT16(60, status.autoCheckIntervalMinutes);
}

void test_version_difference_detection_handles_first_install() {
  FirmwareUpgradeService service;
  service.begin();
  service._installedVersionCode = -1;

  FirmwareUpgradeService::FirmwarePackageInfo packageInfo;
  packageInfo.versionCode = 6;
  TEST_ASSERT_TRUE(service.isVersionDifferentFromInstalled(packageInfo));
}

void test_version_difference_detection_rejects_equal_version() {
  FirmwareUpgradeService service;
  service.begin();
  service._installedVersionCode = 6;

  FirmwareUpgradeService::FirmwarePackageInfo equalVersionPackage;
  equalVersionPackage.versionCode = 6;
  TEST_ASSERT_FALSE(service.isVersionDifferentFromInstalled(equalVersionPackage));
}

void test_version_difference_detection_allows_older_version_for_downgrade() {
  FirmwareUpgradeService service;
  service.begin();
  service._installedVersionCode = 6;

  FirmwareUpgradeService::FirmwarePackageInfo olderVersionPackage;
  olderVersionPackage.versionCode = 5;
  TEST_ASSERT_TRUE(service.isVersionDifferentFromInstalled(olderVersionPackage));
}

void test_version_difference_detection_allows_missing_remote_version_code() {
  FirmwareUpgradeService service;
  service.begin();
  service._installedVersionCode = 6;

  FirmwareUpgradeService::FirmwarePackageInfo packageInfo;
  packageInfo.versionCode = -1;
  TEST_ASSERT_TRUE(service.isVersionDifferentFromInstalled(packageInfo));
}

void setup() {
  Serial.begin(115200);
  delay(200);

  UNITY_BEGIN();
  RUN_TEST(test_begin_sets_hardcoded_uid_and_topic);
  RUN_TEST(test_update_config_cannot_override_hardcoded_uid_and_topic);
  RUN_TEST(test_lookup_url_uses_hardcoded_uid_and_topic);
  RUN_TEST(test_manual_upgrade_uses_hardcoded_config_and_checks_wifi_first);
  RUN_TEST(test_request_manual_upgrade_rejects_when_wifi_disconnected);
  RUN_TEST(test_request_manual_upgrade_accepts_valid_manual_trigger);
  RUN_TEST(test_request_manual_check_accepts_valid_manual_trigger);
  RUN_TEST(test_manual_check_and_upgrade_use_independent_rate_limit);
  RUN_TEST(test_manual_check_rate_limit_uses_check_error_code);
  RUN_TEST(test_manual_upgrade_rate_limit_uses_upgrade_error_code);
  RUN_TEST(test_parse_lookup_response_reads_version_code_from_v_field);
  RUN_TEST(test_parse_lookup_response_fails_when_v_field_missing);
  RUN_TEST(test_update_auto_check_config_forces_disabled);
  RUN_TEST(test_update_auto_check_config_forces_disabled_and_normalizes_invalid_interval);
  RUN_TEST(test_version_difference_detection_handles_first_install);
  RUN_TEST(test_version_difference_detection_rejects_equal_version);
  RUN_TEST(test_version_difference_detection_allows_older_version_for_downgrade);
  RUN_TEST(test_version_difference_detection_allows_missing_remote_version_code);
  UNITY_END();
}

void loop() {}
