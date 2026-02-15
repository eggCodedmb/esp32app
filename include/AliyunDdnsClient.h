#pragma once

#include <Arduino.h>
#include <functional>

class AliyunDdnsClient {
 public:
  using UpdateCallback = std::function<void(const char* oldIp, const char* newIp)>;

  AliyunDdnsClient();
  ~AliyunDdnsClient() = default;

  // RecordId is auto-discovered and cached internally.
  void begin(const String& accessKeyId,
             const String& accessKeySecret,
             const String& domain,
             const String& subDomain = "@");

  void update(uint32_t now, bool useLocalIp);
  void onUpdate(UpdateCallback callback);

  uint32_t getUpdateCount() const { return _updateCount; }
  uint32_t getLastUpdateAt() const { return _lastUpdateAtMs; }
  const String& getLastOldIp() const { return _lastOldIp; }
  const String& getLastNewIp() const { return _lastNewIp; }

 private:
  struct RecordInfo {
    String recordId;
    String value;
  };

  String generateTimestamp() const;
  String generateNonce() const;
  String urlEncode(const String& value) const;
  String calculateSignature(const String& method, const String& sortedParams) const;
  String sha1Hmac(const String& key, const String& data) const;
  String base64Encode(const uint8_t* data, size_t length) const;

  bool fetchRecordInfo(RecordInfo* info);
  bool updateRecord(const String& recordId, const String& newIp);

  String _accessKeyId;
  String _accessKeySecret;
  String _domain;
  String _subDomain;
  String _recordId;

  UpdateCallback _updateCallback;

  uint32_t _updateCount = 0;
  uint32_t _lastUpdateAtMs = 0;
  String _lastOldIp;
  String _lastNewIp;

  bool _begun = false;
  bool _recordIdValid = false;
};
