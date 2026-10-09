/*
 * VehicleHealth.h
 *
 * Diagnostic state of the engine ECM: MIL lamp, readiness monitors, and the
 * stored and pending trouble-code lists. Populated by ObdClient from Mode 01
 * PID 01, Mode 03, and Mode 07 responses; drawn by HealthScreen.
 */
#pragma once

#include <Arduino.h>

struct DtcList {
  static constexpr size_t kMaxCodes = 8;

  uint16_t codes[kMaxCodes] = {};
  uint8_t count = 0;        // codes actually stored here (<= kMaxCodes)
  uint8_t reported = 0;     // codes the ECU said it has (may exceed kMaxCodes)
  uint32_t updatedAtMs = 0;
  bool valid = false;

  // Parses the payload after the response SID: [count][hi lo]... per ISO 15765-4.
  void parse(const uint8_t* data, size_t length, uint32_t nowMs);
};

struct MonitorStatus {
  bool milOn = false;
  uint8_t dtcCount = 0;           // from PID 01 byte A
  uint8_t monitorsSupported = 0;  // continuous + non-continuous
  uint8_t monitorsReady = 0;      // supported and test complete
  uint32_t updatedAtMs = 0;
  bool valid = false;

  // Parses the four data bytes A..D of Mode 01 PID 01.
  void parse(const uint8_t* data, uint32_t nowMs);
};

struct VehicleHealth {
  MonitorStatus status;
  DtcList stored;
  DtcList pending;
};

// Formats a 16-bit DTC as "P0420" etc. `out` must hold at least 6 bytes.
void formatDtc(uint16_t code, char* out);
