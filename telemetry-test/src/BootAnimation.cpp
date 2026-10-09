#include "BootAnimation.h"

#include <Fonts/FreeSansBoldOblique12pt7b.h>
#include <math.h>

namespace {
const char kWordmark[] = "SUBARU";
const char kSubtitle[] = "XV CROSSTREK";
}  // namespace

// Large star to the left, five small ones clustered to its right.
const BootAnimation::Star BootAnimation::kStars[] = {
    {-12, 0, 8, 2},
    {2, -8, 4, 1},
    {13, -5, 4, 1},
    {7, 2, 4, 1},
    {18, 3, 4, 1},
    {5, 9, 4, 1},
};
const size_t BootAnimation::kStarCount = sizeof(kStars) / sizeof(kStars[0]);

BootAnimation::BootAnimation(Adafruit_SH110X& panel) : panel_(panel) {}

void BootAnimation::play() {
  const uint32_t startMs = millis();
  for (;;) {
    const uint32_t t = millis() - startMs;
    if (t >= kEndMs) break;
    drawFrame(t);
    panel_.display();
  }
}

// ---- Frame composition ------------------------------------------------------

void BootAnimation::drawFrame(uint32_t t) {
  panel_.clearDisplay();

  const float glide = easeInOutCubic(progress(t, kGlideStartMs, kGlideMs));
  const int16_t emblemY = kEmblemStartY + static_cast<int16_t>(lroundf((kEmblemFinalY - kEmblemStartY) * glide));
  const int16_t wordmarkY = kWordmarkStartY + static_cast<int16_t>(lroundf((kWordmarkFinalY - kWordmarkStartY) * glide));

  drawEmblem(kCenterX, emblemY, t);

  if (t >= kGlideStartMs) {
    drawWordmark(wordmarkY);
  }

  if (t >= kTypeStartMs) {
    const size_t shown = min<size_t>((t - kTypeStartMs) / kTypeCharMs, strlen(kSubtitle));
    drawSubtitle(shown);
  }
}

void BootAnimation::drawEmblem(int16_t cx, int16_t cy, uint32_t t) {
  for (size_t i = 0; i < kStarCount; ++i) {
    const float grow = easeOutCubic(progress(t, i * kStarStaggerMs, kStarGrowMs));
    if (grow <= 0.0f) continue;
    const Star& s = kStars[i];
    const int16_t outer = max<int16_t>(1, static_cast<int16_t>(lroundf(s.outer * grow)));
    const int16_t inner = max<int16_t>(1, static_cast<int16_t>(lroundf(s.inner * grow)));
    drawStar(cx + s.dx, cy + s.dy, outer, inner);
  }

  const float sweep = easeOutCubic(progress(t, kOvalStartMs, kOvalSweepMs));
  if (sweep > 0.0f) {
    drawOvalSweep(cx, cy, static_cast<uint16_t>(lroundf(360.0f * sweep)));
  }
}

// ---- Primitives -------------------------------------------------------------

// Four-pointed star: a square core with a triangular spike on each side.
void BootAnimation::drawStar(int16_t cx, int16_t cy, int16_t outer, int16_t inner) {
  if (outer <= 2) {
    panel_.drawFastHLine(cx - outer, cy, 2 * outer + 1, SH110X_WHITE);
    panel_.drawFastVLine(cx, cy - outer, 2 * outer + 1, SH110X_WHITE);
    return;
  }
  panel_.fillRect(cx - inner, cy - inner, 2 * inner + 1, 2 * inner + 1, SH110X_WHITE);
  panel_.fillTriangle(cx, cy - outer, cx - inner, cy - inner, cx + inner, cy - inner, SH110X_WHITE);
  panel_.fillTriangle(cx, cy + outer, cx - inner, cy + inner, cx + inner, cy + inner, SH110X_WHITE);
  panel_.fillTriangle(cx - outer, cy, cx - inner, cy - inner, cx - inner, cy + inner, SH110X_WHITE);
  panel_.fillTriangle(cx + outer, cy, cx + inner, cy - inner, cx + inner, cy + inner, SH110X_WHITE);
}

// Ellipse outline traced clockwise from 12 o'clock, `sweepDegrees` of arc.
void BootAnimation::drawOvalSweep(int16_t cx, int16_t cy, uint16_t sweepDegrees) {
  constexpr uint16_t kStepDeg = 5;
  int16_t prevX = cx;
  int16_t prevY = cy - kOvalRy;
  for (uint16_t deg = kStepDeg; deg <= sweepDegrees; deg += kStepDeg) {
    const float a = (static_cast<float>(deg) - 90.0f) * (static_cast<float>(M_PI) / 180.0f);
    const int16_t x = cx + static_cast<int16_t>(lroundf(kOvalRx * cosf(a)));
    const int16_t y = cy + static_cast<int16_t>(lroundf(kOvalRy * sinf(a)));
    panel_.drawLine(prevX, prevY, x, y, SH110X_WHITE);
    prevX = x;
    prevY = y;
  }
}

void BootAnimation::drawWordmark(int16_t baselineY) {
  int16_t x1, y1;
  uint16_t w, h;
  panel_.setFont(&FreeSansBoldOblique12pt7b);
  panel_.setTextSize(1);
  panel_.getTextBounds(kWordmark, 0, 0, &x1, &y1, &w, &h);
  panel_.setCursor(kCenterX - static_cast<int16_t>(w) / 2 - x1, baselineY);
  panel_.print(kWordmark);
  panel_.setFont(nullptr);
}

void BootAnimation::drawSubtitle(size_t visibleChars) {
  const size_t fullWidth = strlen(kSubtitle) * 6 - 1;  // centre on the finished string
  const int16_t x0 = kCenterX - static_cast<int16_t>(fullWidth) / 2;
  panel_.setFont(nullptr);
  panel_.setTextSize(1);
  panel_.setCursor(x0, kSubtitleY);
  for (size_t i = 0; i < visibleChars; ++i) {
    panel_.write(kSubtitle[i]);
  }
}

// ---- Easing -----------------------------------------------------------------

float BootAnimation::easeOutCubic(float x) {
  const float inv = 1.0f - x;
  return 1.0f - inv * inv * inv;
}

float BootAnimation::easeInOutCubic(float x) {
  if (x < 0.5f) return 4.0f * x * x * x;
  const float inv = -2.0f * x + 2.0f;
  return 1.0f - inv * inv * inv / 2.0f;
}

// 0 before `start`, 1 after `start + duration`, linear in between.
float BootAnimation::progress(uint32_t t, uint32_t start, uint32_t duration) {
  if (t <= start) return 0.0f;
  if (t >= start + duration) return 1.0f;
  return static_cast<float>(t - start) / static_cast<float>(duration);
}
