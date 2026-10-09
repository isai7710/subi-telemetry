#include "Button.h"

Button::Button(uint8_t pin, uint32_t debounceMs) : pin_(pin), debounceMs_(debounceMs) {}

void Button::begin() {
  pinMode(pin_, INPUT_PULLUP);
}

void Button::update(uint32_t nowMs) {
  const bool pressedNow = (digitalRead(pin_) == LOW);

  if (pressedNow != rawPressed_) {
    rawPressed_ = pressedNow;
    lastEdgeAtMs_ = nowMs;
  }

  if ((nowMs - lastEdgeAtMs_) >= debounceMs_ && rawPressed_ != stablePressed_) {
    stablePressed_ = rawPressed_;
    if (stablePressed_) {
      pressEventPending_ = true;
    }
  }
}

bool Button::wasPressed() {
  const bool fired = pressEventPending_;
  pressEventPending_ = false;
  return fired;
}
