#include "Pages.h"

namespace Pages {
namespace {

constexpr char kDegree = static_cast<char>(0xF8);  // CP437 degree sign
const char kUnitC[] = {kDegree, 'C', '\0'};
const char kUnitF[] = {kDegree, 'F', '\0'};

constexpr bool kFahrenheit = (kTempUnit == TempUnit::Fahrenheit);

// raw C -> display C or F
constexpr ValueFormat kTempFormat = kFahrenheit ? ValueFormat{9, 5, 32, 0} : ValueFormat{1, 1, 0, 0};
constexpr ValueFormat kPlainFormat{1, 1, 0, 0};
constexpr ValueFormat kTenthsToSignedPercent{1, 10, 0, 0, /*showPlusSign*/ true};
constexpr ValueFormat kMillivoltsToVolts{1, 1000, 0, 1};

// ---- Parameters -------------------------------------------------------------

const ParameterSpec kCoolant = {
    "COOLANT", Obd::Pid::CoolantTemp, Obd::Channel::CoolantTempC,
    kFahrenheit ? kUnitF : kUnitC, kTempFormat,
    /*gauge*/ 0, 120, /*tick*/ 90, /*maxChars*/ 3,
};

const ParameterSpec kLoad = {
    "LOAD", Obd::Pid::EngineLoad, Obd::Channel::EngineLoadPct,
    "%", kPlainFormat,
    /*gauge*/ 0, 100, ParameterSpec::kNoTick, /*maxChars*/ 3,
};

// Trims: gauge spans -25 % .. +25 % (raw tenths), tick at 0 = stoichiometric.
const ParameterSpec kShortTrim = {
    "ST TRIM", Obd::Pid::ShortTermFuelTrim1, Obd::Channel::ShortTermFuelTrimTenths,
    "%", kTenthsToSignedPercent,
    /*gauge*/ -250, 250, /*tick*/ 0, /*maxChars*/ 3,
};

const ParameterSpec kLongTrim = {
    "LT TRIM", Obd::Pid::LongTermFuelTrim1, Obd::Channel::LongTermFuelTrimTenths,
    "%", kTenthsToSignedPercent,
    /*gauge*/ -250, 250, /*tick*/ 0, /*maxChars*/ 3,
};

const ParameterSpec kIntake = {
    "INTAKE", Obd::Pid::IntakeAirTemp, Obd::Channel::IntakeAirTempC,
    kFahrenheit ? kUnitF : kUnitC, kTempFormat,
    /*gauge*/ 0, 80, ParameterSpec::kNoTick, /*maxChars*/ 3,
};

const ParameterSpec kBattery = {
    "BATTERY", Obd::Pid::ControlModuleVoltage, Obd::Channel::ControlModuleMillivolts,
    "V", kMillivoltsToVolts,
    /*gauge*/ 10000, 16000, /*tick: healthy charging*/ 14000, /*maxChars*/ 4,
};

// ---- Dashboards and their request lists -------------------------------------

const PageSpec kEnginePage = {&kCoolant, &kLoad};
const ObdRequest kEngineRequests[] = {
    ObdRequest::currentData(kCoolant.pid),
    ObdRequest::currentData(kLoad.pid),
};

const PageSpec kFuelingPage = {&kShortTrim, &kLongTrim};
const ObdRequest kFuelingRequests[] = {
    ObdRequest::currentData(kShortTrim.pid),
    ObdRequest::currentData(kLongTrim.pid),
};

const PageSpec kIntakeBatteryPage = {&kIntake, &kBattery};
const ObdRequest kIntakeBatteryRequests[] = {
    ObdRequest::currentData(kIntake.pid),
    ObdRequest::currentData(kBattery.pid),
};

// Diagnostics change slowly; poll gently so the bus stays quiet.
const ObdRequest kHealthRequests[] = {
    ObdRequest::currentData(Obd::Pid::MonitorStatus, /*periodMs*/ 500),
    ObdRequest::ecmService(Obd::kModeStoredDtcs, /*periodMs*/ 1500),
    ObdRequest::ecmService(Obd::kModePendingDtcs, /*periodMs*/ 1500),
};

template <size_t N>
constexpr size_t countOf(const ObdRequest (&)[N]) { return N; }

const Screen kScreens[static_cast<size_t>(ScreenId::Count)] = {
    {"ENGINE", &kEnginePage, kEngineRequests, countOf(kEngineRequests)},
    {"HEALTH", nullptr, kHealthRequests, countOf(kHealthRequests)},
    {"FUELING", &kFuelingPage, kFuelingRequests, countOf(kFuelingRequests)},
    {"INTAKE", &kIntakeBatteryPage, kIntakeBatteryRequests, countOf(kIntakeBatteryRequests)},
};

}  // namespace

const Screen& get(ScreenId id) {
  const size_t i = static_cast<size_t>(id);
  return kScreens[i < static_cast<size_t>(ScreenId::Count) ? i : 0];
}

}  // namespace Pages
