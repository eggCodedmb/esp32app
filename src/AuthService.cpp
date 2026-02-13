#include "AuthService.h"

#include <ESPAsyncWebServer.h>
#include <esp_system.h>

namespace {
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

  return cookieHeader.substring(start, end);
}
}  // namespace

AuthService::AuthService(const String& username, const String& password)
    : _username(username), _password(password) {}

bool AuthService::validateCredentials(const String& username, const String& password) const {
  return username == _username && password == _password;
}

String AuthService::issueSessionToken() {
  _activeToken = randomHexToken(32);
  return _activeToken;
}

bool AuthService::isAuthorized(const AsyncWebServerRequest* request) const {
  if (_activeToken.isEmpty() || request == nullptr || !request->hasHeader("Cookie")) {
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
