#include "HealthScreen.h"

#include <Adafruit_SH110X.h>
#include <stdio.h>

namespace {
constexpr int16_t kScreenWidth = 128;
constexpr int16_t kRuleTopY = 10;
constexpr int16_t kStoredY = 14;
constexpr int16_t kPendingY = 34;
constexpr int16_t kRuleBottomY = 43;
constexpr int16_t kMonitorsY = 46;
constexpr int16_t kLineHeight = 9;
constexpr int16_t kCodeWidth = 5 * 6;  // "P0420" in the 6 px font
constexpr int16_t kCodeGap = 6;
}  // namespace

HealthScreen::HealthScreen(Adafruit_GFX& gfx) : gfx_(gfx) {}

void HealthScreen::draw(const VehicleHealth& health) {
  gfx_.setFont(nullptr);
  gfx_.setTextSize(1);
  gfx_.setTextColor(SH110X_WHITE);

  drawHeader(health.status);
  gfx_.drawFastHLine(0, kRuleTopY, kScreenWidth, SH110X_WHITE);
  drawCodeList("STORED", health.stored, kStoredY, 2);
  drawCodeList("PENDING", health.pending, kPendingY, 1);
  gfx_.drawFastHLine(0, kRuleBottomY, kScreenWidth, SH110X_WHITE);
  drawMonitors(health.status, kMonitorsY);
}

void HealthScreen::drawHeader(const MonitorStatus& status) {
  drawText(kLabelX, 1, "HEALTH");

  if (!status.valid) {
    drawText(kScreenWidth - 6 * 6, 1, "MIL --");
    return;
  }

  if (status.milOn) {
    // Inverted badge so a lit lamp is impossible to miss.
    const int16_t w = 3 * 6 + 5;
    const int16_t x = kScreenWidth - w;
    gfx_.fillRoundRect(x, 0, w, 9, 2, SH110X_WHITE);
    gfx_.setTextColor(SH110X_BLACK);
    drawText(x + 3, 1, "MIL");
    gfx_.setTextColor(SH110X_WHITE);
  } else {
    drawText(kScreenWidth - 7 * 6, 1, "MIL off");
  }
}

// Label at the left, codes two per line. If the list is longer than the lines
// allow, the last slot shows "+N" for the overflow.
void HealthScreen::drawCodeList(const char* label, const DtcList& list, int16_t y,
                                uint8_t lines) {
  drawText(kLabelX, y, label);

  if (!list.valid) {
    drawText(kValueX, y, "--");
    return;
  }
  if (list.reported == 0) {
    drawText(kValueX, y, "none");
    return;
  }

  const uint8_t slots = lines * kCodesPerLine;
  const bool overflow = list.reported > slots;
  const uint8_t shown = overflow ? slots - 1 : (list.count < slots ? list.count : slots);

  for (uint8_t i = 0; i < shown; ++i) {
    const int16_t x = kValueX + (i % kCodesPerLine) * (kCodeWidth + kCodeGap);
    const int16_t yy = y + (i / kCodesPerLine) * kLineHeight;
    char code[6];
    formatDtc(list.codes[i], code);
    drawText(x, yy, code);
  }

  if (overflow) {
    char more[8];
    snprintf(more, sizeof(more), "+%u", static_cast<unsigned>(list.reported - shown));
    const int16_t x = kValueX + (shown % kCodesPerLine) * (kCodeWidth + kCodeGap);
    const int16_t yy = y + (shown / kCodesPerLine) * kLineHeight;
    drawText(x, yy, more);
  }
}

void HealthScreen::drawMonitors(const MonitorStatus& status, int16_t y) {
  drawText(kLabelX, y, "MONITORS");

  if (!status.valid) {
    drawText(kValueX + 6, y, "--");
  } else if (status.monitorsSupported == 0) {
    drawText(kValueX + 6, y, "n/a");
  } else {
    char text[16];
    snprintf(text, sizeof(text), "%u/%u ready", static_cast<unsigned>(status.monitorsReady),
             static_cast<unsigned>(status.monitorsSupported));
    drawText(kValueX + 6, y, text);
  }
}

void HealthScreen::drawText(int16_t x, int16_t y, const char* text) {
  gfx_.setCursor(x, y);
  gfx_.print(text);
}
