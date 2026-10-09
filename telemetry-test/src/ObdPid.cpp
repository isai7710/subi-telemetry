#include "ObdPid.h"

namespace Obd {
namespace {

// A - 40  -> degrees C
int32_t decodeTempC(const uint8_t* d) { return static_cast<int32_t>(d[0]) - 40; }

// A * 100 / 255 -> percent, rounded
int32_t decodePercent(const uint8_t* d) { return (static_cast<int32_t>(d[0]) * 100 + 127) / 255; }

// (A - 128) * 100 / 128 -> percent; kept in tenths for one decimal of precision
int32_t decodeFuelTrimTenths(const uint8_t* d) {
  return ((static_cast<int32_t>(d[0]) - 128) * 1000) / 128;
}

// (256A + B) / 4 -> rpm
int32_t decodeRpm(const uint8_t* d) { return (static_cast<int32_t>(d[0]) * 256 + d[1]) / 4; }

// A -> km/h
int32_t decodeKph(const uint8_t* d) { return d[0]; }

// (256A + B) / 1000 -> volts; kept as millivolts
int32_t decodeMillivolts(const uint8_t* d) { return static_cast<int32_t>(d[0]) * 256 + d[1]; }

const PidDef kPidTable[] = {
    {Pid::EngineLoad, Channel::EngineLoadPct, 1, decodePercent},
    {Pid::CoolantTemp, Channel::CoolantTempC, 1, decodeTempC},
    {Pid::ShortTermFuelTrim1, Channel::ShortTermFuelTrimTenths, 1, decodeFuelTrimTenths},
    {Pid::LongTermFuelTrim1, Channel::LongTermFuelTrimTenths, 1, decodeFuelTrimTenths},
    {Pid::EngineRpm, Channel::EngineRpm, 2, decodeRpm},
    {Pid::VehicleSpeed, Channel::VehicleSpeedKph, 1, decodeKph},
    {Pid::IntakeAirTemp, Channel::IntakeAirTempC, 1, decodeTempC},
    {Pid::ControlModuleVoltage, Channel::ControlModuleMillivolts, 2, decodeMillivolts},
};

}  // namespace

const PidDef* findPidDef(Pid pid) {
  for (const PidDef& def : kPidTable) {
    if (def.pid == pid) return &def;
  }
  return nullptr;
}

}  // namespace Obd
