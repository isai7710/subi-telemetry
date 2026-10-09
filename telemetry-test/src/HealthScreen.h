/*
 * HealthScreen.h
 *
 * Draws the vehicle-health view onto a GFX canvas. Owns no hardware; the
 * TelemetryDisplay clears the panel, calls draw(), adds the link footer, and
 * pushes the frame.
 *
 *   HEALTH                        MIL      <- MIL badge inverted when the lamp is on
 *   ------------------------------------
 *   STORED   P0420 P0133
 *            P0171 +2
 *   PENDING  none
 *   ------------------------------------
 *   MONITORS 7/8 ready
 */
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>

#include "VehicleHealth.h"

class HealthScreen {
 public:
  explicit HealthScreen(Adafruit_GFX& gfx);

  void draw(const VehicleHealth& health);

 private:
  static constexpr int16_t kLabelX = 0;
  static constexpr int16_t kValueX = 50;
  static constexpr int16_t kCodesPerLine = 2;

  void drawHeader(const MonitorStatus& status);
  void drawCodeList(const char* label, const DtcList& list, int16_t y, uint8_t lines);
  void drawMonitors(const MonitorStatus& status, int16_t y);
  void drawText(int16_t x, int16_t y, const char* text);

  Adafruit_GFX& gfx_;
};
