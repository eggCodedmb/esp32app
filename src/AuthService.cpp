#include "AuthService.h"

#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <esp_system.h>
#include <time.h>

namespace {
constexpr const char* kAuthNamespace = "esp32auth";
constexpr const char* kPasswordKey = "pwd_plain";
constexpr const char* kSessionTokenKey = "sess_tok";
constexpr const char* kSessionExpiryUnixKey = "sess_exp_unix";
constexpr uint32_t kSessionTtlSeconds = 60UL * 60UL * 24UL * 7UL;
constexpr uint32_t kMinValidUnixTime = 1700000000UL;  // 2023-11-14 22:13:20 UTC

String extractCookieValue(const String& cookieHeader, const String& key) {
  const String needle = key + "=";
  int start = cookieHeader.indexOf(needle);
  if (start < 0) {
    return "";
  }

  start += needle.length();
  int end = cookieHeader.indexOf(';', start);
  if (end < 0) {
    end = cookieHeader.length();
  }

  String value = cookieHeader.substring(start, end);
  value.trim();
  if (value.length() >= 2 && value.charAt(0) == '\"' &&
      value.charAt(value.length() - 1) == '\"') {
    value = value.substring(1, value.length() - 1);
  }
  return value;
}

bool isSessionExpired(uint32_t expiryMs) {
  if (expiryMs == 0) {
    return true;
  }
  return static_cast<int32_t>(millis() - expiryMs) >= 0;
}

uint32_t currentUnixTime() {
  const time_t now = time(nullptr);
  if (now <= 0) {
    return 0;
  }
  return static_cast<uint32_t>(now);
}

bool isUnixTimeValid(uint32_t unixTime) {
  return unixTime >= kMinValidUnixTime;
}
}  // namespace

AuthService::AuthService(const String& username, const String& password)
    : _username(username), _password(password), _defaultPassword(_password) {}

void AuthService::begin() {
  if (!loadStoredPassword()) {
    _password = _defaultPassword;
    persistPassword(_password);
  }

  loadStoredSession();
}

bool AuthService::validateCredentials(const String& username, const String& password) const {
  return username == _username && password == _password;
}

bool AuthService::verifyCurrentPassword(const String& password) const {
  return password == _password;
}

bool AuthService::updatePassword(const String& currentPassword,
                                 const String& newPassword,
                                 String* errorCode) {
  if (!verifyCurrentPassword(currentPassword)) {
    if (errorCode != nullptr) {
      *errorCode = "current_password_incorrect";
    }
    return false;
  }

  if (newPassword.length() < 6) {
    if (errorCode != nullptr) {
      *errorCode = "password_too_short";
    }
    return false;
  }

  if (newPassword == currentPassword) {
    if (errorCode != nullptr) {
      *errorCode = "password_not_changed";
    }
    return false;
  }

  if (!persistPassword(newPassword)) {
    if (errorCode != nullptr) {
      *errorCode = "persist_failed";
    }
    return false;
  }

  _password = newPassword;
  clearSession();
  if (errorCode != nullptr) {
    *errorCode = "";
  }
  return true;
}

String AuthService::issueSessionToken() {
  _activeToken = randomHexToken(32);
  _sessionExpiryMs = millis() + (sessionTtlSeconds() * 1000UL);
  _sessionExpiryUnix = 0;

  const uint32_t nowUnix = currentUnixTime();
  if (isUnixTimeValid(nowUnix)) {
    _sessionExpiryUnix = nowUnix + sessionTtlSeconds();
  }

  persistSession(_activeToken, _sessionExpiryUnix);
  return _activeToken;
}

uint32_t AuthService::sessionTtlSeconds() const {
  return kSessionTtlSeconds;
}

bool AuthService::isAuthorized(const AsyncWebServerRequest* request) const {
  if (_activeToken.isEmpty() || request == nullptr || !request->hasHeader("Cookie")) {
    return false;
  }

  if (_sessionExpiryUnix > 0) {
    const uint32_t nowUnix = currentUnixTime();
    if (isUnixTimeValid(nowUnix) && nowUnix >= _sessionExpiryUnix) {
      return false;
    }
  }

  if (isSessionExpired(_sessionExpiryMs)) {
    return false;
  }

  const AsyncWebHeader* cookieHeader = request->getHeader("Cookie");
  if (cookieHeader == nullptr) {
    return false;
  }

  const String token = extractCookieValue(cookieHeader->value(), "ESPSESSION");
  return !token.isEmpty() && token == _activeToken;
}

void AuthService::clearSession() {
  _activeToken = "";
  _sessionExpiryMs = 0;
  _sessionExpiryUnix = 0;

  Preferences preferences;
  if (!preferences.begin(kAuthNamespace, false)) {
    return;
  }

  preferences.remove(kSessionTokenKey);
  preferences.remove(kSessionExpiryUnixKey);
  preferences.end();
}

bool AuthService::loadStoredPassword() {
  Preferences preferences;
  if (!preferences.begin(kAuthNamespace, true)) {
    return false;
  }

  const String storedPassword = preferences.getString(kPasswordKey, "");
  preferences.end();

  if (storedPassword.isEmpty()) {
    return false;
  }

  _password = storedPassword;
  return true;
}

bool AuthService::persistPassword(const String& password) const {
  Preferences preferences;
  if (!preferences.begin(kAuthNamespace, false)) {
    return false;
  }

  preferences.putString(kPasswordKey, password);
  preferences.end();
  return true;
}

bool AuthService::loadStoredSession() {
  Preferences preferences;
  if (!preferences.begin(kAuthNamespace, true)) {
    return false;
  }

  const String storedToken = preferences.getString(kSessionTokenKey, "");
  const uint32_t storedExpiryUnix = preferences.getULong(kSessionExpiryUnixKey, 0);
  preferences.end();

  if (storedToken.isEmpty()) {
    _activeToken = "";
    _sessionExpiryMs = 0;
    _sessionExpiryUnix = 0;
    return false;
  }

  _activeToken = storedToken;
  _sessionExpiryUnix = storedExpiryUnix;
  _sessionExpiryMs = millis() + (sessionTtlSeconds() * 1000UL);

  const uint32_t nowUnix = currentUnixTime();
  if (_sessionExpiryUnix > 0 && isUnixTimeValid(nowUnix)) {
    if (nowUnix >= _sessionExpiryUnix) {
      clearSession();
      return false;
    }
    const uint32_t remainingSeconds = _sessionExpiryUnix - nowUnix;
    _sessionExpiryMs = millis() + (remainingSeconds * 1000UL);
  }

  return true;
}

bool AuthService::persistSession(const String& token, uint32_t expiryUnix) const {
  Preferences preferences;
  if (!preferences.begin(kAuthNamespace, false)) {
    return false;
  }

  preferences.putString(kSessionTokenKey, token);
  preferences.putULong(kSessionExpiryUnixKey, expiryUnix);
  preferences.end();
  return true;
}

String AuthService::randomHexToken(size_t length) {
  static const char kHex[] = "0123456789abcdef";
  String token;
  token.reserve(length);

  while (token.length() < length) {
    uint32_t randomValue = esp_random();
    for (int i = 0; i < 8 && token.length() < length; ++i) {
      token += kHex[(randomValue >> (i * 4)) & 0x0F];
    }
  }

  return token;
}
