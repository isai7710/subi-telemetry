#include "Parameter.h"

#include <stdio.h>

size_t ValueFormat::format(int32_t raw, char* out, size_t outSize) const {
  int32_t scale = 1;
  for (uint8_t i = 0; i < decimals; ++i) scale *= 10;

  // Value in units of 10^-decimals, rounded half away from zero.
  const int64_t numerator = static_cast<int64_t>(raw) * mul * scale + static_cast<int64_t>(add) * div * scale;
  const int64_t twice = 2 * numerator;
  const int64_t scaled = (twice >= 0) ? (twice + div) / (2 * div) : (twice - div) / (2 * div);

  const char* sign = (scaled < 0) ? "-" : (showPlusSign && scaled > 0) ? "+" : "";
  const int64_t magnitude = (scaled < 0) ? -scaled : scaled;

  if (decimals == 0) {
    return snprintf(out, outSize, "%s%ld", sign, static_cast<long>(magnitude));
  }
  const long whole = static_cast<long>(magnitude / scale);
  const long frac = static_cast<long>(magnitude % scale);
  return snprintf(out, outSize, "%s%ld.%0*ld", sign, whole, decimals, frac);
}

uint8_t ParameterSpec::gaugePercent(int32_t raw) const {
  const int32_t clamped = constrain(raw, gaugeMin, gaugeMax);
  return static_cast<uint8_t>(((clamped - gaugeMin) * 100) / (gaugeMax - gaugeMin));
}

int8_t ParameterSpec::gaugeTickPercent() const {
  if (gaugeTick == kNoTick) return -1;
  return static_cast<int8_t>(gaugePercent(gaugeTick));
}
