/*
 * IsoTpReassembler.h
 *
 * Minimal ISO 15765-2 (ISO-TP) receive side for a single peer.
 *
 * Feed it every CAN frame from the ECU. Single frames complete immediately.
 * A first frame starts a multi-frame message; the caller must then send a
 * flow-control frame (see flowControlFrame) and keep feeding consecutive
 * frames until feed() reports Complete.
 */
#pragma once

#include <Arduino.h>

class IsoTpReassembler {
 public:
  static constexpr size_t kMaxMessageBytes = 64;
  static constexpr size_t kFrameBytes = 8;

  enum class Result {
    Ignored,     // not part of a message we can use (or still in progress)
    FirstFrame,  // a multi-frame message began; send flow control now
    Complete,    // message() / length() hold a full payload
  };

  Result feed(const uint8_t* frame, size_t frameLength, uint32_t nowMs);

  const uint8_t* message() const { return buffer_; }
  size_t length() const { return expected_; }

  bool inProgress() const { return active_; }
  void reset();

  // Fills an 8-byte CTS flow-control frame: block size 0 (send everything),
  // separation time `stMinMs` between consecutive frames.
  static void flowControlFrame(uint8_t* out, uint8_t stMinMs);

 private:
  static constexpr uint32_t kTimeoutMs = 1000;

  uint8_t buffer_[kMaxMessageBytes] = {};
  size_t expected_ = 0;
  size_t received_ = 0;
  uint8_t nextSequence_ = 0;
  bool active_ = false;
  uint32_t startedAtMs_ = 0;
};
