/*
 * ObdClient.h
 *
 * Non-blocking OBD-II poller built on the Adafruit MCP2515 driver.
 *
 * Call update() from loop() as often as possible. The client rotates through
 * its current request list: it sends one request, waits for the matching
 * response (or a short timeout), decodes it, and moves on. Each request may
 * carry a minimum period so slow-changing diagnostics are not hammered.
 * Multi-frame responses (long DTC lists) are reassembled with ISO-TP flow
 * control. Nothing here ever blocks.
 *
 * The request list can be swapped at runtime (setRequests) so only what the
 * active screen needs is on the bus.
 */
#pragma once

#include <Adafruit_MCP2515.h>
#include <Arduino.h>

#include "IsoTpReassembler.h"
#include "ObdPid.h"
#include "ObdRequest.h"
#include "TelemetryData.h"
#include "VehicleHealth.h"

class ObdClient {
 public:
  static constexpr size_t kMaxRequests = 4;
  static constexpr uint32_t kLinkTimeoutMs = 2000;

  explicit ObdClient(int8_t chipSelectPin);

  // Brings up the MCP2515 and installs a hardware filter for ECU responses.
  // Returns false if the controller does not answer on SPI.
  bool begin(long baudRate);

  // Replaces the rotation. Extra entries beyond kMaxRequests are ignored.
  void setRequests(const ObdRequest* requests, size_t count);

  // Drives the request/response state machine. Cheap; call every loop.
  void update(uint32_t nowMs);

  const TelemetryData& data() const { return data_; }
  const VehicleHealth& health() const { return health_; }

  // True while the ECU has answered within the last kLinkTimeoutMs.
  bool isLinkAlive(uint32_t nowMs) const;

  uint32_t responseCount() const { return responseCount_; }

 private:
  static constexpr uint32_t kResponseTimeoutMs = 300;  // per request
  static constexpr uint32_t kRequestGapMs = 40;        // breathing room on the bus
  static constexpr uint8_t kFlowControlStMinMs = 20;   // keeps consecutive frames
                                                       // inside the MCP2515's 2 RX buffers
  struct Slot {
    ObdRequest request;
    uint32_t lastSentAtMs;
    bool everSent;
  };

  bool pickNextRequest(uint32_t nowMs, size_t* indexOut) const;
  void sendRequest(const Slot& slot, uint32_t nowMs);
  void sendFlowControl(uint16_t responseId);
  void drainIncoming(uint32_t nowMs);
  void handleMessage(uint16_t responseId, const uint8_t* message, size_t length, uint32_t nowMs);
  void finishPendingRequest(uint32_t nowMs);

  Adafruit_MCP2515 can_;
  IsoTpReassembler reassembler_;
  TelemetryData data_;
  VehicleHealth health_;

  Slot slots_[kMaxRequests] = {};
  size_t slotCount_ = 0;
  size_t rotation_ = 0;

  bool awaitingResponse_ = false;
  size_t pendingSlot_ = 0;
  uint32_t requestSentAtMs_ = 0;
  uint32_t nextRequestAtMs_ = 0;
  uint32_t lastResponseAtMs_ = 0;
  uint32_t responseCount_ = 0;
};
