/*
 * TelemetryDisplay.h
 *
 * Renders a two-parameter dashboard page on the 128x64 SH1107 OLED FeatherWing.
 *
 * Layout (128 wide x 64 tall, landscape):
 *
 *   +---------------------------------------------------------------+
 *   |    COOLANT              |             LOAD                    |  label
 *   |                         |                                     |
 *   |      88 °C              |            42 %                     |  big value
 *   |                         |                                     |
 *   |   [========    ]        |        [=====       ]               |  gauge
 *   |        ^                |                                     |  (tick)
 *   |                 NO ECU RESPONSE                               |  footer (only on fault)
 *   +---------------------------------------------------------------+
 *
 * The value text size is chosen per page so both columns match: size 3 when
 * the widest expected value fits, otherwise size 2.
 */
#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Arduino.h>

#include "Parameter.h"
#include "TelemetryData.h"
#include "VehicleHealth.h"

enum class LinkState : uint8_t {
  Waiting,  // never heard from the ECU yet
  Alive,    // responses arriving normally
  Lost,     // had responses, but none recently
};

class TelemetryDisplay {
 public:
  TelemetryDisplay();

  // Initialises the panel. Returns false if the OLED does not ACK on I2C.
  bool begin();

  // Full-screen two-line message (errors).
  void showMessage(const char* line1, const char* line2);

  // Draws one frame of a two-column dashboard page.
  void render(const PageSpec& page, const TelemetryData& data, LinkState link, uint32_t nowMs);

  // Draws one frame of the vehicle-health view.
  void renderHealth(const VehicleHealth& health, LinkState link);

  // Raw panel access for full-screen effects such as the boot animation.
  Adafruit_SH110X& panel() { return oled_; }

 private:
  struct Column {
    int16_t x;
    int16_t width;
    int16_t centerX() const { return x + width / 2; }
  };

  // A reading older than this is drawn as "--" even if the link is alive.
  static constexpr uint32_t kStaleAfterMs = 1500;

  void drawParameter(const Column& col, const ParameterSpec& spec, const TelemetryData& data,
                     bool linkUp, uint8_t valueSize, uint32_t nowMs);
  void drawLabel(const Column& col, const char* label);
  void drawValue(const Column& col, const char* digits, const char* unit, uint8_t size);
  void drawGauge(const Column& col, uint8_t fillPercent, int8_t tickPercent);
  void drawFooter(LinkState link);
  void drawCenteredText(int16_t centerX, int16_t y, uint8_t size, const char* text);

  uint8_t chooseValueSize(const PageSpec& page);
  int16_t inkWidth(const char* text, uint8_t size);

  Adafruit_SH1107 oled_;
};
