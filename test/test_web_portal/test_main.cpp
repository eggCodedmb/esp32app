#include <Arduino.h>
#include <unity.h>

#include "AuthService.h"
#include "BemfaService.h"
#include "ConfigStore.h"
#include "DdnsService.h"
#include "FirmwareUpgradeService.h"
#include "HostProbeService.h"
#include "PowerOnService.h"
#include "WakeOnLanService.h"
#include "WebPortal.h"
#include "WifiService.h"
#include "TimeService.h"

namespace {
AuthService auth("admin", "admin123");
WifiService wifi;
ConfigStore config;
WakeOnLanService wol;
HostProbeService probe;
PowerOnService powerOnService(wol, probe);
BemfaService bemfaService;
TimeService timeService;
DdnsService ddnsService(config);
FirmwareUpgradeService firmwareUpgradeService;
WebPortal portal(8080,
                 auth,
                 wifi,
                 config,
                 powerOnService,
                 bemfaService,
                 ddnsService,
                 timeService,
                 firmwareUpgradeService);
}  // namespace

void setUp() {}

void tearDown() {}

void test_parse_bool_value_variants() {
  TEST_ASSERT_TRUE(WebPortal::testParseBoolValue("true", false));
  TEST_ASSERT_TRUE(WebPortal::testParseBoolValue("1", false));
  TEST_ASSERT_FALSE(WebPortal::testParseBoolValue("false", true));
  TEST_ASSERT_FALSE(WebPortal::testParseBoolValue("0", true));
  TEST_ASSERT_TRUE(WebPortal::testParseBoolValue("unknown", true));
  TEST_ASSERT_FALSE(WebPortal::testParseBoolValue("unknown", false));
}

void test_json_escape_handles_quotes_and_newline() {
  const String escaped = WebPortal::testJsonEscape("A\"B\nC\\D");
  TEST_ASSERT_EQUAL_STRING("A\\\"B\\nC\\\\D", escaped.c_str());
}

void test_login_page_contains_expected_elements() {
  const String page = portal.testLoginPage("err");

  TEST_ASSERT_TRUE(page.indexOf("<form method=\"post\" action=\"/login\">") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("admin / admin123") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("err") >= 0);
}

void test_dashboard_page_contains_config_and_password_sections() {
  const String page = portal.testDashboardPage();

  TEST_ASSERT_TRUE(page.indexOf("computerMac") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("/api/auth/password") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("/api/power/on") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("/api/power/status") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("/api/system/info") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("/api/bemfa/status") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("/api/ddns/status") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("/api/ota/status") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("/api/ota/check") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("/api/ota/upgrade") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("/api/ota/manual") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("bemfaForm") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("bemfaTopic") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("ddnsForm") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("ddnsEnabled") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("ddnsAddRecordButton") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("ddnsToggleAllButton") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("ddnsRecords") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("ddns-host-type") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("ddns-record-type") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("ddns-collapse-toggle") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("ddns-record-summary") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("ddns-record-details") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("ddns-ttl") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("ddnsAliyunAccount") < 0);
  TEST_ASSERT_TRUE(page.indexOf("ddnsAliyunRecords") < 0);
  TEST_ASSERT_TRUE(page.indexOf("ddns-ipv6") < 0);
  TEST_ASSERT_TRUE(page.indexOf("AAAA") < 0);
  TEST_ASSERT_TRUE(page.indexOf("useIpv6") < 0);
  TEST_ASSERT_TRUE(page.indexOf("otaCheckButton") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("otaUpgradeButton") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("otaProgress") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("otaCurrentVersion") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("otaTrigger") < 0);
  TEST_ASSERT_TRUE(page.indexOf("otaAutoNext") < 0);
  TEST_ASSERT_TRUE(page.indexOf("otaAutoCheckEnabled") < 0);
  TEST_ASSERT_TRUE(page.indexOf("otaAutoCheckIntervalMinutes") < 0);
  TEST_ASSERT_TRUE(page.indexOf("systemForm") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("statusPollIntervalMinutes") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("refreshAllButton") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("espUptime") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("espFlashFree") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("passwordForm") >= 0);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  UNITY_BEGIN();
  RUN_TEST(test_parse_bool_value_variants);
  RUN_TEST(test_json_escape_handles_quotes_and_newline);
  RUN_TEST(test_login_page_contains_expected_elements);
  RUN_TEST(test_dashboard_page_contains_config_and_password_sections);
  UNITY_END();
}

void loop() {}
