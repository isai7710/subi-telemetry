/*
 * BoardConfig.h
 *
 * Single source of truth for the hardware this sketch runs on:
 *   - Adafruit Feather RP2040 CAN Bus  (MCP2515 on SPI, CS = GPIO 19)
 *   - Adafruit 128x64 OLED FeatherWing (SH1107 on I2C @ 0x3C, buttons on 9/6/5)
 *
 * Pin numbers match the two test sketches that were verified on this hardware.
 */
#pragma once

#include <Arduino.h>

namespace BoardConfig {

// ---- CAN (MCP2515) --------------------------------------------------------
#if defined(ARDUINO_ADAFRUIT_FEATHER_RP2040_CAN)
constexpr int8_t kCanChipSelectPin = PIN_CAN_CS;  // GPIO 19 on this Feather
#else
#error "This sketch targets the Adafruit Feather RP2040 CAN (rp2040:rp2040:adafruit_feather_can)."
#endif

// 500 kbit/s is the ISO 15765-4 default and what the OBD2 test sketch used.
constexpr long kCanBaudRate = 500000;

// ---- OLED FeatherWing (SH1107) --------------------------------------------
constexpr uint8_t kOledI2cAddress = 0x3C;
constexpr uint16_t kOledNativeWidth = 64;   // panel is 64x128 natively...
constexpr uint16_t kOledNativeHeight = 128;
constexpr uint8_t kOledRotation = 1;        // ...rotated to 128 wide x 64 tall

constexpr uint8_t kButtonAPin = 9;
constexpr uint8_t kButtonBPin = 6;
constexpr uint8_t kButtonCPin = 5;

}  // namespace BoardConfig
