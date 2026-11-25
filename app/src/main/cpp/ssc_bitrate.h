#pragma once

#include <stddef.h>
#include <stdint.h>

namespace sscport {

// SSC bitrate by link busy level, with Samsung's per-profile tables:
// SSC UHQ 96 kHz level 0/2/3 = 584000/291000/152000, SSC 48 kHz = 192000/128000/88000.
// Samsung phones get the busy level from Bluetooth controller events, which this phone does not
// send; in adaptive mode the level follows the transmit queue the stack reports every timer tick
// (the state machine of the earlier app/ implementation, 0.1.43):
//   queue >= 2 for 3 consecutive reports -> level 2, queue >= 4 for 3 reports -> level 3,
//   queue < 2 for 100 consecutive reports -> one level back (3 -> 2 -> 0).
// The fixed modes pin a level, as chosen in developer options.
class BitrateLadder {
public:
    enum Mode : uint32_t { kAdaptive = 0, kFixedHigh = 1, kFixedMedium = 2, kFixedLow = 3 };
    static constexpr size_t kLevel2Queue = 2;
    static constexpr size_t kLevel3Queue = 4;
    static constexpr uint32_t kDegradeConfirm = 3;
    static constexpr uint32_t kRecoveryConfirm = 100;

    void reset(bool uhq);
    // One transmit queue report; returns true when the adaptive level changed.
    bool report_queue(size_t queue_length);
    int32_t bitrate(Mode mode) const;
    uint32_t level() const { return level_; }

private:
    bool uhq_ = false;
    uint32_t level_ = 0;
    uint32_t degrade_streak_ = 0;
    uint32_t severe_streak_ = 0;
    uint32_t recovery_streak_ = 0;
};

}  // namespace sscport
