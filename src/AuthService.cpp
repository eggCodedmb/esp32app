#include "AuthService.h"

#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <esp_system.h>

namespace {
constexpr const char* kAuthNamespace = "esp32auth";
constexpr const char* kPasswordKey = "pwd_plain";
constexpr uint32_t kSessionTtlSeconds = 60UL * 60UL * 24UL * 7UL;

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
}  // namespace

AuthService::AuthService(const String& username, const String& password)
    : _username(username), _password(password), _defaultPassword(_password) {}

void AuthService::begin() {
  if (!loadStoredPassword()) {
    _password = _defaultPassword;
    persistPassword(_password);
  }
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
  return _activeToken;
}

uint32_t AuthService::sessionTtlSeconds() const {
  return kSessionTtlSeconds;
}

bool AuthService::isAuthorized(const AsyncWebServerRequest* request) const {
  if (_activeToken.isEmpty() || isSessionExpired(_sessionExpiryMs) || request == nullptr ||
      !request->hasHeader("Cookie")) {
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
