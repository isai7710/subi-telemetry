/*
 * BootAnimation.h
 *
 * A short Subaru-themed intro drawn on the OLED while the sketch starts up.
 *
 *   1. The six stars of the Pleiades pop in one by one, the large one first.
 *   2. The oval sweeps around them, clockwise from the top.
 *   3. The emblem glides upward while the SUBARU wordmark rises beneath it.
 *   4. "XV CROSSTREK" types out on the bottom line.
 *
 * play() blocks for about three seconds; it is meant to be called once from
 * setup() after the CAN controller has been brought up.
 */
#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Arduino.h>

class BootAnimation {
 public:
  explicit BootAnimation(Adafruit_SH110X& panel);

  void play();

 private:
  struct Star {
    int8_t dx;      // offset from the emblem centre
    int8_t dy;
    uint8_t outer;  // tip radius
    uint8_t inner;  // shoulder radius
  };

  static const Star kStars[];
  static const size_t kStarCount;

  // Timeline (ms from start of play()).
  static constexpr uint32_t kStarStaggerMs = 120;
  static constexpr uint32_t kStarGrowMs = 140;
  static constexpr uint32_t kOvalStartMs = 820;
  static constexpr uint32_t kOvalSweepMs = 520;
  static constexpr uint32_t kGlideStartMs = 1620;
  static constexpr uint32_t kGlideMs = 420;
  static constexpr uint32_t kTypeStartMs = 2100;
  static constexpr uint32_t kTypeCharMs = 45;
  static constexpr uint32_t kEndMs = 3100;

  // Emblem geometry.
  static constexpr int16_t kOvalRx = 28;
  static constexpr int16_t kOvalRy = 15;
  static constexpr int16_t kCenterX = 64;
  static constexpr int16_t kEmblemStartY = 32;   // screen centre
  static constexpr int16_t kEmblemFinalY = 17;
  static constexpr int16_t kWordmarkStartY = 92;  // baseline, off the bottom
  static constexpr int16_t kWordmarkFinalY = 52;  // baseline
  static constexpr int16_t kSubtitleY = 56;

  void drawFrame(uint32_t t);
  void drawEmblem(int16_t cx, int16_t cy, uint32_t t);
  void drawStar(int16_t cx, int16_t cy, int16_t outer, int16_t inner);
  void drawOvalSweep(int16_t cx, int16_t cy, uint16_t sweepDegrees);
  void drawWordmark(int16_t baselineY);
  void drawSubtitle(size_t visibleChars);

  static float easeOutCubic(float x);
  static float easeInOutCubic(float x);
  static float progress(uint32_t t, uint32_t start, uint32_t duration);

  Adafruit_SH110X& panel_;
};
