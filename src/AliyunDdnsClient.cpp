#include "AliyunDdnsClient.h"

#include "PublicIpService.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <algorithm>
#include <base64.h>
#include <ctype.h>
#include <mbedtls/md.h>
#include <time.h>
#include <vector>

namespace {
constexpr const char* kAliyunDnsEndpoint = "https://alidns.aliyuncs.com/";
constexpr uint32_t kRequestTimeoutMs = 10000;
constexpr const char* kSignatureMethod = "HMAC-SHA1";
constexpr const char* kSignatureVersion = "1.0";
constexpr const char* kApiVersion = "2015-01-09";
constexpr const char* kRecordTypeIpv4 = "A";
constexpr const char* kRecordTypeIpv6 = "AAAA";
constexpr const char* kUserAgent = "ESP32-Aliyun-DDNS/1.0";
}  // namespace

AliyunDdnsClient::AliyunDdnsClient() {}

void AliyunDdnsClient::begin(const String& accessKeyId,
                             const String& accessKeySecret,
                             const String& domain,
                             const String& subDomain,
                             bool useIpv6) {
  _accessKeyId = accessKeyId;
  _accessKeySecret = accessKeySecret;
  _domain = domain;
  _subDomain = subDomain;
  _useIpv6 = useIpv6;
  _recordId = "";
  _recordIdValid = false;
  _lastApiResponse = "";
  _lastCreatedRecordId = "";
  _begun = true;
}

void AliyunDdnsClient::update(uint32_t now, bool useLocalIp) {
  (void)now;
  if (!_begun) {
    return;
  }

  const time_t currentTime = time(nullptr);
  if (currentTime < 1609459200) {
    return;
  }

  const String newIp = PublicIpService::resolve(useLocalIp, _useIpv6, kRequestTimeoutMs / 2);
  if (newIp.isEmpty()) {
    return;
  }

  RecordInfo recordInfo;
  if (!fetchRecordInfo(&recordInfo)) {
    return;
  }

  if (recordInfo.value == newIp) {
    return;
  }

  const String oldIp = recordInfo.value;
  if (!updateRecord(recordInfo.recordId, newIp)) {
    return;
  }

  _updateCount++;
  _lastUpdateAtMs = millis();
  _lastOldIp = oldIp;
  _lastNewIp = newIp;
  _recordId = recordInfo.recordId;
  _recordIdValid = !_recordId.isEmpty();

  if (_updateCallback) {
    _updateCallback(_lastOldIp.c_str(), _lastNewIp.c_str());
  }
}

void AliyunDdnsClient::onUpdate(UpdateCallback callback) {
  _updateCallback = callback;
}

bool AliyunDdnsClient::describeDomainRecords(const String& domainName) {
  if (domainName.isEmpty()) {
    return false;
  }

  String params = buildCommonParams("DescribeDomainRecords");
  params += "&DomainName=" + urlEncode(domainName);

  _lastApiResponse = "";
  if (!invokeApi("GET", params, &_lastApiResponse)) {
    return false;
  }
  return _lastApiResponse.indexOf("\"DomainRecords\"") != -1;
}

bool AliyunDdnsClient::describeDomainRecordInfo(const String& recordId,
                                                String* rr,
                                                String* type,
                                                String* value) {
  if (recordId.isEmpty() || value == nullptr) {
    return false;
  }

  String params = buildCommonParams("DescribeDomainRecordInfo");
  params += "&RecordId=" + urlEncode(recordId);

  _lastApiResponse = "";
  if (!invokeApi("GET", params, &_lastApiResponse)) {
    return false;
  }

  String parsedValue;
  if (!parseJsonStringField(_lastApiResponse, "Value", &parsedValue) || parsedValue.isEmpty()) {
    return false;
  }

  if (rr != nullptr) {
    String parsedRr;
    if (!parseJsonStringField(_lastApiResponse, "RR", &parsedRr)) {
      parsedRr = "";
    }
    *rr = parsedRr;
  }

  if (type != nullptr) {
    String parsedType;
    if (!parseJsonStringField(_lastApiResponse, "Type", &parsedType)) {
      parsedType = "";
    }
    *type = parsedType;
  }

  *value = parsedValue;
  return true;
}

