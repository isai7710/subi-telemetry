#include "VehicleHealth.h"

#include <stdio.h>

namespace {
uint8_t popcount8(uint8_t v) {
  uint8_t n = 0;
  while (v) {
    n += v & 1;
    v >>= 1;
  }
  return n;
}
}  // namespace

void DtcList::parse(const uint8_t* data, size_t length, uint32_t nowMs) {
  if (length < 1) return;

  reported = data[0];
  const size_t available = (length - 1) / 2;
  const size_t n = (reported < available) ? reported : available;

  count = 0;
  for (size_t i = 0; i < n && count < kMaxCodes; ++i) {
    const uint16_t code = (static_cast<uint16_t>(data[1 + 2 * i]) << 8) | data[2 + 2 * i];
    if (code == 0) continue;  // padding
    codes[count++] = code;
  }
  updatedAtMs = nowMs;
  valid = true;
}

void MonitorStatus::parse(const uint8_t* data, uint32_t nowMs) {
  const uint8_t a = data[0];
  const uint8_t b = data[1];
  const uint8_t c = data[2];
  const uint8_t d = data[3];

  milOn = (a & 0x80) != 0;
  dtcCount = a & 0x7F;

  // Byte B: bits 0-2 continuous monitors supported, bits 4-6 incomplete.
  const uint8_t contSupported = b & 0x07;
  const uint8_t contIncomplete = (b >> 4) & 0x07;
  // Bytes C/D: non-continuous monitors supported / incomplete.
  monitorsSupported = popcount8(contSupported) + popcount8(c);
  monitorsReady = popcount8(contSupported & static_cast<uint8_t>(~contIncomplete)) +
                  popcount8(c & static_cast<uint8_t>(~d));

  updatedAtMs = nowMs;
  valid = true;
}

void formatDtc(uint16_t code, char* out) {
  static const char kSystems[] = {'P', 'C', 'B', 'U'};
  const char system = kSystems[(code >> 14) & 0x03];
  const uint8_t firstDigit = (code >> 12) & 0x03;
  snprintf(out, 6, "%c%u%03X", system, firstDigit, code & 0x0FFF);
}
