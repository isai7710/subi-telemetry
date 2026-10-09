/*
 * ObdRequest.h
 *
 * One entry in the poller's rotation: which service (and PID) to ask for,
 * which CAN ID to send it to, and how often it may be repeated.
 *
 * Numeric PIDs go to the functional address so the request is cheap and
 * generic. Diagnostic services go to the ECM's physical address so exactly
 * one module answers and multi-frame responses never interleave.
 */
#pragma once

#include <Arduino.h>

#include "ObdPid.h"

struct ObdRequest {
  uint8_t mode;
  bool hasPid;
  uint8_t pid;
  uint16_t targetId;
  uint32_t periodMs;  // 0 = as fast as the rotation allows

  static constexpr ObdRequest currentData(Obd::Pid pid, uint32_t periodMs = 0) {
    return ObdRequest{Obd::kModeCurrentData, true, static_cast<uint8_t>(pid),
                      Obd::kFunctionalRequestId, periodMs};
  }

  static constexpr ObdRequest ecmService(uint8_t mode, uint32_t periodMs) {
    return ObdRequest{mode, false, 0, Obd::kEcmPhysicalRequestId, periodMs};
  }

  // True if `message` (a positive response payload) answers this request.
  bool matches(const uint8_t* message, size_t length) const {
    if (length < 1 || message[0] != mode + Obd::kPositiveResponseOffset) return false;
    if (!hasPid) return true;
    return length >= 2 && message[1] == pid;
  }
};
