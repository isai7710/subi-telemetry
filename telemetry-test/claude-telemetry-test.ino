/*
 * claude-telemetry-test
 *
 * OBD-II dashboard for a 2013 Subaru XV Crosstrek on the OLED FeatherWing.
 *
 * Hardware : Adafruit Feather RP2040 CAN Bus + Adafruit 128x64 OLED FeatherWing
 * Board    : rp2040:rp2040:adafruit_feather_can  (see sketch.yaml)
 *
 * Controls : Button A (GPIO 9)  ENGINE (COOLANT + LOAD)  <-> HEALTH (MIL, DTCs, monitors)
 *            Button B (GPIO 6)  FUELING (ST TRIM + LT TRIM)
 *            Button C (GPIO 5)  INTAKE + BATTERY
 *
 * See ARCHITECTURE.md for the design; all the interesting logic lives in src/.
 */

#include "src/BoardConfig.h"
#include "src/BootAnimation.h"
#include "src/Button.h"
#include "src/ObdClient.h"
#include "src/Pages.h"
#include "src/TelemetryDisplay.h"

namespace {

constexpr uint32_t kDisplayIntervalMs = 100;  // 10 Hz refresh is plenty for the eye
constexpr uint32_t kSerialIntervalMs = 1000;  // 1 Hz debug line on the Serial Monitor

ObdClient obd(BoardConfig::kCanChipSelectPin);
TelemetryDisplay display;
Button buttonA(BoardConfig::kButtonAPin);
Button buttonB(BoardConfig::kButtonBPin);
Button buttonC(BoardConfig::kButtonCPin);

Pages::ScreenId activeScreen = Pages::ScreenId::Engine;
bool canReady = false;
uint32_t lastDisplayAtMs = 0;
uint32_t lastSerialAtMs = 0;

void selectScreen(Pages::ScreenId id) {
  activeScreen = id;
  const Pages::Screen& screen = Pages::get(id);
  obd.setRequests(screen.requests, screen.requestCount);
  lastDisplayAtMs = 0;  // redraw immediately
}

// Button A alternates between the engine dashboard and the health view.
Pages::ScreenId buttonATarget() {
  return (activeScreen == Pages::ScreenId::Engine) ? Pages::ScreenId::Health
                                                   : Pages::ScreenId::Engine;
}

LinkState currentLinkState(uint32_t nowMs) {
  if (obd.responseCount() == 0) return LinkState::Waiting;
  return obd.isLinkAlive(nowMs) ? LinkState::Alive : LinkState::Lost;
}

void logParameter(const ParameterSpec& spec) {
  const Reading& r = obd.data().get(spec.channel);
  Serial.print(spec.label);
  Serial.print('=');
  if (r.valid) {
    char text[12];
    spec.format.format(r.value, text, sizeof(text));
    Serial.print(text);
    if (spec.unit != nullptr) Serial.print(spec.unit);
  } else {
    Serial.print("--");
  }
}

void logCodes(const char* label, const DtcList& list) {
  Serial.print(label);
  Serial.print('=');
  if (!list.valid) {
    Serial.print("--");
    return;
  }
  if (list.reported == 0) {
    Serial.print("none");
    return;
  }
  for (uint8_t i = 0; i < list.count; ++i) {
    char code[6];
    formatDtc(list.codes[i], code);
    Serial.print(code);
    if (i + 1 < list.count) Serial.print(',');
  }
  if (list.reported > list.count) {
    Serial.print("(+");
    Serial.print(list.reported - list.count);
    Serial.print(')');
  }
}

void logToSerial(uint32_t nowMs) {
  const Pages::Screen& screen = Pages::get(activeScreen);
  Serial.print("screen=");
  Serial.print(screen.name);
  Serial.print("  ");

  if (screen.dashboard != nullptr) {
    logParameter(*screen.dashboard->left);
    Serial.print("  ");
    logParameter(*screen.dashboard->right);
  } else {
    const VehicleHealth& h = obd.health();
    Serial.print("MIL=");
    Serial.print(!h.status.valid ? "--" : h.status.milOn ? "ON" : "off");
    Serial.print("  monitors=");
    if (h.status.valid) {
      Serial.print(h.status.monitorsReady);
      Serial.print('/');
      Serial.print(h.status.monitorsSupported);
    } else {
      Serial.print("--");
    }
    Serial.print("  ");
    logCodes("stored", h.stored);
    Serial.print("  ");
    logCodes("pending", h.pending);
  }

  Serial.print("  link=");
  Serial.print(obd.isLinkAlive(nowMs) ? "OK" : "DOWN");
  Serial.print("  responses=");
  Serial.println(obd.responseCount());
}

}  // namespace

void setup() {
  Serial.begin(115200);
  // Deliberately no `while (!Serial)`: this must boot in the car with no USB host.

  buttonA.begin();
  buttonB.begin();
  buttonC.begin();

  delay(250);  // let the OLED power rail settle, as in the working OLED test
  if (!display.begin()) {
    Serial.println("OLED not found at 0x3C; continuing without a display.");
  }

  canReady = obd.begin(BoardConfig::kCanBaudRate);
  if (canReady) {
    Serial.println("MCP2515 ready.");
    selectScreen(Pages::ScreenId::Engine);
  } else {
    Serial.println("MCP2515 init failed. Check wiring/connections.");
  }

  BootAnimation intro(display.panel());
  intro.play();

  if (!canReady) {
    display.showMessage("CAN INIT FAILED", "check MCP2515");
  }
}

void loop() {
  if (!canReady) {
    delay(100);  // error is already on screen; nothing else to do
    return;
  }

  const uint32_t nowMs = millis();

  buttonA.update(nowMs);
  buttonB.update(nowMs);
  buttonC.update(nowMs);
  if (buttonA.wasPressed()) selectScreen(buttonATarget());
  if (buttonB.wasPressed() && activeScreen != Pages::ScreenId::Fueling) {
    selectScreen(Pages::ScreenId::Fueling);
  }
  if (buttonC.wasPressed() && activeScreen != Pages::ScreenId::IntakeBattery) {
    selectScreen(Pages::ScreenId::IntakeBattery);
  }

  obd.update(nowMs);

  if (nowMs - lastDisplayAtMs >= kDisplayIntervalMs) {
    lastDisplayAtMs = nowMs;
    const Pages::Screen& screen = Pages::get(activeScreen);
    const LinkState link = currentLinkState(nowMs);
    if (screen.dashboard != nullptr) {
      display.render(*screen.dashboard, obd.data(), link, nowMs);
    } else {
      display.renderHealth(obd.health(), link);
    }
  }

  if (nowMs - lastSerialAtMs >= kSerialIntervalMs) {
    lastSerialAtMs = nowMs;
    logToSerial(nowMs);
  }
}
