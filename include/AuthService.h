#pragma once

#include <Arduino.h>

class AsyncWebServerRequest;

class AuthService {
 public:
  AuthService(const String& username, const String& password);

  bool validateCredentials(const String& username, const String& password) const;
  String issueSessionToken();
  bool isAuthorized(const AsyncWebServerRequest* request) const;
  void clearSession();

 private:
  String _username;
  String _password;
  String _activeToken;

  static String randomHexToken(size_t length);
};
