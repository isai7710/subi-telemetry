/*
 * Parameter.h
 *
 * Describes how one telemetry channel is presented on screen: its label, the
 * PID that feeds it, how the raw integer becomes a printed number, and how it
 * maps onto the bar gauge. Pure description; no drawing code here.
 */
#pragma once

#include <Arduino.h>

#include "ObdPid.h"

// Converts a raw channel value into a printable fixed-point number:
//   display = raw * mul / div + add, expressed with `decimals` decimal places.
// The scaling is applied before rounding so "14.2 V" from 14237 mV works.
struct ValueFormat {
  int32_t mul;
  int32_t div;
  int32_t add;
  uint8_t decimals;
  bool showPlusSign = false;  // "+7" instead of "7"; useful for trims

  // Writes e.g. "88", "-12", "14.2", "+7" into out. Returns characters written.
  size_t format(int32_t raw, char* out, size_t outSize) const;
};

struct ParameterSpec {
  static constexpr int32_t kNoTick = INT32_MIN;

  const char* label;      // short, upper case, <= 7 chars
  Obd::Pid pid;
  Obd::Channel channel;
  const char* unit;       // drawn small beside the value; nullptr for none
  ValueFormat format;
  int32_t gaugeMin;       // raw units
  int32_t gaugeMax;       // raw units
  int32_t gaugeTick;      // raw units, or kNoTick
  uint8_t maxChars;       // widest string format() can produce; drives text size

  uint8_t gaugePercent(int32_t raw) const;
  int8_t gaugeTickPercent() const;  // -1 when there is no tick
};

struct PageSpec {
  const ParameterSpec* left;
  const ParameterSpec* right;
};
