#include "TelemetryDisplay.h"

#include "BoardConfig.h"
#include "HealthScreen.h"

namespace {

// Vertical layout, in pixels from the top of the 64 px tall screen.
constexpr int16_t kLabelY = 0;         // size-1 text, 8 px tall
constexpr int16_t kValueBandY = 14;    // band that holds the big value
constexpr int16_t kValueBandH = 24;    // tall enough for size-3 text
constexpr int16_t kGaugeY = 44;        // 8 px tall bar
constexpr int16_t kGaugeHeight = 8;
constexpr int16_t kGaugeInsetX = 6;
constexpr int16_t kDividerBottom = kGaugeY + kGaugeHeight + 3;
constexpr int16_t kFooterY = 56;       // size-1 text, 8 px tall

constexpr int16_t kScreenWidth = 128;
constexpr int16_t kUnitGap = 1;
constexpr int16_t kValueMargin = 1;    // keep one pixel of air before the divider

}  // namespace

TelemetryDisplay::TelemetryDisplay()
    : oled_(BoardConfig::kOledNativeWidth, BoardConfig::kOledNativeHeight, &Wire) {}

bool TelemetryDisplay::begin() {
  if (!oled_.begin(BoardConfig::kOledI2cAddress, true)) {
    return false;
  }
  oled_.setRotation(BoardConfig::kOledRotation);
  oled_.cp437(true);  // correct glyph mapping so 0xF8 renders as a degree sign
  oled_.setTextColor(SH110X_WHITE);
  oled_.setTextWrap(false);
  oled_.clearDisplay();
  oled_.display();
  return true;
}

void TelemetryDisplay::showMessage(const char* line1, const char* line2) {
  oled_.clearDisplay();
  oled_.setFont(nullptr);
  drawCenteredText(kScreenWidth / 2, 22, 1, line1);
  drawCenteredText(kScreenWidth / 2, 36, 1, line2);
  oled_.display();
}

void TelemetryDisplay::render(const PageSpec& page, const TelemetryData& data, LinkState link,
                              uint32_t nowMs) {
  const Column left{0, kScreenWidth / 2};
  const Column right{kScreenWidth / 2, kScreenWidth / 2};
  const bool linkUp = (link == LinkState::Alive);
  const uint8_t valueSize = chooseValueSize(page);

  oled_.clearDisplay();
  oled_.setFont(nullptr);

  oled_.drawFastVLine(kScreenWidth / 2, kLabelY, kDividerBottom, SH110X_WHITE);
  drawParameter(left, *page.left, data, linkUp, valueSize, nowMs);
  drawParameter(right, *page.right, data, linkUp, valueSize, nowMs);
  drawFooter(link);

  oled_.display();
}

void TelemetryDisplay::renderHealth(const VehicleHealth& health, LinkState link) {
  oled_.clearDisplay();
  HealthScreen(oled_).draw(health);
  drawFooter(link);
  oled_.display();
}

// ---- Private helpers --------------------------------------------------------

void TelemetryDisplay::drawParameter(const Column& col, const ParameterSpec& spec,
                                     const TelemetryData& data, bool linkUp, uint8_t valueSize,
                                     uint32_t nowMs) {
  drawLabel(col, spec.label);

  const Reading& reading = data.get(spec.channel);
  if (linkUp && reading.isFresh(nowMs, kStaleAfterMs)) {
    char text[12];
    spec.format.format(reading.value, text, sizeof(text));
    drawValue(col, text, spec.unit, valueSize);
    drawGauge(col, spec.gaugePercent(reading.value), spec.gaugeTickPercent());
  } else {
    drawValue(col, "--", nullptr, valueSize);
    drawGauge(col, 0, spec.gaugeTickPercent());
  }
}

void TelemetryDisplay::drawLabel(const Column& col, const char* label) {
  drawCenteredText(col.centerX(), kLabelY, 1, label);
}

