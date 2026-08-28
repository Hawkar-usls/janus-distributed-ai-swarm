#pragma once

#include <Arduino.h>
#include <esp_partition.h>

namespace janus_adv_elite {

// A 4 KiB factory-only slot at flash 0x7FF000. Public source and Git history
// contain no credential. The final FACTORY.bin overlays this partition locally
// after CI compilation. Application image hash remains untouched.
struct AdvPrimaryCredentialSlot {
  char magic[8];       // "ADVWIFI1"
  char ssid[33];       // NUL terminated
  char password[65];   // NUL terminated
  uint32_t checksum;   // FNV-1a over ssid[33] + password[65]
};

inline uint32_t advCredentialFnv1a(const uint8_t* p, size_t n) {
  uint32_t h = 2166136261UL;
  while (n--) { h ^= *p++; h *= 16777619UL; }
  return h;
}

inline bool loadFactoryPrimaryCredential(String& ssid, String& password) {
  const esp_partition_t* part = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA,
      static_cast<esp_partition_subtype_t>(0x40),
      "advcred");
  if (!part || part->size < sizeof(AdvPrimaryCredentialSlot)) return false;

  AdvPrimaryCredentialSlot slot{};
  if (esp_partition_read(part, 0, &slot, sizeof(slot)) != ESP_OK) return false;
  if (memcmp(slot.magic, "ADVWIFI1", 8) != 0) return false;
  slot.ssid[sizeof(slot.ssid)-1] = 0;
  slot.password[sizeof(slot.password)-1] = 0;
  uint32_t want = advCredentialFnv1a(
      reinterpret_cast<const uint8_t*>(slot.ssid),
      sizeof(slot.ssid) + sizeof(slot.password));
  if (want != slot.checksum || slot.ssid[0] == 0) return false;
  ssid = slot.ssid;
  password = slot.password;
  return true;
}

}  // namespace janus_adv_elite
