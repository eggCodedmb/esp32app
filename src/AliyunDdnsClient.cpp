#include "AliyunDdnsClient.h"
#include "PublicIpService.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <mbedtls/md.h>
#include <base64.h>
#include <time.h>
#include <ctype.h>

namespace {
constexpr const char* kAliyunDnsEndpoint = "https://alidns.aliyuncs.com/";
constexpr uint32_t kRequestTimeoutMs = 10000;
constexpr const char* kSignatureMethod = "HMAC-SHA1";
constexpr const char* kSignatureVersion = "1.0";
constexpr const char* kRecordType = "A";
constexpr const char* kUserAgent = "ESP32-Aliyun-DDNS/1.0";

}  // namespace

AliyunDdnsClient::AliyunDdnsClient() {}

void AliyunDdnsClient::begin(const String& accessKeyId, const String& accessKeySecret,
                             const String& domain, const String& subDomain) {
  _accessKeyId = accessKeyId;
  _accessKeySecret = accessKeySecret;
  _domain = domain;
  _subDomain = subDomain;
  _recordId = "";
  _recordIdValid = false;
  _begun = true;
}

void AliyunDdnsClient::update(uint32_t now, bool useLocalIp) {
  (void)now;
  if (!_begun) {
    return;
  }

  time_t currentTime = time(nullptr);
  if (currentTime < 1609459200) {
    return;
  }

  const String newIp = PublicIpService::resolve(useLocalIp, kRequestTimeoutMs / 2);
  if (newIp.isEmpty()) {
    return;
  }

  RecordInfo recordInfo;
  if (!fetchRecordInfo(&recordInfo)) {
    return;
  }

  if (!_recordIdValid && !recordInfo.recordId.isEmpty()) {
    _recordId = recordInfo.recordId;
    _recordIdValid = true;
  }
  if (recordInfo.value == newIp) {
    return;
  }
  const String oldIp = recordInfo.value;
  if (updateRecord(recordInfo.recordId, newIp)) {
    _updateCount++;
    _lastUpdateAtMs = millis();
    _lastOldIp = oldIp;
    _lastNewIp = newIp;

    if (!_recordIdValid) {
      _recordId = recordInfo.recordId;
      _recordIdValid = true;
    }
    if (_updateCallback) {
      _updateCallback(_lastOldIp.c_str(), _lastNewIp.c_str());
    }
  }
}

void AliyunDdnsClient::onUpdate(UpdateCallback callback) {
  _updateCallback = callback;
}

String AliyunDdnsClient::generateTimestamp() const {
  time_t now;
  time(&now);
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  
  char buffer[32];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(buffer);
}

String AliyunDdnsClient::generateNonce() const {
  char nonce[17];
  for (int i = 0; i < 16; i++) {
    nonce[i] = 'a' + random(26);
  }
  nonce[16] = '\0';
  return String(nonce);
}

String AliyunDdnsClient::urlEncode(const String& value) const {
  String encoded;
  encoded.reserve(value.length() * 3);
  
  for (size_t i = 0; i < value.length(); i++) {
    const char c = value.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += "%20";
    } else {
      encoded += '%';
      encoded += "0123456789ABCDEF"[((uint8_t)c) >> 4];
      encoded += "0123456789ABCDEF"[((uint8_t)c) & 0x0F];
    }
  }
  
  return encoded;
}

String AliyunDdnsClient::sha1Hmac(const String& key, const String& data) const {
  uint8_t hmacResult[20];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)key.c_str(), key.length());
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)data.c_str(), data.length());
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);
  
  return base64Encode(hmacResult, sizeof(hmacResult));
}

String AliyunDdnsClient::base64Encode(const uint8_t* data, size_t length) const {
  return base64::encode(data, length);
}

String AliyunDdnsClient::calculateSignature(const String& method, const String& sortedParams) const {
  const String stringToSign = method + "&" + urlEncode("/") + "&" + urlEncode(sortedParams);
  const String key = _accessKeySecret + "&";
  return sha1Hmac(key, stringToSign);
}

