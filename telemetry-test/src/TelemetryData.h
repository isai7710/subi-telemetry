/*
 * TelemetryData.h
 *
 * Plain data model shared between the OBD client (producer) and the display
 * (consumer). Each channel remembers when it was last refreshed so the UI can
 * tell a live value from a stale one.
 */
#pragma once

#include <Arduino.h>

#include "ObdPid.h"

struct Reading {
  int32_t value = 0;
  uint32_t updatedAtMs = 0;
  bool valid = false;

  void set(int32_t newValue, uint32_t nowMs) {
    value = newValue;
    updatedAtMs = nowMs;
    valid = true;
  }

  bool isFresh(uint32_t nowMs, uint32_t maxAgeMs) const {
    return valid && (nowMs - updatedAtMs) <= maxAgeMs;
  }
};

class TelemetryData {
 public:
  const Reading& get(Obd::Channel channel) const { return readings_[index(channel)]; }

  void set(Obd::Channel channel, int32_t value, uint32_t nowMs) {
    readings_[index(channel)].set(value, nowMs);
  }

 private:
  static size_t index(Obd::Channel channel) { return static_cast<size_t>(channel); }

  Reading readings_[static_cast<size_t>(Obd::Channel::Count)];
};