bool AliyunDdnsClient::addDomainRecord(const String& domainName,
                                       const String& rr,
                                       const String& type,
                                       const String& value) {
  if (domainName.isEmpty() || rr.isEmpty() || type.isEmpty() || value.isEmpty()) {
    return false;
  }

  String params = buildCommonParams("AddDomainRecord");
  params += "&DomainName=" + urlEncode(domainName);
  params += "&RR=" + urlEncode(rr);
  params += "&Type=" + urlEncode(type);
  params += "&Value=" + urlEncode(value);

  _lastApiResponse = "";
  if (!invokeApi("POST", params, &_lastApiResponse)) {
    return false;
  }

  String createdRecordId;
  if (!parseJsonStringField(_lastApiResponse, "RecordId", &createdRecordId) ||
      createdRecordId.isEmpty()) {
    return false;
  }

  _lastCreatedRecordId = createdRecordId;
  _recordId = createdRecordId;
  _recordIdValid = true;
  return true;
}

bool AliyunDdnsClient::updateDomainRecord(const String& recordId,
                                          const String& rr,
                                          const String& type,
                                          const String& value) {
  if (recordId.isEmpty() || rr.isEmpty() || type.isEmpty() || value.isEmpty()) {
    return false;
  }

  String params = buildCommonParams("UpdateDomainRecord");
  params += "&RecordId=" + urlEncode(recordId);
  params += "&RR=" + urlEncode(rr);
  params += "&Type=" + urlEncode(type);
  params += "&Value=" + urlEncode(value);

  _lastApiResponse = "";
  if (!invokeApi("POST", params, &_lastApiResponse)) {
    return false;
  }

  String updatedRecordId;
  if (!parseJsonStringField(_lastApiResponse, "RecordId", &updatedRecordId) ||
      updatedRecordId.isEmpty()) {
    return false;
  }
  if (updatedRecordId != recordId) {
    return false;
  }

  _recordId = updatedRecordId;
  _recordIdValid = true;
  return true;
}

bool AliyunDdnsClient::deleteDomainRecord(const String& recordId) {
  if (recordId.isEmpty()) {
    return false;
  }

  String params = buildCommonParams("DeleteDomainRecord");
  params += "&RecordId=" + urlEncode(recordId);

  _lastApiResponse = "";
  if (!invokeApi("POST", params, &_lastApiResponse)) {
    return false;
  }

  String deletedRecordId;
  if (!parseJsonStringField(_lastApiResponse, "RecordId", &deletedRecordId) ||
      deletedRecordId.isEmpty()) {
    return false;
  }
  if (deletedRecordId != recordId) {
    return false;
  }

  if (_recordId == recordId) {
    _recordId = "";
    _recordIdValid = false;
  }
  if (_lastCreatedRecordId == recordId) {
    _lastCreatedRecordId = "";
  }
  return true;
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
  for (int i = 0; i < 16; ++i) {
    nonce[i] = 'a' + random(26);
  }
  nonce[16] = '\0';
  return String(nonce);
}

String AliyunDdnsClient::urlEncode(const String& value) const {
  String encoded;
  encoded.reserve(value.length() * 3);

  for (size_t i = 0; i < value.length(); ++i) {
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

String AliyunDdnsClient::calculateSignature(const String& method,
                                            const String& sortedParams) const {
  const String stringToSign = method + "&" + urlEncode("/") + "&" + urlEncode(sortedParams);
  const String key = _accessKeySecret + "&";
  return sha1Hmac(key, stringToSign);
}

String AliyunDdnsClient::sha1Hmac(const String& key, const String& data) const {
  uint8_t hmacResult[20];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), 1);
  mbedtls_md_hmac_starts(&ctx, reinterpret_cast<const unsigned char*>(key.c_str()), key.length());
  mbedtls_md_hmac_update(&ctx, reinterpret_cast<const unsigned char*>(data.c_str()), data.length());
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);
  return base64Encode(hmacResult, sizeof(hmacResult));
}

String AliyunDdnsClient::base64Encode(const uint8_t* data, size_t length) const {
  return base64::encode(data, length);
}

const char* AliyunDdnsClient::recordType() const {
  return _useIpv6 ? kRecordTypeIpv6 : kRecordTypeIpv4;
}

String AliyunDdnsClient::buildCommonParams(const String& action) const {
  if (action.isEmpty()) {
    return "";
  }

  String params = "Action=" + action;
  params += "&AccessKeyId=" + urlEncode(_accessKeyId);
  params += "&Format=JSON";
  params += "&SignatureMethod=" + String(kSignatureMethod);
  params += "&SignatureNonce=" + generateNonce();
  params += "&SignatureVersion=" + String(kSignatureVersion);
  params += "&Timestamp=" + urlEncode(generateTimestamp());
  params += "&Version=" + String(kApiVersion);
  return params;
}

