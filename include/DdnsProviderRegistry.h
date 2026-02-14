#pragma once

#include <Arduino.h>

class EasyDDNSClass;

struct DdnsProviderSpec {
  const char* id;
  bool requiresDomain;
  bool requiresPassword;
};

namespace DdnsProviderRegistry {

String normalizeProvider(const String& provider);
const DdnsProviderSpec& resolve(const String& provider);
bool providerRequiresDomain(const String& provider);
bool providerRequiresPassword(const String& provider);
void configureClient(EasyDDNSClass* client,
                     const String& provider,
                     const String& domain,
                     const String& username,
                     const String& password);

}  // namespace DdnsProviderRegistry
