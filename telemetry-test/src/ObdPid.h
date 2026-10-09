/*
 * ObdPid.h
 *
 * OBD-II (SAE J1979 / ISO 15765-4) constants, the Mode 01 PIDs this sketch
 * knows how to decode, and the channels they land in.
 *
 * Every numeric reading is stored as an integer in a fixed "raw" unit (see
 * Channel) so the data path never touches floating point. Display formatting
 * and unit conversion happen later, in Parameter/Pages.
 */
#pragma once

#include <Arduino.h>

namespace Obd {

// ---- CAN addressing (ISO 15765-4, 11-bit) ----------------------------------
constexpr uint16_t kFunctionalRequestId = 0x7DF;  // every emissions ECU listens
constexpr uint16_t kEcmPhysicalRequestId = 0x7E0;  // engine ECM only
constexpr uint16_t kEcmResponseId = 0x7E8;         // engine ECM answers here

// ECUs reply on 0x7E8..0x7EF. Mask keeps the top eight bits fixed so all eight
// response IDs pass the hardware filter.
constexpr uint16_t kEcuResponseIdBase = 0x7E8;
constexpr uint16_t kEcuResponseIdMask = 0x7F8;

// Physical request ID for the ECU that answered on `responseId` (0x7E8 -> 0x7E0).
constexpr uint16_t physicalRequestIdFor(uint16_t responseId) { return responseId - 8; }

// ---- Services ---------------------------------------------------------------
constexpr uint8_t kModeCurrentData = 0x01;
constexpr uint8_t kModeStoredDtcs = 0x03;
constexpr uint8_t kModePendingDtcs = 0x07;
constexpr uint8_t kPositiveResponseOffset = 0x40;  // 0x01 -> 0x41, 0x03 -> 0x43

enum class Pid : uint8_t {
  MonitorStatus = 0x01,         // MIL, DTC count, readiness (4 bytes; not a Channel)
  EngineLoad = 0x04,
  CoolantTemp = 0x05,
  ShortTermFuelTrim1 = 0x06,
  LongTermFuelTrim1 = 0x07,
  EngineRpm = 0x0C,
  VehicleSpeed = 0x0D,
  IntakeAirTemp = 0x0F,
  ControlModuleVoltage = 0x42,
};

// Storage slots for decoded numeric values, each with its fixed raw unit.
enum class Channel : uint8_t {
  CoolantTempC,             // degrees C
  EngineLoadPct,            // percent
  ShortTermFuelTrimTenths,  // tenths of a percent, signed
  LongTermFuelTrimTenths,   // tenths of a percent, signed
  EngineRpm,                // revolutions per minute
  VehicleSpeedKph,          // km/h
  IntakeAirTempC,           // degrees C
  ControlModuleMillivolts,  // millivolts
  Count
};

// One row per supported numeric PID: how many data bytes it carries and how
// to turn them into the channel's raw integer.
struct PidDef {
  Pid pid;
  Channel channel;
  uint8_t dataBytes;
  int32_t (*decode)(const uint8_t* data);
};

// Returns nullptr for a PID this sketch does not decode numerically.
const PidDef* findPidDef(Pid pid);

}  // namespace Obd
