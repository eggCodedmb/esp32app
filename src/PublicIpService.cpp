#include "PublicIpService.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <ctype.h>
#include <cstring>

namespace
{
  constexpr const char *const kPublicIpv4Urls[] = {
      "https://ip.3322.net/",
      "https://ifconfig.me/ip"};

  constexpr const char *kUserAgent = "ESP32-Public-IP/1.0";

  template <size_t N>
  constexpr size_t arrayLength(const char *const (&)[N])
  {
    return N;
  }

  uint32_t remainingTimeoutMs(uint32_t startMs, uint32_t timeoutMs)
  {
    if (timeoutMs == 0)
    {
      return 0;
    }

    const uint32_t elapsedMs = millis() - startMs;
    if (elapsedMs >= timeoutMs)
    {
      return 0;
    }
    return timeoutMs - elapsedMs;
  }

  using ResolveUrlFetch = String (*)(const char *url, uint32_t timeoutMs);

  String resolveFromUrlList(const char *const *urls,
                            size_t urlCount,
                            uint32_t startMs,
                            uint32_t timeoutMs,
                            ResolveUrlFetch fetch)
  {
    for (size_t i = 0; i < urlCount; ++i)
    {
      const uint32_t requestTimeoutMs = remainingTimeoutMs(startMs, timeoutMs);
      if (requestTimeoutMs == 0)
      {
        break;
      }

      const String ip = fetch(urls[i], requestTimeoutMs);
      if (!ip.isEmpty())
      {
        return ip;
      }
    }

    return "";
  }
}

String PublicIpService::resolveIpv4(bool useLocalIp, uint32_t timeoutMs)
{
  if (useLocalIp)
  {
    const String ipv4 = WiFi.localIP().toString();
    if (ipv4 == "0.0.0.0")
    {
      return "";
    }
    return ipv4;
  }

  return resolveFromUrlList(kPublicIpv4Urls,
                            arrayLength(kPublicIpv4Urls),
                            millis(),
                            timeoutMs,
                            &PublicIpService::fetchFromUrl);
}

String PublicIpService::resolve(bool useLocalIp, uint32_t timeoutMs)
{
  return resolveIpv4(useLocalIp, timeoutMs);
}

String PublicIpService::fetchFromUrl(const char *url, uint32_t timeoutMs)
{
  if (url == nullptr || timeoutMs == 0)
  {
    return "";
  }

  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  HTTPClient http;
  http.setConnectTimeout(timeoutMs);
  http.setTimeout(timeoutMs);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setUserAgent(kUserAgent);

  const bool useHttps = std::strncmp(url, "https://", 8) == 0;
  const bool begun = [&]()
  {
    if (useHttps)
    {
      secureClient.setInsecure();
      return http.begin(secureClient, url);
    }
    return http.begin(plainClient, url);
  }();

  if (!begun)
  {
    return "";
  }

  String ip = "";
  const int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK)
  {
    const String response = http.getString();
    ip = extractIpv4FromText(response);
  }

  http.end();
  return ip;
}

String PublicIpService::extractIpv4FromText(const String &response)
{
  String text = response;
  text.trim();
  if (text.isEmpty())
  {
    return "";
  }

  IPAddress parsed;
  if (parsed.fromString(text))
  {
    return text;
  }

  const int length = text.length();
  for (int i = 0; i < length; ++i)
  {
    if (!isdigit(static_cast<unsigned char>(text.charAt(i))))
    {
      continue;
    }

    int end = i;
    while (end < length)
    {
      const char c = text.charAt(end);
      if (!isdigit(static_cast<unsigned char>(c)) && c != '.')
      {
        break;
      }
      end += 1;
    }

    const String candidate = text.substring(i, end);
    if (candidate.length() >= 7 && candidate.length() <= 15 && parsed.fromString(candidate))
    {
      return candidate;
    }
  }

  return "";
}