String AliyunDdnsClient::buildSignedParams(const String& method, const String& rawParams) const {
  if (rawParams.isEmpty()) {
    return "";
  }

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

  String sortedParams;
  for (size_t i = 0; i < pairs.size(); ++i) {
    if (i > 0) {
      sortedParams += "&";
    }
    sortedParams += pairs[i];
  }

  const String signature = calculateSignature(method, sortedParams);
  sortedParams += "&Signature=" + urlEncode(signature);
  return sortedParams;
}

bool AliyunDdnsClient::sendSignedRequest(const String& method,
                                         const String& signedParams,
                                         String* response) const {
  if (response == nullptr || signedParams.isEmpty()) {
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setConnectTimeout(kRequestTimeoutMs);
  http.setTimeout(kRequestTimeoutMs);
  http.setUserAgent(kUserAgent);

  int code = -1;
  if (method == "GET") {
    const String url = String(kAliyunDnsEndpoint) + "?" + signedParams;
    if (!http.begin(client, url)) {
      return false;
    }
    code = http.GET();
  } else if (method == "POST") {
    if (!http.begin(client, String(kAliyunDnsEndpoint))) {
      return false;
    }
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    code = http.POST(signedParams);
  } else {
    return false;
  }

  if (code > 0) {
    *response = http.getString();
  } else {
    *response = "";
  }

  http.end();
  return code == HTTP_CODE_OK;
}

bool AliyunDdnsClient::invokeApi(const String& method,
                                 const String& rawParams,
                                 String* response) const {
  if (response == nullptr || rawParams.isEmpty()) {
    return false;
  }
  if (!_begun || _accessKeyId.isEmpty() || _accessKeySecret.isEmpty()) {
    return false;
  }

  const String signedParams = buildSignedParams(method, rawParams);
  if (signedParams.isEmpty()) {
    return false;
  }

  if (!sendSignedRequest(method, signedParams, response)) {
    return false;
  }

  // Aliyun errors include top-level Code + Message.
  if (response->indexOf("\"Code\":\"") != -1 && response->indexOf("\"Message\":\"") != -1) {
    return false;
  }
  return true;
}

bool AliyunDdnsClient::parseJsonStringField(const String& response,
                                            const char* key,
                                            String* value) const {
  if (key == nullptr || value == nullptr) {
    return false;
  }

  const String pattern = String("\"") + key + "\":\"";
  int start = response.indexOf(pattern);
  if (start == -1) {
    return false;
  }
  start += pattern.length();

  const int end = response.indexOf("\"", start);
  if (end == -1) {
    return false;
  }

  *value = response.substring(start, end);
  return true;
}

bool AliyunDdnsClient::fetchRecordInfo(RecordInfo* info) {
  if (info == nullptr || _domain.isEmpty() || _subDomain.isEmpty()) {
    return false;
  }

  String params = buildCommonParams("DescribeDomainRecords");
  params += "&DomainName=" + urlEncode(_domain);
  params += "&RRKeyWord=" + urlEncode(_subDomain);
  params += "&TypeKeyWord=" + urlEncode(String(recordType()));

  String response;
  if (!invokeApi("GET", params, &response)) {
    return false;
  }

  info->recordId = "";
  info->value = "";
  if (!parseJsonStringField(response, "RecordId", &info->recordId) ||
      !parseJsonStringField(response, "Value", &info->value)) {
    return false;
  }
  if (info->recordId.isEmpty() || info->value.isEmpty()) {
    return false;
  }

  _recordId = info->recordId;
  _recordIdValid = true;
  return true;
}

bool AliyunDdnsClient::updateRecord(const String& recordId, const String& newIp) {
  if (recordId.isEmpty() || newIp.isEmpty()) {
    return false;
  }

  String params = buildCommonParams("UpdateDomainRecord");
  params += "&RecordId=" + urlEncode(recordId);
  params += "&RR=" + urlEncode(_subDomain);
  params += "&Type=" + urlEncode(String(recordType()));
  params += "&Value=" + urlEncode(newIp);

  String response;
  if (!invokeApi("POST", params, &response)) {
    return false;
  }

  String updatedRecordId;
  if (!parseJsonStringField(response, "RecordId", &updatedRecordId) || updatedRecordId.isEmpty()) {
    return false;
  }
  if (updatedRecordId != recordId) {
    return false;
  }

  _recordId = updatedRecordId;
  _recordIdValid = true;
  return true;
}
