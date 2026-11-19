#include "GranularEngine.h"
#include <algorithm>
#include <cmath>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_AMD64)
 #include <xmmintrin.h>
 #define AI_HAS_SSE 1
#endif

namespace afterimage
{
namespace
{
struct ScopedFtz
{
#ifdef AI_HAS_SSE
    unsigned old;
    ScopedFtz() { old = _mm_getcsr(); _mm_setcsr (old | 0x8040u); }
    ~ScopedFtz() { _mm_setcsr (old); }
#endif
};

constexpr float kPi = 3.14159265358979f;
constexpr float kIntervals[7] = { -12.f, -7.f, -5.f, 0.f, 5.f, 7.f, 12.f }; // consonant set for RANDOM mode

inline float hermite (float xm1, float x0, float x1, float x2, float t) noexcept
{
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}
inline float sanitize (float x) noexcept { return (std::fabs (x) < 1.0e4f) ? x : 0.f; } // also catches NaN
inline float softLimit (float x, bool& hit) noexcept
{
    const float a = std::fabs (x);
    if (a <= 0.9f) return x;
    hit = true;
    const float o = (a - 0.9f) * 10.f;
    return std::copysign (0.9f + 0.1f * (o / (1.f + o)), x); // C1-continuous, asymptote 1.0
}
inline double frac (double x) noexcept { return x - std::floor (x); }
} // namespace

GranularEngine::GranularEngine() = default;

void GranularEngine::prepare (double sampleRate, int)
{
    sr_ = std::max (8000.0, sampleRate);
    D_ = std::ceil (kBufferSeconds * sr_);
    int len = 1024;
    while ((double) len < D_ + 32.0) len <<= 1;
    bufLen_ = len; mask_ = len - 1;
    bufL_.assign ((size_t) len, 0.f); bufR_.assign ((size_t) len, 0.f);
    int binSize = len / kVizBins; binShift_ = 0;
    while ((1 << binShift_) < binSize) ++binShift_;
    viz.binSize.store (1 << binShift_); viz.visibleBins.store ((int) std::min<double> (kVizBins, D_ / (double) (1 << binShift_)));
    riseTable_.resize (1026);
    for (int i = 0; i < 1026; ++i) riseTable_[(size_t) i] = 0.5f - 0.5f * std::cos (kPi * (float) i / 1024.f);
    rampSamples_ = std::max (1, (int) (0.010 * sr_));
    fadeSamples_ = std::max (8, (int) (0.003 * sr_));
    dcR_ = (float) (1.0 - 2.0 * 3.14159265358979 * 8.0 / sr_);
    fbCoef_ = (float) (1.0 - std::exp (-2.0 * 3.14159265358979 * 8000.0 / sr_));
    prepared_ = true;
    reset();
}

void GranularEngine::hardReset() noexcept
{
    std::fill (bufL_.begin(), bufL_.end(), 0.f); std::fill (bufR_.begin(), bufR_.end(), 0.f);
    for (auto& g : voices_) g = Grain{};
    writeIdx_ = 0; liveCounter_ = 0; trigCounter_ = 0; nextInterval_ = 0; lastCell_ = INT64_MIN; scan_ = 0;
    anchorValid_ = false; stutterCount_ = 0;
    dcxL_ = dcyL_ = dcxR_ = dcyR_ = fbLpL_ = fbLpR_ = 0.f;
    peakCur_ = 0.f; peakBin_ = 0; limitHold_ = 0; needReset_ = false;
    for (auto& p : viz.peaks) p.store (0.f);
    for (auto& g : viz.grains) g.env.store (0.f);
    viz.activeGrains.store (0);
}

void GranularEngine::reset()
{
    if (! prepared_) return;
    hardReset();
    lfo1_ = Lfo{}; lfo2_ = Lfo{}; lfo1_.seed = 12345u; lfo2_.seed = 67890u;
    primed_ = false; frozen_ = false; lastPlaying_ = false;
}

float GranularEngine::rnd01() noexcept
{
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return (float) (rng_ >> 8) * (1.f / 16777216.f);
}

float GranularEngine::rise (float u) const noexcept
{
    u = std::min (1.f, std::max (0.f, u)) * 1024.f;
    const int i = (int) u; const float f = u - (float) i;
    return riseTable_[(size_t) i] + f * (riseTable_[(size_t) i + 1] - riseTable_[(size_t) i]);
}

float GranularEngine::evalLfo (Lfo& l, int shape, float rateHz, bool syncOn, double beats, const HostInfo& host,
                               double ppqSub, bool ppqOk, int sub) noexcept
{
    double np;
    if (syncOn && host.bpmValid && host.bpm > 0.0)
        np = ppqOk ? frac (ppqSub / beats) : frac (l.phase + (host.bpm / 60.0 / beats) * (double) sub / sr_);
    else
        np = frac (l.phase + (double) rateHz * (double) sub / sr_);
    const bool wrapped = np < l.phase;
    l.phase = np;
    if (wrapped)
    {
        l.seed ^= l.seed << 13; l.seed ^= l.seed >> 17; l.seed ^= l.seed << 5;
        l.rndTarget = (float) (l.seed >> 8) * (2.f / 16777216.f) - 1.f;
    }
    const float ph = (float) np;
    switch (shape)
    {
        case ShapeTriangle: return ph < 0.5f ? 4.f * ph - 1.f : 3.f - 4.f * ph;
        case ShapeSquare:   return ph < 0.5f ? 1.f : -1.f;
        case ShapeRandom:   l.rnd += (l.rndTarget - l.rnd) * 0.25f; return l.rnd;
        default:            return std::sin (2.f * kPi * ph);
    }
}

void GranularEngine::updateControl (const HostInfo& host, double ppqSub, bool ppqOk, int sub) noexcept
{
    const Params& p = params_;
    const int mode = std::min (NumModes - 1, std::max (0, (int) (p.mode + 0.5f)));

    // --- LFOs -------------------------------------------------------------
    const int div1 = std::min (kNumDivisions - 1, std::max (0, (int) (p.lfo1Div + 0.5f)));
    const int div2 = std::min (kNumDivisions - 1, std::max (0, (int) (p.lfo2Div + 0.5f)));
    lfoVal1_ = evalLfo (lfo1_, (int) (p.lfo1Shape + 0.5f), p.lfo1Rate, p.lfo1Sync > 0.5f, kDivisionBeats[div1], host, ppqSub, ppqOk, sub);
    lfoVal2_ = evalLfo (lfo2_, (int) (p.lfo2Shape + 0.5f), p.lfo2Rate, p.lfo2Sync > 0.5f, kDivisionBeats[div2], host, ppqSub, ppqOk, sub);
    float modPos = 0, modPitch = 0, modSpray = 0, modWidth = 0, sizeMul = 1, densMul = 1;
    auto apply = [&] (int dest, float v)
    {
        switch (dest)
        {
            case DestPosition: modPos += 0.5f * v; break;
            case DestPitch:    modPitch += 12.f * v; break;
            case DestSize:     sizeMul *= std::exp2 (2.f * v); break;
            case DestDensity:  densMul *= std::exp2 (2.f * v); break;
            case DestSpray:    modSpray += 0.5f * v; break;
            case DestWidth:    modWidth += v; break;
            default: break;
        }
    };
    apply ((int) (p.lfo1Dest + 0.5f), lfoVal1_ * p.lfo1Amount);
    apply ((int) (p.lfo2Dest + 0.5f), lfoVal2_ * p.lfo2Amount);

    // --- base values --------------------------------------------------------
    float posN = std::min (1.f, std::max (0.f, p.position + modPos));
    float pitch = std::min (36.f, std::max (-36.f, p.pitch + modPitch));
    double sizeMs = std::min (1000.0, std::max (2.0, (double) p.size * sizeMul));
    double dens = std::min (400.0, std::max (0.25, (double) p.density * densMul * std::exp2 (4.f * (p.overlap - 0.5f))));
    float spray = std::min (1.f, std::max (0.f, p.spray + modSpray));
    float width = std::min (2.f, std::max (0.f, p.width + modWidth));
    float taper = 1.f - 0.95f * std::min (1.f, std::max (0.f, p.shape));

    Eff e;
    e.reverse = p.reverse > 0.5f;
    e.syncActive = p.sync > 0.5f && host.bpmValid && host.bpm > 0.0;
    const int div = std::min (kNumDivisions - 1, std::max (0, (int) (p.division + 0.5f)));
    e.stepBeats = kDivisionBeats[div];
    const double stepMs = e.syncActive ? 60000.0 / host.bpm * e.stepBeats : 0.0;
    bool scanEnabled = true;
    float panBase = 0.25f;

    switch (mode)
    {
        case ModeTexture:
            sizeMs = std::min (1000.0, std::max (sizeMs * 2.5, 100.0));
            dens = std::min (400.0, std::max (dens, 4.0 / (sizeMs * 0.001)));
            e.timeJitter = 0.6f; spray = std::min (1.f, std::max (spray * 1.5f, 0.1f));
            taper = std::max (taper, 0.7f); panBase = 0.9f; break;
        case ModeStutter:
            sizeMs = std::min (sizeMs, 150.0);
            if (e.syncActive) sizeMs = std::min (sizeMs, stepMs * 0.98);
            spray *= 0.05f; taper = std::min (taper, 0.3f); panBase = 0.f; e.stutter = true; scanEnabled = false; break;
        case ModePitch:
            sizeMs = std::min (70.0, std::max (20.0, sizeMs));
            dens = std::min (400.0, 4.0 / (sizeMs * 0.001) * std::exp2 (4.f * (p.overlap - 0.5f)));
            e.detune = spray * 0.35f; spray *= 0.02f; panBase = 0.1f; scanEnabled = false; break;
        case ModeReverse:
            e.reverse = true; panBase = 0.3f; break;
        case ModeTime:
            sizeMs = std::min (250.0, std::max (40.0, sizeMs));
            dens = std::min (400.0, std::max (dens, 4.0 / (sizeMs * 0.001)));
            spray *= 0.25f; panBase = 0.1f; break;
        case ModeRandom:
            e.randomMode = true; e.sizeJitter = 0.5f; e.timeJitter = 0.8f;
            spray = std::min (1.f, std::max (spray * 2.f, 0.3f)); panBase = 1.f; break;
        default: e.timeJitter = 0.05f; break;
    }

    // --- scan (speed) ----------------------------------------------------
    const double posD = (double) posN * D_;
    if (scanEnabled)
    {
        if (std::fabs (p.speed - 1.f) < 0.01f) scan_ *= 0.998;
        else scan_ += (1.0 - (double) p.speed) * (double) sub;
        const double range = std::max (1.0, D_ - 2.0 * kGuard);
        const double tot = posD + scan_;
        if (tot > D_) scan_ -= range * std::ceil ((tot - D_) / range);
        else if (tot < kGuard) scan_ += range * std::ceil (((double) kGuard - tot) / range);
    }
    else scan_ = 0.0;

    e.delayCenter = std::min (D_, std::max ((double) kGuard, posD + scan_));
    e.sizeSamples = sizeMs * 0.001 * sr_;
    e.pitch = pitch;
    e.sprayRange = (double) spray * 0.75 * sr_;
    e.taper = std::min (1.f, std::max (0.05f, taper));
    e.panSpread = panBase * std::min (1.f, width * 0.5f + 0.25f);
    const double rateHz = e.syncActive ? 1000.0 / stepMs : dens;
    e.baseInterval = std::max (1.0, sr_ / rateHz);
    const double N = rateHz * sizeMs * 0.001;
    e.norm = (float) (1.0 / std::max (1.0, std::pow (0.5 * N, 0.85)));
    eff_ = e;
    viz.overlap.store ((float) N, std::memory_order_relaxed);

    // freeze transitions
    const bool fr = p.freeze > 0.5f;
    if (fr != frozen_) { frozen_ = fr; anchorValid_ = false; }
    if (nextInterval_ > e.baseInterval * (1.0 + e.timeJitter)) nextInterval_ = e.baseInterval * (1.0 + e.timeJitter);

    // --- smoothed targets --------------------------------------------------
    const float mixT = bypassed_ ? 0.f : std::min (1.f, std::max (0.f, p.mix));
    const float outT = bypassed_ ? 1.f : std::pow (10.f, std::min (12.f, std::max (-36.f, p.output)) / 20.f);
    const float fbT = bypassed_ ? 0.f : std::min (1.f, std::max (0.f, p.feedback)) * 0.92f;
    if (! primed_)
    {
        mixRamp_.snap (mixT); widthRamp_.snap (width); outRamp_.snap (outT); fbRamp_.snap (fbT);
        dryG_ = std::cos (mixT * 0.5f * kPi); wetG_ = std::sin (mixT * 0.5f * kPi);
        primed_ = true;
    }
    mixRamp_.set (mixT, rampSamples_); widthRamp_.set (width, rampSamples_);
    outRamp_.set (outT, rampSamples_); fbRamp_.set (fbT, rampSamples_);
}

void GranularEngine::spawn() noexcept
{
    int vi = -1;
    for (int i = 0; i < kMaxVoices; ++i) if (! voices_[i].active) { vi = i; break; }
    if (vi < 0) return;
    const Eff& e = eff_;

    double sizeS = e.sizeSamples;
    if (e.sizeJitter > 0.f) sizeS *= std::exp2 (e.sizeJitter * (rnd01() * 2.f - 1.f));
    sizeS = std::min (sr_, std::max (16.0, sizeS));

    float pitch = e.pitch; bool rev = e.reverse;
    if (e.randomMode)
    {
        if (rnd01() < 0.5f) pitch += kIntervals[(int) (rnd01() * 6.999f)];
        if (rnd01() < 0.25f) rev = ! rev;
    }
    if (e.detune > 0.f) pitch += e.detune * (rnd01() * 2.f - 1.f);
    pitch = std::min (48.f, std::max (-48.f, pitch));
    const double ratio = std::exp2 ((double) pitch / 12.0);
    const double dir = rev ? -1.0 : 1.0;

    const double maxL = (D_ - 2.0 * kGuard) / (1.0 + ratio);
    const int length = std::max (16, (int) std::min (sizeS, maxL));

    double delay = e.delayCenter + (double) (rnd01() * 2.f - 1.f) * e.sprayRange;
    if (e.stutter)
    {
        if (! anchorValid_ || (stutterCount_ % 4) == 0)
        {
            anchorDelay_ = delay; anchorCounter_ = liveCounter_; anchorValid_ = true;
        }
        ++stutterCount_;
        delay = anchorDelay_ + (frozen_ ? 0.0 : (double) (liveCounter_ - anchorCounter_));
    }
    const double w = frozen_ ? 0.0 : 1.0;
    const double delta = (double) length * (w - dir * ratio);
    double lo = (double) kGuard + std::max (0.0, -delta);
    double hi = D_ - std::max (0.0, delta);
    if (lo > hi) lo = hi = 0.5 * (lo + hi);
    delay = std::min (hi, std::max (lo, delay));

    Grain& g = voices_[vi];
    g.active = true;
    g.readPos = (double) writeIdx_ - delay;
    while (g.readPos < 0.0) g.readPos += (double) bufLen_;
    g.inc = dir * ratio;
    g.age = 0; g.length = length; g.invLen = 1.f / (float) length;
    g.half = e.taper * 0.5f; g.invHalf = 2.f / e.taper;
    g.fade = 1.f; g.fadeStep = 0.f;
    g.pan = (rnd01() * 2.f - 1.f) * e.panSpread;
    const float gl = std::cos (std::max (0.f, g.pan) * 0.5f * kPi);
    const float gr = std::cos (std::max (0.f, -g.pan) * 0.5f * kPi);
    g.gl = e.norm * gl; g.gr = e.norm * gr;
    spawned_.fetch_add (1, std::memory_order_relaxed);
}

void GranularEngine::housekeeping (int sub) noexcept
{
    // Safety net: fade out any grain that is about to cross the write head
    // (e.g. after a freeze toggle) so there is never a discontinuity.
    const double w = frozen_ ? 0.0 : 1.0;
    int active = 0;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        Grain& g = voices_[i];
        if (! g.active) { viz.grains[i].env.store (0.f, std::memory_order_relaxed); continue; }
        ++active;
        double d = (double) writeIdx_ - g.readPos; if (d < 0.0) d += (double) bufLen_;
        if (g.fadeStep == 0.f)
        {
            const double look = (double) std::min (fadeSamples_ + sub + 2, g.length - g.age);
            const double proj = d + (w - g.inc) * look;
            if (d < kGuard - 1 || proj < kGuard - 1 || proj > (double) (bufLen_ - kGuard) || d > (double) (bufLen_ - kGuard))
                g.fadeStep = -1.f / (float) fadeSamples_;
        }
        float p = (float) g.age * g.invLen, env = 1.f;
        if (p < g.half) env = rise (p * g.invHalf); else if (p > 1.f - g.half) env = rise ((1.f - p) * g.invHalf);
        auto& v = viz.grains[i];
        v.delayNorm.store ((float) (d / D_), std::memory_order_relaxed);
        v.lenNorm.store ((float) ((double) g.length / D_), std::memory_order_relaxed);
        v.env.store (env * g.fade, std::memory_order_relaxed);
        v.pan.store (g.pan, std::memory_order_relaxed);
    }
    viz.activeGrains.store (active, std::memory_order_relaxed);
}