bool AliyunDdnsClient::fetchRecordInfo(RecordInfo* info) {
  if (info == nullptr) {
    return false;
  }

  auto buildSignedParams = [this](const String& rawParams) {
    std::vector<String> pairs;
    int start = 0;
    while (start < rawParams.length()) {
      int end = rawParams.indexOf('&', start);
      if (end == -1) {
        end = rawParams.length();
      }
      pairs.push_back(rawParams.substring(start, end));
      start = end + 1;
    }

    std::sort(pairs.begin(), pairs.end());

    String sorted;
    for (size_t i = 0; i < pairs.size(); ++i) {
      if (i > 0) {
        sorted += "&";
      }
      sorted += pairs[i];
    }

    const String signature = calculateSignature("GET", sorted);
    sorted += "&Signature=" + urlEncode(signature);
    return sorted;
  };

  auto queryDescribe = [this](const String& signedParams, String* response) {
    if (response == nullptr) {
      return false;
    }

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setConnectTimeout(kRequestTimeoutMs);
    http.setTimeout(kRequestTimeoutMs);
    http.setUserAgent(kUserAgent);

    const String url = String(kAliyunDnsEndpoint) + "?" + signedParams;
    if (!http.begin(client, url)) {
      return false;
    }

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
      http.end();
      return false;
    }

    *response = http.getString();
    http.end();
    return true;
  };

  auto parseValue = [](const String& response, String* value) {
    if (value == nullptr) {
      return false;
    }

    int valueStart = response.indexOf("\"Value\":\"");
    if (valueStart == -1) {
      return false;
    }

    valueStart += 9;
    const int valueEnd = response.indexOf("\"", valueStart);
    if (valueEnd == -1) {
      return false;
    }

    *value = response.substring(valueStart, valueEnd);
    return !value->isEmpty();
  };

  String params = "Action=DescribeDomainRecords";
  params += "&DomainName=" + urlEncode(_domain);
  params += "&RRKeyWord=" + urlEncode(_subDomain);
  params += "&TypeKeyWord=" + String(kRecordType);
  params += "&AccessKeyId=" + urlEncode(_accessKeyId);
  params += "&Format=JSON";
  params += "&SignatureMethod=" + String(kSignatureMethod);
  params += "&SignatureNonce=" + generateNonce();
  params += "&SignatureVersion=" + String(kSignatureVersion);
  params += "&Timestamp=" + urlEncode(generateTimestamp());
  params += "&Version=2015-01-09";

  String response;
  if (!queryDescribe(buildSignedParams(params), &response)) {
    return false;
  }

  if (_recordIdValid && !_recordId.isEmpty()) {
    info->recordId = _recordId;
    return parseValue(response, &info->value);
  }

  int recordIdStart = response.indexOf("\"RecordId\":\"");
  if (recordIdStart != -1) {
    recordIdStart += 12;
    const int recordIdEnd = response.indexOf("\"", recordIdStart);
    if (recordIdEnd != -1) {
      info->recordId = response.substring(recordIdStart, recordIdEnd);
      _recordId = info->recordId;
      _recordIdValid = !info->recordId.isEmpty();
    }
  }

  if (!parseValue(response, &info->value)) {
    return false;
  }

  return !info->recordId.isEmpty() && !info->value.isEmpty();
}

bool AliyunDdnsClient::updateRecord(const String& recordId, const String& newIp) {
  if (recordId.isEmpty() || newIp.isEmpty()) {
    return false;
  }
  
  String params = "Action=UpdateDomainRecord";
  params += "&RecordId=" + urlEncode(recordId);
  params += "&RR=" + urlEncode(_subDomain);
  params += "&Type=";
  params += kRecordType;
  params += "&Value=" + urlEncode(newIp);
  params += "&AccessKeyId=" + urlEncode(_accessKeyId);
  params += "&Format=JSON";
  params += "&SignatureMethod=";
  params += kSignatureMethod;
  params += "&SignatureNonce=" + generateNonce();
  params += "&SignatureVersion=";
  params += kSignatureVersion;
  params += "&Timestamp=" + urlEncode(generateTimestamp());
  params += "&Version=2015-01-09";
  
  // 鍙傛暟鎺掑簭
  std::vector<String> paramPairs;
  int start = 0;
  while (start < params.length()) {
    int end = params.indexOf('&', start);
    if (end == -1) {
      end = params.length();
    }
    paramPairs.push_back(params.substring(start, end));
    start = end + 1;
  }
  
  std::sort(paramPairs.begin(), paramPairs.end());
  
  String sortedParams;
  for (size_t i = 0; i < paramPairs.size(); i++) {
    if (i > 0) {
      sortedParams += "&";
    }
    sortedParams += paramPairs[i];
  }
  
  const String signature = calculateSignature("POST", sortedParams);
  sortedParams += "&Signature=" + urlEncode(signature);
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(kRequestTimeoutMs);
  http.setTimeout(kRequestTimeoutMs);
  http.setUserAgent(kUserAgent);
  
  const String url = String(kAliyunDnsEndpoint);
  if (!http.begin(client, url)) {
    return false;
  }
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  bool success = false;
  const int httpCode = http.POST(sortedParams);
  String response;
  if (httpCode > 0) {
    response = http.getString();
  }
  if (httpCode == HTTP_CODE_OK) {
    success = response.indexOf("\"RecordId\":\"") != -1;
  }
  
  http.end();
  return success;
}



