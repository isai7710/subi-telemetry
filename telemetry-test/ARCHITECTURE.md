# claude-telemetry-test — Architecture

> Audience: an LLM (or engineer) opening this codebase cold. Read this file
> first. It states what the hardware is, how data flows, which invariants the
> code relies on, and how to extend it without breaking those invariants.

## 1. What this is

An Arduino sketch for an OBD-II dashboard in a **2013 Subaru XV Crosstrek**
(FB20 engine, CVT). It polls the engine ECM over CAN, decodes a handful of
Mode 01 PIDs and diagnostic services, and draws them on a 128x64 OLED. Three
buttons switch between screens. Verified working on the car.

| Item | Value |
|---|---|
| MCU board | Adafruit Feather RP2040 CAN Bus (RP2040 + MCP2515 + TJA1051 transceiver) |
| Display | Adafruit 128x64 OLED FeatherWing (SH1107, I2C 0x3C), three tactile buttons |
| FQBN | `rp2040:rp2040:adafruit_feather_can` (see `sketch.yaml`) |
| CAN | 500 kbit/s, 11-bit IDs, ISO 15765-4 |
| Libraries | Adafruit MCP2515 (+ Adafruit CAN, Adafruit BusIO), Adafruit SH110X, Adafruit GFX |
| Toolchain | `arduino-cli compile --fqbn rp2040:rp2040:adafruit_feather_can claude-telemetry-test` from the parent directory; the user uploads with `arduino-cli upload` |

Compiler warnings about "Adafruit CAN claims to run on samd" and
`-Woverloaded-virtual` in `CANController.h` come from the library, not this
code. This sketch compiles with zero warnings of its own under `--warnings all`.

## 2. Pin map (BoardConfig.h is the single source of truth)

| Function | Pin | Notes |
|---|---|---|
| MCP2515 chip select | `PIN_CAN_CS` = GPIO 19 | board variant macro; SPI pins come from the variant |
| OLED | I2C `Wire` (GPIO 2 SDA, GPIO 3 SCL) | address 0x3C, panel is 64x128 native, rotated to 128x64 |
| Button A | GPIO 9 | active low, internal pull-up |
| Button B | GPIO 6 | active low, internal pull-up |
| Button C | GPIO 5 | active low, internal pull-up |

CAN standby/reset pins are not touched; the working reference sketches did not
touch them either.

## 3. File map

```
claude-telemetry-test/
  claude-telemetry-test.ino   setup()/loop() only: owns objects, maps buttons to screens, serial log
  sketch.yaml                 FQBN + port
  ARCHITECTURE.md             this file
  src/
    BoardConfig.h             pins, I2C address, CAN baud, OLED geometry (namespace BoardConfig)
    ObdPid.h/.cpp             CAN IDs, service numbers, Pid enum, Channel enum, PidDef decode table
    ObdRequest.h              one poll-rotation entry: service, optional PID, target CAN ID, period
    IsoTpReassembler.h/.cpp   ISO 15765-2 receive side (single + multi-frame), flow-control frame builder
    TelemetryData.h           Reading (value + timestamp + valid) and channel-indexed store
    VehicleHealth.h/.cpp      MonitorStatus (PID 01), DtcList (Mode 03/07), formatDtc()
    ObdClient.h/.cpp          non-blocking poller: scheduler, TX, RX, reassembly, dispatch
    Parameter.h/.cpp          ValueFormat (fixed-point formatter), ParameterSpec, PageSpec
    Pages.h/.cpp              the concrete parameters, dashboards, request lists, screens, unit prefs
    TelemetryDisplay.h/.cpp   owns the SH1107; dashboard renderer; delegates health view; footer
    HealthScreen.h/.cpp       vehicle-health renderer (draws onto an Adafruit_GFX&)
    BootAnimation.h/.cpp      ~3 s procedural Subaru intro (stars, oval, wordmark, subtitle)
    Button.h/.cpp             debounced active-low button with one-shot wasPressed()
```

