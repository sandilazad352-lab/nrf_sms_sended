#include "CounterStorage.h"

#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <stddef.h>

using namespace Adafruit_LittleFS_Namespace;

namespace {
constexpr uint32_t STORE_MAGIC = 0x4E524643; // "NRFC"
constexpr uint16_t STORE_VERSION = 1;
constexpr char STORE_A[] = "/counter-a.bin";
constexpr char STORE_B[] = "/counter-b.bin";

struct __attribute__((packed)) DiskRecord {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  uint32_t sequence;
  uint32_t count;
  uint32_t acknowledgedCount;
  int8_t nearRssiDbm;
  uint8_t settingsFlags;
  uint16_t reserved2;
  uint32_t crc;
};

File file(InternalFS);

uint32_t crc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      const uint32_t mask = 0UL - (crc & 1UL);
      crc = (crc >> 1) ^ (0xEDB88320UL & mask);
    }
  }
  return ~crc;
}

bool readRecord(const char* path, DiskRecord& record) {
  if (!file.open(path, FILE_O_READ)) return false;
  const int bytesRead = file.read(&record, sizeof(record));
  file.close();
  if (bytesRead != static_cast<int>(sizeof(record))) return false;
  if (record.magic != STORE_MAGIC || record.version != STORE_VERSION) return false;
  return record.crc == crc32(reinterpret_cast<const uint8_t*>(&record), offsetof(DiskRecord, crc));
}

void fromRecord(const DiskRecord& record, CounterState& state) {
  state.sequence = record.sequence;
  state.count = record.count;
  state.acknowledgedCount = record.acknowledgedCount;
  state.nearRssiDbm = record.nearRssiDbm;
  state.settingsFlags = record.settingsFlags;
}
} // namespace

bool CounterStorage::begin() {
  return InternalFS.begin();
}

bool CounterStorage::load(CounterState& state) {
  DiskRecord a = {};
  DiskRecord b = {};
  const bool validA = readRecord(STORE_A, a);
  const bool validB = readRecord(STORE_B, b);

  if (validA && (!validB || static_cast<int32_t>(a.sequence - b.sequence) > 0)) {
    fromRecord(a, state);
    return true;
  }
  if (validB) {
    fromRecord(b, state);
    return true;
  }

  state.sequence = 0;
  state.count = 0;
  state.acknowledgedCount = 0;
  state.nearRssiDbm = -65;
  state.settingsFlags = 0x01;
  return save(state);
}

bool CounterStorage::save(CounterState& state) {
  DiskRecord record = {};
  record.magic = STORE_MAGIC;
  record.version = STORE_VERSION;
  record.sequence = state.sequence + 1;
  record.count = state.count;
  record.acknowledgedCount = state.acknowledgedCount;
  record.nearRssiDbm = state.nearRssiDbm;
  record.settingsFlags = state.settingsFlags;
  record.crc = crc32(reinterpret_cast<const uint8_t*>(&record), offsetof(DiskRecord, crc));

  // Keep the previous valid slot intact until the new one has been written.
  const char* path = (state.sequence & 1U) ? STORE_B : STORE_A;
  if (!file.open(path, FILE_O_WRITE)) return false;
  const bool ok = file.truncate(0) && file.seek(0) &&
      file.write(reinterpret_cast<const uint8_t*>(&record), sizeof(record)) == sizeof(record);
  file.flush();
  file.close();
  if (ok) state.sequence = record.sequence;
  return ok;
}
