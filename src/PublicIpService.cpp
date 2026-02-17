#include "PublicIpService.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <ctype.h>
#include <cstring>

namespace
{
  constexpr const char *kPublicIpv4Urls[] = {
      "https://ip.3322.net/",
      "http://ip.3322.net/",
      "https://ifconfig.me/ip",
      "http://ifconfig.me/ip"};

  constexpr const char *kPublicIpv6Urls[] = {
      "https://ipv6.ip.mir6.com/"};

  constexpr const char *kUserAgent = "ESP32-Public-IP/1.0";

  bool isIpv6TokenChar(char c)
  {
    return isxdigit(static_cast<unsigned char>(c)) || c == ':' || c == '.';
  }

  bool isLikelyIpv6(const String &value)
  {
    if (value.length() < 2 || value.length() > 45)
    {
      return false;
    }
    if (value.indexOf(":::") >= 0)
    {
      return false;
    }

    int colonCount = 0;
    for (int i = 0; i < value.length(); ++i)
    {
      const char c = value.charAt(i);
      if (!isIpv6TokenChar(c))
      {
        return false;
      }
      if (c == ':')
      {
        colonCount += 1;
      }
    }

    if (colonCount < 2)
    {
      return false;
    }
    if (value.startsWith(":") && !value.startsWith("::"))
    {
      return false;
    }
    if (value.endsWith(":") && !value.endsWith("::"))
    {
      return false;
    }
    return true;
  }

  bool isUsableLocalIpv6(const String &value)
  {
    if (!isLikelyIpv6(value))
    {
      return false;
    }

    String normalized = value;
    normalized.toLowerCase();
    if (normalized == "::" || normalized == "0:0:0:0:0:0:0:0")
    {
      return false;
    }
    if (normalized.startsWith("fe80:"))
    {
      // Link-local address is not suitable for public DDNS records.
      return false;
    }
    return true;
  }

  String extractJsonStringField(const String &text, const char *key)
  {
    if (key == nullptr)
    {
      return "";
    }

    int keyPos = text.indexOf(key);
    while (keyPos >= 0)
    {
      const int colonPos = text.indexOf(':', keyPos + static_cast<int>(std::strlen(key)));
      if (colonPos < 0)
      {
        return "";
      }

      int valueStart = colonPos + 1;
      while (valueStart < text.length() &&
             isspace(static_cast<unsigned char>(text.charAt(valueStart))))
      {
        valueStart += 1;
      }

      if (valueStart < text.length() && text.charAt(valueStart) == '"')
      {
        valueStart += 1;
        int valueEnd = valueStart;
        while (valueEnd < text.length())
        {
          const char c = text.charAt(valueEnd);
          if (c == '\\')
          {
            valueEnd += 2;
            continue;
          }
          if (c == '"')
          {
            break;
          }
          valueEnd += 1;
        }

        if (valueEnd < text.length() && text.charAt(valueEnd) == '"')
        {
          return text.substring(valueStart, valueEnd);
        }
        return "";
      }

      keyPos = text.indexOf(key, keyPos + static_cast<int>(std::strlen(key)));
    }

    return "";
  }
} // namespace

String PublicIpService::resolve(bool useLocalIp, bool useIpv6, uint32_t timeoutMs)
{
  if (useLocalIp)
  {
    if (useIpv6)
    {
      WiFi.enableIpV6();
      String ipv6 = WiFi.localIPv6().toString();
      ipv6.trim();
      if (!isUsableLocalIpv6(ipv6))
      {
        return "";
      }
      return ipv6;
    }

    const String ipv4 = WiFi.localIP().toString();
    if (ipv4 == "0.0.0.0")
    {
      return "";
    }
    return ipv4;
  }

  const char *const *urls = useIpv6 ? kPublicIpv6Urls : kPublicIpv4Urls;
  const size_t urlCount =
      useIpv6 ? (sizeof(kPublicIpv6Urls) / sizeof(kPublicIpv6Urls[0]))
              : (sizeof(kPublicIpv4Urls) / sizeof(kPublicIpv4Urls[0]));
  for (size_t i = 0; i < urlCount; ++i)
  {
    const String ip = fetchFromUrl(urls[i], useIpv6, timeoutMs);
    if (!ip.isEmpty())
    {
      return ip;
    }
  }

  return "";
}

String PublicIpService::fetchFromUrl(const char *url, bool useIpv6, uint32_t timeoutMs)
{
  if (url == nullptr)
  {
    return "";
  }
  Serial.print(url);

  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  HTTPClient http;
  http.setConnectTimeout(timeoutMs);
  http.setTimeout(timeoutMs);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setUserAgent(kUserAgent);

  bool begun = false;
  const bool useHttps = std::strncmp(url, "https://", 8) == 0;
  if (useHttps)
  {
    secureClient.setInsecure();
    begun = http.begin(secureClient, url);
  }
  else
  {
    begun = http.begin(plainClient, url);
  }

  if (!begun)
  {
    return "";
  }

  String ip = "";
  const int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK)
  {
    const String response = http.getString();
    ip = useIpv6 ? extractIpv6FromText(response) : extractIpv4FromText(response);
  }

  http.end();
  Serial.print(httpCode);
  Serial.print(ip);
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

String PublicIpService::extractIpv6FromText(const String &response)
{
  String text = response;
  text.trim();
  if (text.isEmpty())
  {
    return "";
  }
  if (isLikelyIpv6(text))
  {
    return text;
  }
  return "";
}
