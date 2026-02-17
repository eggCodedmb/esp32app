#pragma once

#include <Arduino.h>

class AsyncWebServerRequest;

class AuthService {
 public:
  AuthService(const String& username, const String& password);

  void begin();
  bool validateCredentials(const String& username, const String& password) const;
  bool verifyCurrentPassword(const String& password) const;
  bool updatePassword(const String& currentPassword, const String& newPassword, String* errorCode);
  String issueSessionToken();
  uint32_t sessionTtlSeconds() const;
  bool isAuthorized(const AsyncWebServerRequest* request) const;
  void clearSession();

 private:
  String _username;
  String _password;
  String _defaultPassword;
  String _activeToken;
  uint32_t _sessionExpiryMs = 0;
  uint32_t _sessionExpiryUnix = 0;

  bool loadStoredPassword();
  bool loadStoredSession();
  bool persistPassword(const String& password) const;
  bool persistSession(const String& token, uint32_t expiryUnix) const;
  static String randomHexToken(size_t length);
};