void GranularEngine::renderSub (float* L, float* R, int n, double ppq0, bool ppqOk, double ppqPerSample) noexcept
{
    bool hit = false;
    const bool useGrid = eff_.syncActive && ppqOk;
    for (int i = 0; i < n; ++i)
    {
        const float inL = sanitize (L[i]), inR = sanitize (R[i]);

        // --- trigger -----------------------------------------------------
        if (useGrid)
        {
            const auto cell = (std::int64_t) std::floor ((ppq0 + (double) i * ppqPerSample) / eff_.stepBeats + 1.0e-9);
            if (cell != lastCell_) { lastCell_ = cell; spawn(); }
        }
        else
        {
            trigCounter_ += 1.0;
            if (trigCounter_ >= nextInterval_)
            {
                trigCounter_ -= nextInterval_; if (trigCounter_ > eff_.baseInterval) trigCounter_ = 0.0;
                spawn();
                nextInterval_ = std::max (1.0, eff_.baseInterval * (1.0 + (double) eff_.timeJitter * (double) (rnd01() * 2.f - 1.f) * 0.9));
            }
        }

        // --- capture -----------------------------------------------------
        const float fbg = fbRamp_.next();
        if (! frozen_)
        {
            bufL_[(size_t) writeIdx_] = inL + fbLpL_ * fbg;
            bufR_[(size_t) writeIdx_] = inR + fbLpR_ * fbg;
            const int bin = writeIdx_ >> binShift_;
            if (bin != peakBin_) { viz.peaks[peakBin_ & (kVizBins - 1)].store (peakCur_, std::memory_order_relaxed); peakCur_ = 0.f; peakBin_ = bin; }
            peakCur_ = std::max (peakCur_, std::fabs (0.5f * (inL + inR)));
            writeIdx_ = (writeIdx_ + 1) & mask_;
            ++liveCounter_;
        }

        // --- grains --------------------------------------------------------
        float sL = 0.f, sR = 0.f;
        for (int v = 0; v < kMaxVoices; ++v)
        {
            Grain& g = voices_[v];
            if (! g.active) continue;
            const float p = (float) g.age * g.invLen;
            float env = 1.f;
            if (p < g.half) env = rise (p * g.invHalf); else if (p > 1.f - g.half) env = rise ((1.f - p) * g.invHalf);
            const int i0 = (int) g.readPos;
            const float t = (float) (g.readPos - (double) i0);
            const int im = (i0 - 1) & mask_, ia = i0 & mask_, ib = (i0 + 1) & mask_, ic = (i0 + 2) & mask_;
            const float amp = env * g.fade;
            sL += hermite (bufL_[(size_t) im], bufL_[(size_t) ia], bufL_[(size_t) ib], bufL_[(size_t) ic], t) * amp * g.gl;
            sR += hermite (bufR_[(size_t) im], bufR_[(size_t) ia], bufR_[(size_t) ib], bufR_[(size_t) ic], t) * amp * g.gr;
            g.readPos += g.inc;
            if (g.readPos < 0.0) g.readPos += (double) bufLen_; else if (g.readPos >= (double) bufLen_) g.readPos -= (double) bufLen_;
            if (++g.age >= g.length) g.active = false;
            if (g.fadeStep != 0.f) { g.fade += g.fadeStep; if (g.fade <= 0.f) g.active = false; }
        }

        // --- wet chain: DC blocker -> safety soft limiter -----------------------
        float wL = sL - dcxL_ + dcR_ * dcyL_; dcxL_ = sL; dcyL_ = wL;
        float wR = sR - dcxR_ + dcR_ * dcyR_; dcxR_ = sR; dcyR_ = wR;
        if (std::fabs (dcyL_) < 1.0e-20f) dcyL_ = 0.f;
        if (std::fabs (dcyR_) < 1.0e-20f) dcyR_ = 0.f;
        wL = softLimit (wL, hit); wR = softLimit (wR, hit);
        fbLpL_ += fbCoef_ * (wL - fbLpL_); fbLpR_ += fbCoef_ * (wR - fbLpR_);
        if (std::fabs (fbLpL_) < 1.0e-20f) fbLpL_ = 0.f;
        if (std::fabs (fbLpR_) < 1.0e-20f) fbLpR_ = 0.f;

        // --- width (mid/side) ---------------------------------------------
        const float wd = widthRamp_.next();
        const float m = 0.5f * (wL + wR), s = 0.5f * (wL - wR) * wd;
        wL = m + s; wR = m - s;

        // --- equal-power mix + output ------------------------------------------
        const float mx = mixRamp_.next();
        if (mixRamp_.active() || mx != mixRamp_.target) { dryG_ = std::cos (mx * 0.5f * kPi); wetG_ = std::sin (mx * 0.5f * kPi); }
        else { dryG_ = std::cos (mixRamp_.target * 0.5f * kPi); wetG_ = std::sin (mixRamp_.target * 0.5f * kPi); }
        const float og = outRamp_.next();
        float oL = (inL * dryG_ + wL * wetG_) * og;
        float oR = (inR * dryG_ + wR * wetG_) * og;
        if (! (std::fabs (oL) < 16.f)) { if (! std::isfinite (oL)) needReset_ = true; oL = std::copysign (std::isfinite (oL) ? 16.f : 0.f, oL); hit = true; }
        if (! (std::fabs (oR) < 16.f)) { if (! std::isfinite (oR)) needReset_ = true; oR = std::copysign (std::isfinite (oR) ? 16.f : 0.f, oR); hit = true; }
        L[i] = oL; R[i] = oR;
    }
    if (hit) limitHold_ = (int) (0.25 * sr_); else limitHold_ = std::max (0, limitHold_ - n);
    viz.limiting.store (limitHold_ > 0, std::memory_order_relaxed);
}

