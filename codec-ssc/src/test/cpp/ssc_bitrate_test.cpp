#include "ssc_bitrate.h"

#include <cassert>

using namespace sscport;

static void feed(BitrateLadder& ladder, size_t queue, int count) {
    for (int i = 0; i < count; ++i) ladder.report_queue(queue);
}

int main() {
    using M = BitrateLadder::Mode;
    BitrateLadder ladder;
    ladder.reset(true);
    assert(ladder.bitrate(M::kAdaptive) == 584000);
    // Level 2 after three consecutive reports with queue >= 2, not before.
    feed(ladder, 2, 2);
    assert(ladder.level() == 0);
    feed(ladder, 0, 1);
    feed(ladder, 3, 2);
    assert(ladder.level() == 0);
    assert(ladder.report_queue(3) && ladder.level() == 2);
    assert(ladder.bitrate(M::kAdaptive) == 291000);
    // Level 3 after three reports with queue >= 4.
    feed(ladder, 5, 2);
    assert(ladder.level() == 2);
    assert(ladder.report_queue(5) && ladder.level() == 3);
    assert(ladder.bitrate(M::kAdaptive) == 152000);
    // Recovery: 100 short-queue reports per level; an interruption restarts the count.
    feed(ladder, 1, 99);
    feed(ladder, 2, 1);
    feed(ladder, 1, 99);
    assert(ladder.level() == 3);
    feed(ladder, 1, 1);
    assert(ladder.level() == 2);
    feed(ladder, 0, 100);
    assert(ladder.level() == 0);
    // From level 0 a severe queue goes straight to level 3.
    feed(ladder, 4, 3);
    assert(ladder.level() == 3);
    // Fixed modes ignore the adaptive level.
    assert(ladder.bitrate(M::kFixedHigh) == 584000);
    assert(ladder.bitrate(M::kFixedMedium) == 291000);
    assert(ladder.bitrate(M::kFixedLow) == 152000);
    // 48 kHz table; reset returns to level 0.
    ladder.reset(false);
    assert(ladder.level() == 0 && ladder.bitrate(M::kAdaptive) == 192000);
    feed(ladder, 2, 3);
    assert(ladder.bitrate(M::kAdaptive) == 128000);
    assert(ladder.bitrate(M::kFixedLow) == 88000);
}