// Big digits with a small unit tucked into the top-right corner, centred as a
// group inside the column and vertically centred in the value band.
void TelemetryDisplay::drawValue(const Column& col, const char* digits, const char* unit,
                                 uint8_t size) {
  const int16_t digitsInk = inkWidth(digits, size);
  const int16_t unitInk = (unit != nullptr) ? inkWidth(unit, 1) : 0;
  const int16_t totalInk = digitsInk + (unit != nullptr ? kUnitGap + unitInk : 0);

  const int16_t x0 = col.x + (col.width - totalInk) / 2;
  const int16_t y0 = kValueBandY + (kValueBandH - 8 * size) / 2;

  oled_.setTextSize(size);
  oled_.setCursor(x0, y0);
  oled_.print(digits);

  if (unit != nullptr) {
    oled_.setTextSize(1);
    oled_.setCursor(x0 + digitsInk + kUnitGap, y0 + 1);
    oled_.print(unit);
  }
}

// Rounded outline bar with a proportional fill. tickPercent < 0 hides the tick.
void TelemetryDisplay::drawGauge(const Column& col, uint8_t fillPercent, int8_t tickPercent) {
  const int16_t x = col.x + kGaugeInsetX;
  const int16_t w = col.width - 2 * kGaugeInsetX;
  const int16_t y = kGaugeY;
  const int16_t h = kGaugeHeight;

  oled_.drawRoundRect(x, y, w, h, 2, SH110X_WHITE);

  const int16_t innerW = w - 4;
  const int16_t fillW = (innerW * min<int16_t>(fillPercent, 100)) / 100;
  if (fillW > 0) {
    oled_.fillRect(x + 2, y + 2, fillW, h - 4, SH110X_WHITE);
  }

  if (tickPercent >= 0) {
    const int16_t tickX = x + 2 + (innerW * min<int16_t>(tickPercent, 100)) / 100;
    oled_.drawFastVLine(tickX, y + h + 1, 2, SH110X_WHITE);
  }
}

void TelemetryDisplay::drawFooter(LinkState link) {
  switch (link) {
    case LinkState::Waiting:
      drawCenteredText(kScreenWidth / 2, kFooterY, 1, "WAITING FOR ECU");
      break;
    case LinkState::Lost:
      drawCenteredText(kScreenWidth / 2, kFooterY, 1, "NO ECU RESPONSE");
      break;
    case LinkState::Alive:
    default:
      break;  // healthy: keep the screen clean
  }
}

void TelemetryDisplay::drawCenteredText(int16_t centerX, int16_t y, uint8_t size,
                                        const char* text) {
  const int16_t w = inkWidth(text, size);
  oled_.setTextSize(size);
  oled_.setCursor(centerX - w / 2, y);
  oled_.print(text);
}

// Size 3 if the widest value each parameter can produce still fits its column;
// otherwise size 2. Both columns use the same size so the page looks balanced.
uint8_t TelemetryDisplay::chooseValueSize(const PageSpec& page) {
  const ParameterSpec* specs[] = {page.left, page.right};
  const int16_t available = kScreenWidth / 2 - kValueMargin;

  for (uint8_t size = 3; size > 2; --size) {
    bool fits = true;
    for (const ParameterSpec* spec : specs) {
      const int16_t digitsInk = spec->maxChars * 6 * size - size;
      const int16_t unitInk = (spec->unit != nullptr) ? inkWidth(spec->unit, 1) : 0;
      const int16_t total = digitsInk + (spec->unit != nullptr ? kUnitGap + unitInk : 0);
      if (total > available) fits = false;
    }
    if (fits) return size;
  }
  return 2;
}

// Width of the built-in font's ink for `text` at `size`: 6 px per cell minus
// the trailing spacing column of the last character.
int16_t TelemetryDisplay::inkWidth(const char* text, uint8_t size) {
  const size_t n = strlen(text);
  if (n == 0) return 0;
  return static_cast<int16_t>(n * 6 * size - size);
}
