#include "ssc_bitrate.h"

namespace ssc {

void BitrateLadder::reset(bool uhq) {
    uhq_ = uhq;
    level_ = 0;
    degrade_streak_ = severe_streak_ = recovery_streak_ = 0;
}

bool BitrateLadder::report_queue(size_t queue_length) {
    const uint32_t previous = level_;
    if (level_ >= 3) {
        degrade_streak_ = severe_streak_ = 0;
        if (queue_length < kLevel2Queue) {
            if (++recovery_streak_ >= kRecoveryConfirm) {
                level_ = 2;
                recovery_streak_ = 0;
            }
        } else {
            recovery_streak_ = 0;
        }
    } else if (level_ >= 2) {
        degrade_streak_ = 0;
        if (queue_length >= kLevel3Queue) {
            recovery_streak_ = 0;
            if (++severe_streak_ >= kDegradeConfirm) {
                level_ = 3;
                severe_streak_ = 0;
            }
        } else if (queue_length < kLevel2Queue) {
            severe_streak_ = 0;
            if (++recovery_streak_ >= kRecoveryConfirm) {
                level_ = 0;
                recovery_streak_ = 0;
            }
        } else {
            severe_streak_ = recovery_streak_ = 0;
        }
    } else {
        recovery_streak_ = 0;
        if (queue_length >= kLevel3Queue) {
            ++degrade_streak_;
            if (++severe_streak_ >= kDegradeConfirm) {
                level_ = 3;
                degrade_streak_ = severe_streak_ = 0;
            }
        } else if (queue_length >= kLevel2Queue) {
            severe_streak_ = 0;
            if (++degrade_streak_ >= kDegradeConfirm) {
                level_ = 2;
                degrade_streak_ = 0;
            }
        } else {
            degrade_streak_ = severe_streak_ = 0;
        }
    }
    return level_ != previous;
}

int32_t BitrateLadder::bitrate(Mode mode) const {
    const uint32_t level = mode == kFixedHigh ? 0 : mode == kFixedMedium ? 2
                         : mode == kFixedLow ? 3 : level_;
    if (level >= 3) return uhq_ ? 152000 : 88000;
    if (level >= 2) return uhq_ ? 291000 : 128000;
    return uhq_ ? 584000 : 192000;
}

}  // namespace ssc
