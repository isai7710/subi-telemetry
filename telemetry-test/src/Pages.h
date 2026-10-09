/*
 * Pages.h
 *
 * The screens the FeatherWing buttons select, chosen for a 2013 Subaru
 * XV Crosstrek (FB20, CVT):
 *
 *   A  ENGINE   COOLANT + LOAD      warm-up and how hard it is working
 *   A  HEALTH   MIL, DTCs, monitors (press A again to toggle back)
 *   B  FUELING  ST TRIM + LT TRIM   fuel trims: the earliest sign of a
 *                                   vacuum leak, weak injector, or tired O2 sensor
 *   C  INTAKE + BATTERY             heat soak and charging-system health
 *
 * RPM and speed are deliberately absent: the instrument cluster already
 * shows them. Units are chosen at compile time below.
 */
#pragma once

#include <Arduino.h>

#include "ObdRequest.h"
#include "Parameter.h"

namespace Pages {

enum class TempUnit : uint8_t { Celsius, Fahrenheit };

// ---- User preferences ------------------------------------------------------
constexpr TempUnit kTempUnit = TempUnit::Celsius;

enum class ScreenId : uint8_t {
  Engine,
  Health,
  Fueling,
  IntakeBattery,
  Count,
};

// A screen is either a two-column dashboard (dashboard != nullptr) or the
// vehicle-health view. Either way it owns the list of requests to poll.
struct Screen {
  const char* name;            // for the serial log
  const PageSpec* dashboard;   // nullptr for the health screen
  const ObdRequest* requests;
  size_t requestCount;
};

const Screen& get(ScreenId id);

}  // namespace Pages