Includes from the `.ino` are `"src/X.h"`; inside `src/` they are `"X.h"`.
No heap allocation anywhere. No `String`. No floating point in the data path
(the boot animation uses `float` for easing only).

## 4. Data flow

```
 loop()
   │  every pass
   ├─ Button::update()  ─► wasPressed() ─► selectScreen(id)
   │                                          └─ ObdClient::setRequests(screen.requests)
   ├─ ObdClient::update(now)
   │     ├─ drainIncoming(): MCP2515 RX ─► IsoTpReassembler::feed()
   │     │        ├─ FirstFrame  ─► sendFlowControl(0x7E0)
   │     │        └─ Complete    ─► handleMessage()
   │     │                             ├─ 0x41 + PID 01       ─► VehicleHealth.status
   │     │                             ├─ 0x41 + numeric PID  ─► TelemetryData[channel]  (via PidDef table)
   │     │                             ├─ 0x43                ─► VehicleHealth.stored
   │     │                             └─ 0x47                ─► VehicleHealth.pending
   │     ├─ timeout of the outstanding request (unless a multi-frame RX is in progress)
   │     └─ pickNextRequest() ─► sendRequest()   (round-robin, honouring each request's periodMs)
   │  every 100 ms
   ├─ TelemetryDisplay::render(PageSpec, TelemetryData, LinkState)   or
   │  TelemetryDisplay::renderHealth(VehicleHealth, LinkState)
   │  every 1000 ms
   └─ Serial log line for the active screen
```

Exactly one OBD request is outstanding at any time. The next is sent only
after the matching response arrives or the 300 ms timeout expires, plus a
40 ms gap. This keeps the bus quiet and makes ISO-TP reassembly single-stream.

## 5. Screens and buttons

| Button | Screen | Left / right | Requests (target, period) |
|---|---|---|---|
| A | ENGINE | COOLANT °C (gauge 0..120, tick 90) / LOAD % (0..100) | PID 05, PID 04 (0x7DF, as fast as possible) |
| A again | HEALTH | MIL badge, stored DTCs, pending DTCs, readiness monitors | PID 01 (0x7DF, 500 ms); Mode 03 and Mode 07 (0x7E0, 1500 ms) |
| B | FUELING | ST TRIM % / LT TRIM % (gauge -25..+25, tick 0, signed) | PID 06, PID 07 |
| C | INTAKE | INTAKE °C (0..80) / BATTERY V one decimal (10..16, tick 14.0) | PID 0F, PID 42 |

Button A toggles ENGINE ↔ HEALTH. B and C are absolute selects. RPM and speed
were deliberately dropped: the instrument cluster already shows them.

A screen is `Pages::Screen { name, dashboard (PageSpec* or nullptr), requests, requestCount }`.
`dashboard == nullptr` means "draw the health view". Switching screens swaps the
poll list, so only the active screen's requests are on the bus.

## 6. Key design decisions and invariants

1. **Integers only, fixed raw units per channel.** `Obd::Channel` documents the
   unit of each slot (°C, %, tenths of %, rpm, km/h, mV). Decoders in
   `ObdPid.cpp` produce that unit. Display conversion (°F, volts, signed %) is
   done by `ValueFormat` at draw time with exact integer rounding
   (half away from zero). Never store floats.
2. **Freshness, not just validity.** Every `Reading` and health block carries
   `updatedAtMs`. The dashboard draws `--` when a value is older than 1500 ms
   or the link is down. Health keeps showing last-known data when the link
   drops but the footer says `NO ECU RESPONSE`.
3. **Link state** is derived in the `.ino`: `Waiting` (never answered),
   `Alive` (answer within 2000 ms), `Lost` (otherwise). Only the footer and
   value blanking depend on it.
