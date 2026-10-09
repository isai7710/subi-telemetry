#include "IsoTpReassembler.h"

#include <string.h>

namespace {
constexpr uint8_t kPciSingle = 0x0;
constexpr uint8_t kPciFirst = 0x1;
constexpr uint8_t kPciConsecutive = 0x2;
constexpr uint8_t kPciFlowControl = 0x3;
}  // namespace

IsoTpReassembler::Result IsoTpReassembler::feed(const uint8_t* frame, size_t frameLength,
                                                uint32_t nowMs) {
  if (frameLength == 0) return Result::Ignored;
  if (active_ && (nowMs - startedAtMs_) > kTimeoutMs) reset();

  const uint8_t pci = frame[0] >> 4;

  switch (pci) {
    case kPciSingle: {
      const size_t n = frame[0] & 0x0F;
      if (n == 0 || n > kFrameBytes - 1 || frameLength < n + 1) return Result::Ignored;
      reset();
      memcpy(buffer_, &frame[1], n);
      expected_ = n;
      received_ = n;
      return Result::Complete;
    }

    case kPciFirst: {
      if (frameLength < kFrameBytes) return Result::Ignored;
      const size_t n = (static_cast<size_t>(frame[0] & 0x0F) << 8) | frame[1];
      if (n < kFrameBytes || n > kMaxMessageBytes) {
        reset();
        return Result::Ignored;
      }
      memcpy(buffer_, &frame[2], kFrameBytes - 2);
      expected_ = n;
      received_ = kFrameBytes - 2;
      nextSequence_ = 1;
      active_ = true;
      startedAtMs_ = nowMs;
      return Result::FirstFrame;
    }

    case kPciConsecutive: {
      if (!active_) return Result::Ignored;
      const uint8_t sequence = frame[0] & 0x0F;
      if (sequence != nextSequence_) {
        reset();
        return Result::Ignored;
      }
      const size_t available = frameLength - 1;
      const size_t wanted = expected_ - received_;
      const size_t n = (available < wanted) ? available : wanted;
      memcpy(&buffer_[received_], &frame[1], n);
      received_ += n;
      nextSequence_ = (nextSequence_ + 1) & 0x0F;
      if (received_ >= expected_) {
        active_ = false;
        return Result::Complete;
      }
      return Result::Ignored;
    }

    case kPciFlowControl:
    default:
      return Result::Ignored;
  }
}

void IsoTpReassembler::reset() {
  expected_ = 0;
  received_ = 0;
  nextSequence_ = 0;
  active_ = false;
}

void IsoTpReassembler::flowControlFrame(uint8_t* out, uint8_t stMinMs) {
  memset(out, 0, kFrameBytes);
  out[0] = (kPciFlowControl << 4) | 0x0;  // CTS
  out[1] = 0x00;                          // block size: unlimited
  out[2] = stMinMs;                       // separation time, ms
}
