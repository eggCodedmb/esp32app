#include "PublicIpService.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <ctype.h>
#include <cstring>

namespace {
constexpr const char* kPublicIpUrls[] = {
    "https://ip.3322.net/",
    "http://ip.3322.net/",
    "https://ifconfig.me/ip",
    "http://ifconfig.me/ip"};

constexpr const char* kUserAgent = "ESP32-Public-IP/1.0";
}  // namespace

String PublicIpService::resolve(bool useLocalIp, uint32_t timeoutMs) {
  if (useLocalIp) {
    const String ip = WiFi.localIP().toString();
    if (ip == "0.0.0.0") {
      return "";
    }
    return ip;
  }

  for (size_t i = 0; i < sizeof(kPublicIpUrls) / sizeof(kPublicIpUrls[0]); ++i) {
    const String ip = fetchFromUrl(kPublicIpUrls[i], timeoutMs);
    if (!ip.isEmpty()) {
      return ip;
    }
  }

  return "";
}

String PublicIpService::fetchFromUrl(const char* url, uint32_t timeoutMs) {
  if (url == nullptr) {
    return "";
  }

  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  HTTPClient http;
  http.setConnectTimeout(timeoutMs);
  http.setTimeout(timeoutMs);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setUserAgent(kUserAgent);

  bool begun = false;
  const bool useHttps = std::strncmp(url, "https://", 8) == 0;
  if (useHttps) {
    secureClient.setInsecure();
    begun = http.begin(secureClient, url);
  } else {
    begun = http.begin(plainClient, url);
  }

  if (!begun) {
    return "";
  }

  String ip = "";
  const int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    ip = extractIpv4FromText(http.getString());
  }

  http.end();
  return ip;
}

String PublicIpService::extractIpv4FromText(const String& response) {
  String text = response;
  text.trim();
  if (text.isEmpty()) {
    return "";
  }

  IPAddress parsed;
  if (parsed.fromString(text)) {
    return text;
  }

  const int length = text.length();
  for (int i = 0; i < length; ++i) {
    if (!isdigit(static_cast<unsigned char>(text.charAt(i)))) {
      continue;
    }

    int end = i;
    while (end < length) {
      const char c = text.charAt(end);
      if (!isdigit(static_cast<unsigned char>(c)) && c != '.') {
        break;
      }
      end += 1;
    }

    const String candidate = text.substring(i, end);
    if (candidate.length() >= 7 && candidate.length() <= 15 && parsed.fromString(candidate)) {
      return candidate;
    }
  }

  return "";
}