4. **Nothing blocks after setup.** No `delay()` in `loop()` on the normal
   path; no `while (!Serial)` anywhere (the board must boot with no USB host).
   `BootAnimation::play()` and the 250 ms OLED power-up delay are the only
   blocking calls and they are in `setup()`.
5. **Hardware acceptance filter.** `ObdClient::begin` sets the MCP2515 filter
   to `0x7E8/0x7F8`, so only ECU responses (0x7E8..0x7EF) reach the RP2040.
   `drainIncoming` re-checks the ID anyway.
6. **Addressing.** Numeric PIDs go to the functional ID 0x7DF. Diagnostic
   services (Mode 03/07) go to the ECM's physical ID 0x7E0 so exactly one
   module answers and multi-frame responses cannot interleave. PID 01 is
   accepted only from 0x7E8 for the same reason. Flow control is sent to
   `responseId - 8`.
7. **ISO-TP.** `IsoTpReassembler` handles single frames, first+consecutive
   frames (max 64 bytes), sequence checking, and a 1 s stall timeout. The flow
   control frame requests `STmin = 20 ms` because the MCP2515 has only two RX
   buffers and `display()` can stall the loop for ~25 ms; a 20 ms spacing
   guarantees at most two frames queue during that stall. Do not lower STmin
   without adding block-size handling.
8. **Request scheduler.** `ObdRequest.periodMs` is a minimum re-send interval.
   `pickNextRequest` walks the rotation from a cursor and returns the first
   entry that is due; dashboards use period 0 (back-to-back), health uses
   500/1500 ms. While a multi-frame receive is in progress the request timeout
   is suspended.
9. **DTC parsing follows ISO 15765-4**: Mode 03/07 payload is
   `[count][hi lo]...`. `DtcList` keeps up to 8 codes but records `reported`
   (the ECU's count) so the UI can print `+N` overflow. Zero codes (`0x0000`
   padding) are skipped. `formatDtc` maps the top two bits to P/C/B/U.
10. **Readiness monitors** (PID 01): byte B bits 0-2 = continuous supported,
    bits 4-6 = continuous incomplete; byte C = non-continuous supported, byte D
    = incomplete. `monitorsReady = supported & ~incomplete`, summed.
11. **Per-page value text size.** `TelemetryDisplay::chooseValueSize` picks
    size 3 if every parameter's `maxChars` plus unit fits in 63 px of ink,
    else size 2, so both columns of a page always match. `maxChars` on a
    `ParameterSpec` must be the true worst case or text will cross the divider.
12. **Classic GFX font metrics** are hard-coded: 6 px advance per char per
    size unit, ink width = `chars*6*size - size`. The degree sign is CP437
    0xF8 and requires `cp437(true)`, which `TelemetryDisplay::begin` sets.
13. **Gauges** are defined in raw channel units (`gaugeMin/Max/Tick`) so they
    never depend on the display unit preference.
14. **Unit preference** is compile-time: `Pages::kTempUnit`. (Speed/mph was
    removed with the RPM/speed page.) Changing it re-derives units, formulas,
    and labels; gauges are unaffected.

## 7. Display geometry (128x64 landscape, rotation 1)

Dashboard (`TelemetryDisplay.cpp`):

| Element | Y | Notes |
|---|---|---|
| Label (size 1) | 0 | centred in its 64 px column |
| Value band | 14..37 | size 3 at y=14, size 2 at y=18; unit at size 1 is placed top-right of digits with 1 px gap |
| Gauge | 44..51 | rounded rect 52 px wide, 2 px inset fill, optional tick 2 px below |
| Divider | x=64, y 0..54 | |
| Footer | 56 | only `WAITING FOR ECU` / `NO ECU RESPONSE` |

Health (`HealthScreen.cpp`): header y=1 with MIL badge right-aligned
(inverted rounded rect when lit), rule y=10, STORED y=14 (two lines of two
codes), PENDING y=34 (one line), rule y=43, MONITORS y=46, footer y=56.
Label column x=0, value column x=50, code cell 30 px + 6 px gap.

