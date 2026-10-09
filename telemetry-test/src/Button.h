/*
 * Button.h
 *
 * Debounced, active-low momentary button (the OLED FeatherWing buttons pull
 * the pin to ground when pressed). wasPressed() fires exactly once per press.
 */
#pragma once

#include <Arduino.h>

class Button {
 public:
  explicit Button(uint8_t pin, uint32_t debounceMs = 30);

  void begin();
  void update(uint32_t nowMs);

  // Returns true once for each press edge seen since the last call.
  bool wasPressed();

 private:
  uint8_t pin_;
  uint32_t debounceMs_;
  bool rawPressed_ = false;
  bool stablePressed_ = false;
  uint32_t lastEdgeAtMs_ = 0;
  bool pressEventPending_ = false;
};
