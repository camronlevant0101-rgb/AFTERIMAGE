// AFTERIMAGE granular engine - real-time safe, JUCE independent.
// All memory is allocated in prepare(); process() never allocates, locks or does I/O.
#pragma once
#include "Params.h"
#include <atomic>
#include <cstdint>
#include <vector>

namespace afterimage
{
constexpr int kMaxVoices = 96;
constexpr int kSubBlock = 32;
constexpr int kVizBins = 512;
constexpr double kBufferSeconds = 4.0;
constexpr int kGuard = 8; // samples kept between a grain read head and the write head

struct HostInfo
{
    bool bpmValid = false;  double bpm = 120.0;
    bool playing = false;
    bool ppqValid = false;  double ppq = 0.0; // ppq at first sample of the block
};

struct GrainViz { std::atomic<float> delayNorm { 0.f }, lenNorm { 0.f }, env { 0.f }, pan { 0.f }; };

// Written by the audio thread (relaxed atomics), read by the GUI thread.
struct VizData
{
    std::atomic<float> peaks[kVizBins];
    GrainViz grains[kMaxVoices];
    std::atomic<int> writeBin { 0 }, binSize { 1 }, visibleBins { kVizBins }, activeGrains { 0 };
    std::atomic<bool> frozen { false }, limiting { false };
    std::atomic<float> lfo1 { 0.f }, lfo2 { 0.f }, inLevel { 0.f }, outLevel { 0.f }, overlap { 0.f };
    VizData() { for (auto& p : peaks) p.store (0.f); }
};

class GranularEngine
{
public:
    GranularEngine();
    void prepare (double sampleRate, int maxBlockSize);   // NOT real-time safe
    void reset();                                         // clears buffer, grains, state
    void setParams (const Params& p) noexcept { params_ = p; }
    void setBypassed (bool b) noexcept { bypassed_ = b; }
    void process (float* L, float* R, int numSamples, const HostInfo& host) noexcept;
    void processMono (float* x, int numSamples, const HostInfo& host) noexcept;

    VizData viz;
    double sampleRate() const noexcept { return sr_; }
    double usableDelaySamples() const noexcept { return D_; }
    std::int64_t grainsSpawned() const noexcept { return spawned_.load (std::memory_order_relaxed); }
    int activeGrainCount() const noexcept { return viz.activeGrains.load (std::memory_order_relaxed); }

private:
    struct Grain
    {
        bool active = false;
        double readPos = 0, inc = 1;
        int age = 0, length = 1;
        float invLen = 1, half = 0.5f, invHalf = 2.f, gl = 1, gr = 1, fade = 1, fadeStep = 0, pan = 0;
    };
    struct Ramp
    {
        float cur = 0, target = 0, step = 0; int remaining = 0;
        void snap (float v) { cur = target = v; remaining = 0; }
        void set (float t, int n) { if (t == target) return; target = t; if (n <= 0) { cur = t; remaining = 0; return; } step = (t - cur) / (float) n; remaining = n; }
        float next() { if (remaining > 0) { cur += step; if (--remaining == 0) cur = target; } return cur; }
        bool active() const { return remaining > 0; }
    };
    struct Lfo { double phase = 0; float rnd = 0, rndTarget = 0; std::uint32_t seed = 1; };
    struct Eff
    {
        double sizeSamples = 1000, delayCenter = 1000, sprayRange = 0, baseInterval = 2400, stepBeats = 0.25;
        float pitch = 0, detune = 0, timeJitter = 0, sizeJitter = 0, taper = 1, panSpread = 0, norm = 1;
        bool reverse = false, randomMode = false, stutter = false, syncActive = false;
    };

    void updateControl (const HostInfo& host, double ppqSub, bool ppqOk, int sub) noexcept;
    void renderSub (float* L, float* R, int n, double ppq0, bool ppqOk, double ppqPerSample) noexcept;
    void housekeeping (int sub) noexcept;
    void spawn() noexcept;
    float evalLfo (Lfo& l, int shape, float rateHz, bool syncOn, double beats, const HostInfo& host, double ppqSub, bool ppqOk, int sub) noexcept;
    float rnd01() noexcept;
    float rise (float u) const noexcept;
    void hardReset() noexcept;

    Params params_;
    Eff eff_;
    bool prepared_ = false, bypassed_ = false, frozen_ = false, primed_ = false, lastPlaying_ = false;
    double sr_ = 48000.0, D_ = 192000.0;
    int bufLen_ = 0, mask_ = 0, binShift_ = 0, rampSamples_ = 480, fadeSamples_ = 144;
    std::vector<float> bufL_, bufR_;
    std::vector<float> riseTable_;
    int writeIdx_ = 0;
    std::int64_t liveCounter_ = 0;
    Grain voices_[kMaxVoices];
    std::atomic<std::int64_t> spawned_ { 0 };
    std::uint32_t rng_ = 22222u;

    double trigCounter_ = 0, nextInterval_ = 0;
    std::int64_t lastCell_ = INT64_MIN;
    double scan_ = 0;
    // stutter anchor
    bool anchorValid_ = false; double anchorDelay_ = 0; std::int64_t anchorCounter_ = 0; int stutterCount_ = 0;

    Lfo lfo1_, lfo2_; float lfoVal1_ = 0, lfoVal2_ = 0;
    Ramp mixRamp_, widthRamp_, outRamp_, fbRamp_;
    float dryG_ = 1, wetG_ = 0;
    float dcxL_ = 0, dcyL_ = 0, dcxR_ = 0, dcyR_ = 0, dcR_ = 0.999f;
    float fbLpL_ = 0, fbLpR_ = 0, fbCoef_ = 0.5f;
    float peakCur_ = 0; int peakBin_ = 0;
    int limitHold_ = 0;
    bool needReset_ = false;
};
} // namespace afterimage
