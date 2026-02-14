#include "DdnsProviderRegistry.h"

#include <EasyDDNS.h>

namespace {
constexpr const char* kDuckdnsProviderId = "duckdns";

const DdnsProviderSpec kDuckdnsProvider{
    kDuckdnsProviderId,
    true,
    false,
};

void configureDuckdns(EasyDDNSClass* client,
                      const String& domain,
                      const String& username,
                      const String& password) {
  if (client == nullptr) {
    return;
  }

  client->service(kDuckdnsProviderId);
  client->client(domain, username, password);
}
}  // namespace

namespace DdnsProviderRegistry {

String normalizeProvider(const String& provider) {
  String normalized = provider;
  normalized.trim();
  normalized.toLowerCase();
  if (normalized == kDuckdnsProviderId) {
    return normalized;
  }
  return String(kDuckdnsProviderId);
}

const DdnsProviderSpec& resolve(const String& provider) {
  const String normalized = normalizeProvider(provider);
  if (normalized == kDuckdnsProviderId) {
    return kDuckdnsProvider;
  }
  return kDuckdnsProvider;
}

bool providerRequiresDomain(const String& provider) {
  return resolve(provider).requiresDomain;
}

bool providerRequiresPassword(const String& provider) {
  return resolve(provider).requiresPassword;
}

void configureClient(EasyDDNSClass* client,
                     const String& provider,
                     const String& domain,
                     const String& username,
                     const String& password) {
  const String normalized = normalizeProvider(provider);
  if (normalized == kDuckdnsProviderId) {
    configureDuckdns(client, domain, username, password);
    return;
  }
  configureDuckdns(client, domain, username, password);
}

}  // namespace DdnsProviderRegistry
