#include <Arduino.h>
#include <Preferences.h>
#include <unity.h>

#include "AuthService.h"

namespace {
constexpr const char* kAuthNamespace = "esp32auth";

void clearAuthNamespace() {
  Preferences preferences;
  const bool opened = preferences.begin(kAuthNamespace, false);
  TEST_ASSERT_TRUE(opened);
  if (opened) {
    preferences.clear();
    preferences.end();
  }
}
}  // namespace

void setUp() {
  clearAuthNamespace();
}

void tearDown() {}

void test_validate_credentials_with_default_password() {
  AuthService auth("admin", "admin123");
  auth.begin();

  TEST_ASSERT_TRUE(auth.validateCredentials("admin", "admin123"));
  TEST_ASSERT_FALSE(auth.validateCredentials("admin", "wrong"));
  TEST_ASSERT_FALSE(auth.validateCredentials("other", "admin123"));
}

void test_update_password_rejects_invalid_inputs() {
  AuthService auth("admin", "admin123");
  auth.begin();

  String error;
  TEST_ASSERT_FALSE(auth.updatePassword("bad-old", "newpass1", &error));
  TEST_ASSERT_EQUAL_STRING("current_password_incorrect", error.c_str());

  TEST_ASSERT_FALSE(auth.updatePassword("admin123", "123", &error));
  TEST_ASSERT_EQUAL_STRING("password_too_short", error.c_str());

  TEST_ASSERT_FALSE(auth.updatePassword("admin123", "admin123", &error));
  TEST_ASSERT_EQUAL_STRING("password_not_changed", error.c_str());
}

void test_update_password_persists_for_new_instance() {
  AuthService auth("admin", "admin123");
  auth.begin();

  String error;
  TEST_ASSERT_TRUE(auth.updatePassword("admin123", "newPass123", &error));
  TEST_ASSERT_EQUAL_STRING("", error.c_str());
  TEST_ASSERT_TRUE(auth.validateCredentials("admin", "newPass123"));
  TEST_ASSERT_FALSE(auth.validateCredentials("admin", "admin123"));

  AuthService reloaded("admin", "admin123");
  reloaded.begin();
  TEST_ASSERT_TRUE(reloaded.validateCredentials("admin", "newPass123"));
  TEST_ASSERT_FALSE(reloaded.validateCredentials("admin", "admin123"));
}

void test_issue_session_token_has_expected_length() {
  AuthService auth("admin", "admin123");
  const String token = auth.issueSessionToken();
  TEST_ASSERT_EQUAL_UINT32(32, token.length());
  TEST_ASSERT_TRUE(auth.sessionTtlSeconds() > 0);
  TEST_ASSERT_FALSE(auth.isAuthorized(nullptr));
}

void setup() {
  Serial.begin(115200);
  delay(200);

  UNITY_BEGIN();
  RUN_TEST(test_validate_credentials_with_default_password);
  RUN_TEST(test_update_password_rejects_invalid_inputs);
  RUN_TEST(test_update_password_persists_for_new_instance);
  RUN_TEST(test_issue_session_token_has_expected_length);
  UNITY_END();
}

void loop() {}
