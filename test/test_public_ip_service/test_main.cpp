#include <Arduino.h>
#include <WiFi.h>
#include <unity.h>

#include "PublicIpService.h"
#include "WifiService.h"

namespace {
constexpr uint32_t kWifiReconnectTimeoutMs = 15000;
constexpr uint32_t kIpv4ResolveTimeoutMs = 5000;
constexpr uint8_t kBenchmarkRuns = 3;

bool ensureWifiConnected(WifiService* wifiService) {
  if (wifiService == nullptr) {
    return false;
  }

  if (wifiService->isConnected()) {
    return true;
  }

  if (wifiService->reconnectFromStored(kWifiReconnectTimeoutMs)) {
    return true;
  }

  return wifiService->isConnected();
}

bool isValidIpv4Literal(const String& value) {
  if (value.isEmpty()) {
    return false;
  }

  IPAddress parsed;
  return parsed.fromString(value);
}
}  // namespace

void setUp() {
  WiFi.mode(WIFI_STA);
}

void tearDown() {}

void test_can_get_local_ipv4() {
  WifiService wifiService;
  if (!ensureWifiConnected(&wifiService)) {
    TEST_IGNORE_MESSAGE("WiFi not connected. Save WiFi config first, then rerun this test.");
    return;
  }

  const String ipv4 = PublicIpService::resolveIpv4(true, kIpv4ResolveTimeoutMs);
  Serial.printf("[IPv4 Local] ip=%s\n", ipv4.c_str());

  TEST_ASSERT_FALSE_MESSAGE(ipv4.isEmpty(),
                            "Local IPv4 is empty. Check WiFi connection status.");
  TEST_ASSERT_TRUE_MESSAGE(isValidIpv4Literal(ipv4),
                           "Local IPv4 format is invalid.");
}

void test_public_ipv4_resolve_latency() {
  WifiService wifiService;
  if (!ensureWifiConnected(&wifiService)) {
    TEST_IGNORE_MESSAGE("WiFi not connected. Save WiFi config first, then rerun this test.");
    return;
  }

  uint32_t totalElapsedMs = 0;
  uint32_t bestElapsedMs = UINT32_MAX;
  uint32_t worstElapsedMs = 0;
  uint8_t successCount = 0;

  Serial.println("[IPv4 Benchmark] Start.");
  for (uint8_t run = 0; run < kBenchmarkRuns; ++run) {
    const uint32_t startMs = millis();
    const String ipv4 = PublicIpService::resolve(false, kIpv4ResolveTimeoutMs);
    const uint32_t elapsedMs = millis() - startMs;

    totalElapsedMs += elapsedMs;
    if (elapsedMs < bestElapsedMs) {
      bestElapsedMs = elapsedMs;
    }
    if (elapsedMs > worstElapsedMs) {
      worstElapsedMs = elapsedMs;
    }

    if (!ipv4.isEmpty()) {
      successCount += 1;
    }

    Serial.printf("[IPv4 Benchmark] run=%u/%u elapsed=%lu ms ip=%s\n",
                  static_cast<unsigned>(run + 1),
                  static_cast<unsigned>(kBenchmarkRuns),
                  static_cast<unsigned long>(elapsedMs),
                  ipv4.c_str());
    delay(150);
  }

  const uint32_t avgElapsedMs = totalElapsedMs / kBenchmarkRuns;
  Serial.printf("[IPv4 Benchmark] summary success=%u/%u avg=%lu ms best=%lu ms worst=%lu ms\n",
                static_cast<unsigned>(successCount),
                static_cast<unsigned>(kBenchmarkRuns),
                static_cast<unsigned long>(avgElapsedMs),
                static_cast<unsigned long>(bestElapsedMs),
                static_cast<unsigned long>(worstElapsedMs));

  TEST_ASSERT_TRUE_MESSAGE(successCount > 0,
                           "Public IPv4 resolve failed in all benchmark runs.");
}

void setup() {
  Serial.begin(115200);
  delay(200);

  UNITY_BEGIN();
  RUN_TEST(test_can_get_local_ipv4);
  RUN_TEST(test_public_ipv4_resolve_latency);
  UNITY_END();
}

void loop() {}