void GranularEngine::process (float* L, float* R, int numSamples, const HostInfo& host) noexcept
{
    if (! prepared_ || numSamples <= 0) return;
    ScopedFtz ftz;
    if (host.playing != lastPlaying_) { lastPlaying_ = host.playing; lastCell_ = INT64_MIN; }
    const bool ppqOk = host.playing && host.ppqValid && host.bpmValid && host.bpm > 0.0;
    const double ppqPerSample = host.bpmValid && host.bpm > 0.0 ? host.bpm / 60.0 / sr_ : 0.0;
    viz.frozen.store (params_.freeze > 0.5f, std::memory_order_relaxed);

    int done = 0;
    while (done < numSamples)
    {
        const int sub = std::min (kSubBlock, numSamples - done);
        const double ppqSub = host.ppq + (double) done * ppqPerSample;
        updateControl (host, ppqSub, ppqOk, sub);
        renderSub (L + done, R + done, sub, ppqSub, ppqOk, ppqPerSample);
        housekeeping (sub);
        viz.lfo1.store (lfoVal1_, std::memory_order_relaxed);
        viz.lfo2.store (lfoVal2_, std::memory_order_relaxed);
        done += sub;
    }
    float lvl = 0.f; for (int i = 0; i < numSamples; ++i) lvl = std::max (lvl, std::fabs (L[i]));
    viz.outLevel.store (lvl, std::memory_order_relaxed);
    viz.peaks[peakBin_ & (kVizBins - 1)].store (peakCur_, std::memory_order_relaxed);
    viz.writeBin.store (peakBin_ & (kVizBins - 1), std::memory_order_relaxed);
    if (needReset_) hardReset(); // NaN/Inf escaped somewhere: wipe state rather than propagate
}

void GranularEngine::processMono (float* x, int numSamples, const HostInfo& host) noexcept
{
    float tmp[kSubBlock];
    const double ppqPerSample = host.bpmValid && host.bpm > 0.0 ? host.bpm / 60.0 / sr_ : 0.0;
    for (int off = 0; off < numSamples; off += kSubBlock)
    {
        const int n = std::min (kSubBlock, numSamples - off);
        for (int i = 0; i < n; ++i) tmp[i] = x[off + i];
        HostInfo h = host; h.ppq = host.ppq + (double) off * ppqPerSample;
        process (x + off, tmp, n, h);
        for (int i = 0; i < n; ++i) x[off + i] = 0.5f * (x[off + i] + tmp[i]);
    }
}
} // namespace afterimage
