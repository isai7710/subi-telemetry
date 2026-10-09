#include "ObdClient.h"

ObdClient::ObdClient(int8_t chipSelectPin) : can_(chipSelectPin) {}

bool ObdClient::begin(long baudRate) {
  if (!can_.begin(baudRate)) {
    return false;
  }
  // Only ECU responses (0x7E8..0x7EF) reach the RX buffers; everything else on
  // the bus is dropped by the MCP2515 itself, so the RP2040 never sees it.
  can_.filter(Obd::kEcuResponseIdBase, Obd::kEcuResponseIdMask);
  return true;
}

void ObdClient::setRequests(const ObdRequest* requests, size_t count) {
  slotCount_ = 0;
  for (size_t i = 0; i < count && slotCount_ < kMaxRequests; ++i) {
    slots_[slotCount_++] = Slot{requests[i], 0, false};
  }
  rotation_ = 0;
  // Let any in-flight request finish on its own; the next one uses the new list.
}

void ObdClient::update(uint32_t nowMs) {
  drainIncoming(nowMs);

  if (awaitingResponse_ && !reassembler_.inProgress() &&
      (nowMs - requestSentAtMs_) >= kResponseTimeoutMs) {
    finishPendingRequest(nowMs);  // ECU did not answer; don't stall the others
  }

  if (!awaitingResponse_ && static_cast<int32_t>(nowMs - nextRequestAtMs_) >= 0) {
    size_t index;
    if (pickNextRequest(nowMs, &index)) {
      sendRequest(slots_[index], nowMs);
      slots_[index].lastSentAtMs = nowMs;
      slots_[index].everSent = true;
      pendingSlot_ = index;
      rotation_ = (index + 1) % slotCount_;
    }
  }
}

bool ObdClient::isLinkAlive(uint32_t nowMs) const {
  return responseCount_ > 0 && (nowMs - lastResponseAtMs_) <= kLinkTimeoutMs;
}

// Round-robin from the rotation cursor, skipping entries whose period has not
// elapsed yet. Returns false if nothing is due.
bool ObdClient::pickNextRequest(uint32_t nowMs, size_t* indexOut) const {
  for (size_t n = 0; n < slotCount_; ++n) {
    const size_t i = (rotation_ + n) % slotCount_;
    const Slot& slot = slots_[i];
    if (!slot.everSent || (nowMs - slot.lastSentAtMs) >= slot.request.periodMs) {
      *indexOut = i;
      return true;
    }
  }
  return false;
}

void ObdClient::sendRequest(const Slot& slot, uint32_t nowMs) {
  const ObdRequest& req = slot.request;
  const uint8_t payloadLength = req.hasPid ? 2 : 1;

  // ISO-TP single frame: [length][mode][pid?][pad to 8]
  can_.beginPacket(req.targetId);
  can_.write(payloadLength);
  can_.write(req.mode);
  if (req.hasPid) can_.write(req.pid);
  for (uint8_t i = 1 + payloadLength; i < IsoTpReassembler::kFrameBytes; ++i) {
    can_.write(static_cast<uint8_t>(0x00));
  }
  can_.endPacket();

  awaitingResponse_ = true;
  requestSentAtMs_ = nowMs;
}

void ObdClient::sendFlowControl(uint16_t responseId) {
  uint8_t frame[IsoTpReassembler::kFrameBytes];
  IsoTpReassembler::flowControlFrame(frame, kFlowControlStMinMs);
  can_.beginPacket(Obd::physicalRequestIdFor(responseId));
  can_.write(frame, sizeof(frame));
  can_.endPacket();
}

void ObdClient::drainIncoming(uint32_t nowMs) {
  while (can_.parsePacket() > 0) {
    const uint16_t id = static_cast<uint16_t>(can_.packetId());

    uint8_t frame[IsoTpReassembler::kFrameBytes];
    size_t length = 0;
    while (can_.available() && length < sizeof(frame)) {
      frame[length++] = static_cast<uint8_t>(can_.read());
    }

    if ((id & Obd::kEcuResponseIdMask) != Obd::kEcuResponseIdBase) {
      continue;  // belt and braces; the hardware filter should already drop this
    }

    switch (reassembler_.feed(frame, length, nowMs)) {
      case IsoTpReassembler::Result::FirstFrame:
        sendFlowControl(id);
        break;
      case IsoTpReassembler::Result::Complete:
        handleMessage(id, reassembler_.message(), reassembler_.length(), nowMs);
        break;
      case IsoTpReassembler::Result::Ignored:
      default:
        break;
    }
  }
}

// `message` is a complete positive-response payload: [SID+0x40][...].
void ObdClient::handleMessage(uint16_t responseId, const uint8_t* message, size_t length,
                              uint32_t nowMs) {
  if (length < 1) return;
  const uint8_t sid = message[0];
  bool consumed = false;

  if (sid == Obd::kModeCurrentData + Obd::kPositiveResponseOffset && length >= 2) {
    const Obd::Pid pid = static_cast<Obd::Pid>(message[1]);
    if (pid == Obd::Pid::MonitorStatus) {
      if (length >= 6 && responseId == Obd::kEcmResponseId) {
        health_.status.parse(&message[2], nowMs);
        consumed = true;
      }
    } else if (const Obd::PidDef* def = Obd::findPidDef(pid)) {
      if (length >= 2u + def->dataBytes) {
        data_.set(def->channel, def->decode(&message[2]), nowMs);
        consumed = true;
      }
    }
  } else if (sid == Obd::kModeStoredDtcs + Obd::kPositiveResponseOffset) {
    health_.stored.parse(&message[1], length - 1, nowMs);
    consumed = true;
  } else if (sid == Obd::kModePendingDtcs + Obd::kPositiveResponseOffset) {
    health_.pending.parse(&message[1], length - 1, nowMs);
    consumed = true;
  }

  if (!consumed) return;

  lastResponseAtMs_ = nowMs;
  ++responseCount_;

  if (awaitingResponse_ && slots_[pendingSlot_].request.matches(message, length)) {
    finishPendingRequest(nowMs);
  }
}

void ObdClient::finishPendingRequest(uint32_t nowMs) {
  awaitingResponse_ = false;
  nextRequestAtMs_ = nowMs + kRequestGapMs;
}