Boot animation (`BootAnimation.cpp`): emblem = ellipse rx 28 / ry 15 with six
four-pointed stars at fixed offsets; timeline constants are in the header
(stars 0..0.76 s, oval sweep 0.82..1.34 s, glide 1.62..2.04 s, subtitle types
from 2.1 s, ends 3.1 s). Wordmark uses `FreeSansBoldOblique12pt7b`
(101 px wide); everything else uses the classic font.

## 8. How to extend

**Add a numeric PID**
1. Add to `Obd::Pid` and `Obd::Channel` (`ObdPid.h`), documenting the raw unit.
2. Add a decoder and a `kPidTable` row in `ObdPid.cpp` (`dataBytes` must be right; the client length-checks against it).
3. Add a `ParameterSpec` in `Pages.cpp`: label (≤7 chars), unit (or nullptr), `ValueFormat`, gauge range in raw units, tick or `kNoTick`, honest `maxChars`.

**Add or change a dashboard screen**
1. Define a `PageSpec {&left, &right}` and an `ObdRequest` array in `Pages.cpp`.
2. Add a `ScreenId` and a `kScreens` row (order must match the enum).
3. Map a button to it in the `.ino`.

**Add a diagnostic service** (e.g. Mode 09 VIN): add the service constant,
an `ObdRequest::ecmService(...)` entry, a parser in `VehicleHealth`, and a
branch in `ObdClient::handleMessage`. Responses longer than 64 bytes need
`IsoTpReassembler::kMaxMessageBytes` raised.

**Change the colour/size of nothing**: the panel is 1-bit. Emphasis is done
with inversion (see the MIL badge) or text size.

## 9. Verification practice

- **Compile** after every change (command in §1). Expect only the two library warnings.
- **Host unit tests**: the pure logic (`Parameter`, `ObdPid`, `IsoTpReassembler`,
  `VehicleHealth`, `ObdRequest`) compiles on the host with a 5-line stub
  `Arduino.h` providing `<stdint.h>`, `<string.h>` and a `constrain` macro.
  This was used to verify rounding, DTC decoding, multi-frame reassembly, and
  monitor bit-math. Rebuild that harness in a scratch directory when touching
  those files.
- **Pixel preview**: a small Python emulator of the GFX primitives, driven by
  the library's real `glcdfont.c`, was used to render each screen to PNG and
  eyeball layout before uploading. It caught a value-size regression once.
  Recreate it when changing layout constants; it is ~100 lines.
- **On-car**: the serial log (115200 baud, one line per second) prints the
  active screen's values, link state and response count. A column stuck on
  `--` while its neighbour updates means that PID is unsupported by the ECM.

## 10. Known unknowns

- Fuel-trim PIDs (06/07), PID 01, and Modes 03/07 are standard and expected to
  work on this ECM, but have not yet been confirmed on the car as of the
  commit that added them. Coolant, load, intake temp, voltage, RPM and speed
  were confirmed.
- Multi-frame DTC responses (three or more codes) have only been exercised in
  the host test, not against the ECM.
- Mode 03 responses from modules other than the ECM (e.g. the TCM at 0x7E9)
  are intentionally not requested. If TCM codes matter, add a second
  `ecmService`-style request targeting 0x7E1 and accept 0x7E9 in
  `handleMessage`, merging the lists.

## 11. Conventions

- `k`-prefixed constants, `snake_case_` trailing-underscore members, `PascalCase` types, `camelCase` functions.
- One class per file pair; headers carry a short purpose comment at the top.
- Hardware access only in `ObdClient` (CAN), `TelemetryDisplay`/`BootAnimation` (OLED) and `Button` (GPIO). Everything else is plain data and pure functions, which is what makes host testing possible.
- Timestamps are `uint32_t` milliseconds from `millis()`; comparisons use subtraction so wrap-around is safe.
- Serial output is for humans; nothing parses it.
