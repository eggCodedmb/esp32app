#include <Arduino.h>
#include <unity.h>

#include "AuthService.h"
#include "BemfaService.h"
#include "ConfigStore.h"
#include "HostProbeService.h"
#include "PowerOnService.h"
#include "WakeOnLanService.h"
#include "WebPortal.h"
#include "WifiService.h"

namespace {
AuthService auth("admin", "admin123");
WifiService wifi;
ConfigStore config;
WakeOnLanService wol;
HostProbeService probe;
PowerOnService powerOnService(wol, probe);
BemfaService bemfaService;
WebPortal portal(80, auth, wifi, config, powerOnService, bemfaService);
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
  TEST_ASSERT_TRUE(page.indexOf("bemfaForm") >= 0);
  TEST_ASSERT_TRUE(page.indexOf("bemfaTopic") >= 0);
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
