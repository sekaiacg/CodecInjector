#pragma once

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <mutex>

namespace port {

// Binaural rendering before the SSC encoder with the device's own Dolby Spatializer, used as a
// client of the AIDL audio effect HAL (the effect runs in the audio HAL process, as it does for
// the system Spatializer). One FMQ round trip per encoder frame; a frame that cannot be processed
// passes through unchanged.
class DolbyHalStage {
public:
    DolbyHalStage();
    ~DolbyHalStage();

    // Creates and starts the effect for one SSC session. Returns false when the HAL is unavailable.
    bool start(uint32_t sample_rate, size_t frames);
    void stop();
    // In-place on interleaved stereo Q8.23 samples (the SSC encoder input format).
    void process(int32_t* pcm, size_t frames);

    void set_enabled(bool enabled) { __atomic_store_n(&enabled_, enabled, __ATOMIC_RELAXED); }
    // Head yaw in radians, AOSP rotation-vector convention (positive = head turned left).
    void set_yaw(float radians);
    bool running() const;

private:
    struct Session;
    mutable std::mutex mutex_;
    std::unique_ptr<Session> session_;
    bool enabled_ = true;
    uint32_t yaw_bits_ = 0;
};

}  // namespace port
