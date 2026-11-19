// AFTERIMAGE native DSP test-suite (no JUCE needed). Build: see CMakeLists / README.
#include "GranularEngine.h"
#include "ParamTable.h"
#include "PresetIO.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <new>
#include <thread>
#include <vector>

using namespace afterimage;

// ---- allocation counter (proves no heap use inside process()) ------------------
static std::atomic<long> gAllocs { 0 };
static std::atomic<bool> gCount { false };
void* operator new (std::size_t n) { if (gCount.load()) ++gAllocs; if (void* p = std::malloc (n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }

static int gFail = 0, gPass = 0;
#define CHECK(cond, ...) do { if (cond) ++gPass; else { ++gFail; std::printf ("  FAIL %s:%d  %s  ", __FILE__, __LINE__, #cond); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)
static void section (const char* s) { std::printf ("[%s]\n", s); }

constexpr double kPi = 3.14159265358979323846;
using Gen = std::function<float (long)>;
struct Out { std::vector<float> L, R; };

static Out run (GranularEngine& e, const Params& p, long n, const Gen& gl, const Gen& gr, int block = 256,
                HostInfo host = {}, double startPpq = 0.0, long startIdx = 0)
{
    Out o; o.L.resize ((size_t) n); o.R.resize ((size_t) n);
    e.setParams (p);
    std::vector<float> bl ((size_t) block), br ((size_t) block);
    for (long i = 0; i < n; i += block)
    {
        const int m = (int) std::min<long> (block, n - i);
        for (int k = 0; k < m; ++k) { bl[(size_t) k] = gl (startIdx + i + k); br[(size_t) k] = gr (startIdx + i + k); }
        HostInfo h = host;
        if (host.ppqValid) h.ppq = startPpq + (double) i * host.bpm / 60.0 / e.sampleRate();
        e.process (bl.data(), br.data(), m, h);
        for (int k = 0; k < m; ++k) { o.L[(size_t) (i + k)] = bl[(size_t) k]; o.R[(size_t) (i + k)] = br[(size_t) k]; }
    }
    return o;
}
static Gen sine (double f, double sr, float a = 0.5f) { return [=] (long i) { return a * (float) std::sin (2.0 * kPi * f * (double) i / sr); }; }
static Gen noise (float a = 0.5f) { auto s = std::make_shared<unsigned> (12345u); return [=] (long) { *s = *s * 1664525u + 1013904223u; return a * ((float) (*s >> 8) / 8388608.f - 1.f); }; }
static Gen zero() { return [] (long) { return 0.f; }; }

static bool finiteBounded (const Out& o, float lim = 16.f)
{
    for (size_t i = 0; i < o.L.size(); ++i)
        if (! std::isfinite (o.L[i]) || ! std::isfinite (o.R[i]) || std::fabs (o.L[i]) > lim || std::fabs (o.R[i]) > lim) return false;
    return true;
}
static double rms (const std::vector<float>& v, size_t a, size_t b)
{ double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i]; return std::sqrt (s / (double) std::max<size_t> (1, b - a)); }
static double goertzel (const std::vector<float>& v, size_t a, size_t b, double f, double sr)
{
    const double w = 2.0 * kPi * f / sr, c = 2.0 * std::cos (w); double s1 = 0, s2 = 0;
    for (size_t i = a; i < b; ++i) { const double s = (double) v[i] + c * s1 - s2; s2 = s1; s1 = s; }
    return (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) (b - a);
}
static GranularEngine* mk (double sr, int block = 512) { auto* e = new GranularEngine(); e->prepare (sr, block); return e; }

// ---------------------------------------------------------------------------------
static void testSilenceImpulse()
{
    section ("silence / impulse / continuous, all modes");
    for (int m = 0; m < NumModes; ++m)
    {
        std::unique_ptr<GranularEngine> e (mk (48000));
        Params p; p.mode = (float) m; p.mix = 1.f;
        auto o = run (*e, p, 48000, zero(), zero());
        float mx = 0; for (float v : o.L) mx = std::max (mx, std::fabs (v));
        CHECK (mx < 1e-9f, "mode %d silence leaked %g", m, (double) mx);

        e.reset (mk (48000));
        auto imp = [] (long i) { return i == 100 ? 1.f : 0.f; };
        o = run (*e, p, 3 * 48000, imp, imp);
        CHECK (finiteBounded (o, 4.f), "mode %d impulse not finite/bounded", m);

        e.reset (mk (48000));
        o = run (*e, p, 3 * 48000, sine (220, 48000), noise());
        CHECK (finiteBounded (o, 4.f), "mode %d continuous not finite/bounded", m);
        CHECK (rms (o.L, 48000, o.L.size()) > 0.01, "mode %d produced no sound (rms %g)", m, rms (o.L, 48000, o.L.size()));
    }
}

static void testMixBehaviour()
{
    section ("dry/wet: 0% bit-exact dry, 100% has no dry");
    std::unique_ptr<GranularEngine> e (mk (48000));
    Params p; p.mix = 0.f; p.output = 0.f;
    auto in = sine (330, 48000);
    auto o = run (*e, p, 48000, in, in);
    bool exact = true; for (long i = 0; i < 48000; ++i) if (o.L[(size_t) i] != in (i)) { exact = false; break; }
    CHECK (exact, "mix=0 not bit-exact");

    e.reset (mk (48000)); p.mix = 1.f;
    auto imp = [] (long i) { return i == 0 ? 1.f : 0.f; };
    o = run (*e, p, 4800, imp, imp);
    CHECK (std::fabs (o.L[0]) < 1e-6f, "dry impulse leaked at mix=1: %g", (double) o.L[0]);
    CHECK (rms (o.L, 0, 4800) > 0.0, "no wet output");

    // level: coherent resynthesis of a sine should stay within +-4 dB of input
    e.reset (mk (48000)); p = Params{}; p.mix = 1.f; p.position = 0.f; p.spray = 0.f; p.density = 20; p.size = 100;
    o = run (*e, p, 3 * 48000, sine (440, 48000), sine (440, 48000));
    const double r = rms (o.L, 48000, o.L.size()) / (0.5 / std::sqrt (2.0));
    CHECK (r > 0.5 && r < 1.6, "resynthesis level ratio %.3f", r);
    std::printf ("  info: wet/dry level ratio for default-ish settings = %.3f\n", r);
}

static void testMono()
{
    section ("mono processing");
    std::unique_ptr<GranularEngine> e (mk (48000));
    Params p; p.mix = 0.f;
    std::vector<float> x (48000); for (size_t i = 0; i < x.size(); ++i) x[i] = 0.5f * (float) std::sin (2 * kPi * 440 * (double) i / 48000);
    auto y = x; e->setParams (p); e->processMono (y.data(), (int) y.size(), {});
    bool same = true; for (size_t i = 0; i < x.size(); ++i) if (x[i] != y[i]) same = false;
    CHECK (same, "mono mix=0 not transparent");
    e.reset (mk (48000)); p.mix = 1.f; p.mode = ModeTexture; y = x; e->setParams (p);
    e->processMono (y.data(), (int) y.size(), {});
    bool ok = true; for (float v : y) if (! std::isfinite (v) || std::fabs (v) > 4.f) ok = false;
    CHECK (ok, "mono wet not finite"); CHECK (rms (y, 20000, 48000) > 0.01, "mono wet silent");
}

static void testFreeze()
{
    section ("freeze");
    const double sr = 48000; std::unique_ptr<GranularEngine> e (mk (sr));
    Params p; p.mix = 1.f; p.size = 200; p.density = 30; p.position = 0.1f; p.spray = 0.05f;
    run (*e, p, (long) sr, sine (440, sr), sine (440, sr));
    p.freeze = 1.f;
    // loud 3 kHz noise-ish tone arrives while frozen: must be ignored
    auto o = run (*e, p, (long) (3 * sr), sine (3000, sr, 0.9f), sine (3000, sr, 0.9f), 256, {}, 0, (long) sr);
    const size_t a = (size_t) sr, b = o.L.size();
    const double e440 = goertzel (o.L, a, b, 440, sr), e3k = goertzel (o.L, a, b, 3000, sr);
    CHECK (rms (o.L, a, b) > 0.05, "frozen audio faded out (rms %g)", rms (o.L, a, b));
    CHECK (e440 > 20.0 * e3k, "frozen buffer polluted by new input (440:%g 3k:%g)", e440, e3k);
    // sanity: unfrozen engine would pass the new tone through
    e.reset (mk (sr)); p.freeze = 0.f;
    run (*e, p, (long) sr, sine (440, sr), sine (440, sr));
    o = run (*e, p, (long) (3 * sr), sine (3000, sr, 0.9f), sine (3000, sr, 0.9f), 256, {}, 0, (long) sr);
    CHECK (goertzel (o.L, a, b, 3000, sr) > goertzel (o.L, a, b, 440, sr), "unfrozen control did not follow new input");
    // without freeze, silence after > 4 s clears the buffer
    e.reset (mk (sr)); p.freeze = 0.f;
    run (*e, p, (long) sr, sine (440, sr), sine (440, sr));
    o = run (*e, p, (long) (6 * sr), zero(), zero());
    CHECK (rms (o.L, (size_t) (5 * sr), o.L.size()) < 1e-4, "unfrozen buffer did not drain");
}

static void testReverse()
{
    section ("reverse playback direction");
    const double sr = 48000; auto ramp = [=] (long i) { return (float) (-0.9 + 1.8 * (double) i / (3.0 * sr)); };
    auto slope = [&] (bool rev, int mode)
    {
        std::unique_ptr<GranularEngine> e (mk (sr));
        Params p; p.mix = 1.f; p.size = 100; p.density = 4; p.spray = 0; p.shape = 1.f; p.position = 0.5f; p.speed = 1.f;
        p.reverse = rev ? 1.f : 0.f; p.mode = (float) mode; p.width = 1.f;
        run (*e, p, (long) (3 * sr), ramp, ramp);
        p.freeze = 1.f;
        auto o = run (*e, p, (long) sr, zero(), zero(), 256, {}, 0, (long) (3 * sr));
        // find isolated grains (runs of non-zero output) and compare the flat middle: late half minus early half
        double score = 0; size_t i = 0; const auto& v = o.L;
        while (i < v.size())
        {
            if (std::fabs (v[i]) < 1e-5f) { ++i; continue; }
            size_t j = i; size_t quiet = 0;
            while (j < v.size() && quiet < 200) { quiet = (std::fabs (v[j]) < 1e-5f) ? quiet + 1 : 0; ++j; }
            const size_t len = j - i;
            if (len > 3000)
            {
                auto mean = [&] (double a, double b) { double s = 0; size_t n = 0; for (size_t k = i + (size_t) (a * (double) len); k < i + (size_t) (b * (double) len); ++k) { s += v[k]; ++n; } return s / (double) std::max<size_t> (1, n); };
                score += mean (0.55, 0.65) - mean (0.35, 0.45);
            }
            i = j;
        }
        return score;
    };
    CHECK (slope (false, ModeNormal) > 0, "forward grain should rise");
    CHECK (slope (true, ModeNormal) < 0, "reverse toggle should fall");
    CHECK (slope (false, ModeReverse) < 0, "REVERSE mode should fall");
}

static void testPitch()
{
    section ("pitch shifting");
    const double sr = 48000;
    for (int semis : { -12, 12 })
    {
        std::unique_ptr<GranularEngine> e (mk (sr));
        Params p; p.mix = 1.f; p.size = 80; p.density = 60; p.position = 0.f; p.spray = 0; p.pitch = (float) semis; p.mode = ModePitch;
        auto o = run (*e, p, (long) (3 * sr), sine (440, sr), sine (440, sr));
        const size_t a = (size_t) sr, b = o.L.size();
        const double target = semis > 0 ? 880 : 220, other = 440;
        CHECK (goertzel (o.L, a, b, target, sr) > 5.0 * goertzel (o.L, a, b, other, sr), "pitch %+d: target %g vs 440 %g", semis,
               goertzel (o.L, a, b, target, sr), goertzel (o.L, a, b, other, sr));
    }
}

static void testDensitySize()
{
    section ("density / size");
    const double sr = 48000;
    for (double d : { 5.0, 20.0, 60.0 })
    {
        std::unique_ptr<GranularEngine> e (mk (sr));
        Params p; p.density = (float) d; p.overlap = 0.5f; p.size = 50; p.mix = 1.f;
        run (*e, p, (long) (10 * sr), sine (200, sr), sine (200, sr));
        const double n = (double) e->grainsSpawned();
        CHECK (std::fabs (n - 10.0 * d) < 0.05 * 10.0 * d + 2, "density %g -> %g grains in 10 s", d, n);
    }
    auto duty = [&] (float sizeMs)
    {
        std::unique_ptr<GranularEngine> e (mk (sr));
        Params p; p.density = 2; p.size = sizeMs; p.mix = 1.f; p.spray = 0; p.position = 0.5f; p.shape = 1.f; p.freeze = 0;
        auto dc = sine (1000, sr, 0.8f);
        run (*e, p, (long) (5 * sr), dc, dc);
        p.freeze = 1.f;
        auto o = run (*e, p, (long) (10 * sr), zero(), zero(), 256, {}, 0, (long) (5 * sr));
        size_t on = 0, tot = 0; for (size_t k = 0; k + 48 <= o.L.size(); k += 48) { ++tot; if (rms (o.L, k, k + 48) > 0.01) ++on; }
        return (double) on / (double) tot;
    };
    const double d200 = duty (200), d50 = duty (50);
    CHECK (d200 / d50 > 3.0 && d200 / d50 < 5.0, "size scaling duty200=%.3f duty50=%.3f", d200, d50);
}

static void testAutomationFuzz()
{
    section ("random parameter automation (every ~7 samples), all rates / blocks");
    unsigned s = 99; auto r = [&] { s = s * 1664525u + 1013904223u; return (float) (s >> 8) / 16777216.f; };
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        std::unique_ptr<GranularEngine> e (mk (sr));
        Out o; const int blocks[] = { 1, 7, 64, 333, 4096 }; bool ok = true;
        for (int bsz : blocks)
        {
            const long n = (long) (sr * 0.8);
            std::vector<float> L ((size_t) bsz), R ((size_t) bsz);
            auto in = noise (0.6f);
            for (long i = 0; i < n; i += bsz)
            {
                Params p;
                for (int k = 0; k < kNumParams; ++k) { const auto& pi = kParams[k]; float v = pi.min + r() * (pi.max - pi.min); p.*(pi.member) = clampToParam (pi, v); }
                p.mix = r(); p.feedback = r();
                HostInfo h; h.bpmValid = true; h.bpm = 60 + 120 * r(); h.playing = r() > 0.3f; h.ppqValid = true; h.ppq = (double) i / sr * 2.0;
                const int m = (int) std::min<long> (bsz, n - i);
                for (int k = 0; k < m; ++k) { L[(size_t) k] = in (i + k); R[(size_t) k] = in (i + k + 7); }
                e->setParams (p); e->process (L.data(), R.data(), m, h);
                for (int k = 0; k < m; ++k) if (! std::isfinite (L[(size_t) k]) || ! std::isfinite (R[(size_t) k]) || std::fabs (L[(size_t) k]) > 16.f || std::fabs (R[(size_t) k]) > 16.f) ok = false;
            }
        }
        CHECK (ok, "NaN/Inf/extreme output at sr=%g", sr);
        // re-prepare mid-stream (sample-rate / buffer-size change)
        e->prepare (sr == 44100.0 ? 96000.0 : 44100.0, 128);
        auto o2 = run (*e, Params{}, 20000, noise(), noise(), 97);
        CHECK (finiteBounded (o2), "after re-prepare");
    }
}

static void testClicks()
{
    section ("click check: freeze toggling + parameter jumps on a sine");
    const double sr = 48000; std::unique_ptr<GranularEngine> e (mk (sr));
    Params p; p.mix = 1.f; p.size = 120; p.density = 30; p.position = 0.f; p.spray = 0.1f;
    auto in = sine (440, sr, 0.5f);
    float maxd = 0, last = 0; long idx = 0;
    for (int seg = 0; seg < 24; ++seg)
    {
        p.freeze = (seg % 2) ? 1.f : 0.f; p.position = (seg % 3) * 0.1f; p.mix = (seg % 5 == 4) ? 0.3f : 1.f;
        auto o = run (*e, p, 12000, in, in, 256, {}, 0, idx); idx += 12000;
        if (seg >= 4) for (float v : o.L) { maxd = std::max (maxd, std::fabs (v - last)); last = v; } else last = o.L.back();
    }
    CHECK (maxd < 0.2f, "max sample step %.4f (natural sine step ~0.03)", (double) maxd);
    std::printf ("  info: max sample-to-sample step = %.4f\n", (double) maxd);
}

static void testHostSync()
{
    section ("host tempo sync / transport");
    const double sr = 48000;
    std::unique_ptr<GranularEngine> e (mk (sr));
    Params p; p.sync = 1.f; p.division = 3.f; p.mix = 1.f; // 1/8
    HostInfo h; h.bpmValid = true; h.bpm = 120; h.playing = true; h.ppqValid = true;
    run (*e, p, (long) (10 * sr), sine (200, sr), sine (200, sr), 512, h, 0.0);
    CHECK (std::fabs ((double) e->grainsSpawned() - 40.0) <= 2.0, "1/8 @120bpm -> %lld grains/10 s (want 40)", (long long) e->grainsSpawned());

    e.reset (mk (sr)); p.division = 0.f; // 1/1 => every 2 s
    run (*e, p, (long) (10 * sr), sine (200, sr), sine (200, sr), 512, h, 0.0);
    CHECK (std::fabs ((double) e->grainsSpawned() - 5.0) <= 1.0, "1/1 -> %lld grains", (long long) e->grainsSpawned());

    // Transport stopped but tempo known -> tempo-derived free-run
    e.reset (mk (sr)); p.division = 3.f; h.playing = false;
    run (*e, p, (long) (10 * sr), sine (200, sr), sine (200, sr), 512, h, 0.0);
    CHECK (std::fabs ((double) e->grainsSpawned() - 40.0) <= 2.0, "stopped transport -> %lld", (long long) e->grainsSpawned());

    // No host info at all -> falls back to density param
    e.reset (mk (sr)); p.density = 10; HostInfo none;
    run (*e, p, (long) (10 * sr), sine (200, sr), sine (200, sr), 512, none, 0.0);
    CHECK (std::fabs ((double) e->grainsSpawned() - 100.0) <= 5.0, "no host -> %lld (want ~100)", (long long) e->grainsSpawned());

    // start/stop toggling doesn't misbehave
    e.reset (mk (sr)); bool ok = true;
    for (int i = 0; i < 40; ++i) { h.playing = (i % 2) == 0; auto o = run (*e, p, 3000, noise(), noise(), 100, h, i * 3.0); ok &= finiteBounded (o); }
    CHECK (ok, "transport toggling");
}

static void testLfo()
{
    section ("LFOs (free + synced)");
    const double sr = 48000;
    for (int shape = 0; shape < NumLfoShapes; ++shape)
    {
        std::unique_ptr<GranularEngine> e (mk (sr));
        Params p; p.lfo1Rate = 2.f; p.lfo1Shape = (float) shape; p.lfo1Dest = DestPosition; p.lfo1Amount = 1.f;
        e->setParams (p); float mn = 9, mx = -9;
        std::vector<float> L (32), R (32);
        for (int i = 0; i < (int) (3 * sr / 32); ++i) { e->process (L.data(), R.data(), 32, {}); const float v = e->viz.lfo1.load(); mn = std::min (mn, v); mx = std::max (mx, v); }
        CHECK (mn < -0.3f && mx > 0.3f, "shape %d range [%g,%g]", shape, (double) mn, (double) mx);
        if (shape != ShapeRandom) CHECK (mn > -1.001f && mx < 1.001f, "shape %d out of range", shape);
    }
    // synced: period of 1/4 @120 bpm = 0.5 s, phase locked to ppq
    std::unique_ptr<GranularEngine> e (mk (sr));
    Params p; p.lfo1Sync = 1.f; p.lfo1Div = 2.f; p.lfo1Dest = DestPitch;
    HostInfo h; h.bpmValid = true; h.bpm = 120; h.playing = true; h.ppqValid = true;
    e->setParams (p); std::vector<float> L (32), R (32); std::vector<float> vals;
    for (int i = 0; i < (int) (2 * sr / 32); ++i) { h.ppq = (double) i * 32 / sr * 2.0; e->process (L.data(), R.data(), 32, h); vals.push_back (e->viz.lfo1.load()); }
    const int per = (int) (0.5 * sr / 32); double maxdiff = 0;
    for (size_t i = 0; i + (size_t) per < vals.size(); ++i) maxdiff = std::max (maxdiff, (double) std::fabs (vals[i] - vals[i + (size_t) per]));
    CHECK (maxdiff < 0.05, "sync LFO periodicity err %g", maxdiff);
}

static void testFeedback()
{
    section ("feedback stability");
    const double sr = 48000; std::unique_ptr<GranularEngine> e (mk (sr));
    Params p; p.feedback = 1.f; p.mix = 1.f; p.density = 100; p.size = 500; p.overlap = 1.f; p.position = 0.05f; p.mode = ModeTexture;
    auto o = run (*e, p, (long) (15 * sr), noise (0.9f), noise (0.9f));
    CHECK (finiteBounded (o, 1.01f), "feedback exceeded soft-limit ceiling");
    float mx = 0; for (float v : o.L) mx = std::max (mx, std::fabs (v));
    std::printf ("  info: peak with feedback=100%% : %.3f (limiter engaged flag=%d)\n", (double) mx, (int) e->viz.limiting.load());
    auto o2 = run (*e, p, (long) (5 * sr), zero(), zero(), 256, {}, 0, (long) (15 * sr));
    CHECK (finiteBounded (o2, 1.01f), "feedback unstable on silence");
    // DC check
    e.reset (mk (sr)); p = Params{}; p.mix = 1.f; p.feedback = 0.8f;
    o = run (*e, p, (long) (8 * sr), sine (100, sr, 0.8f), sine (100, sr, 0.8f));
    double mean = 0; for (size_t i = (size_t) (4 * sr); i < o.L.size(); ++i) mean += o.L[i]; mean /= (double) (o.L.size() - (size_t) (4 * sr));
    CHECK (std::fabs (mean) < 0.02, "DC offset %g", mean);
}

static void testPresets()
{
    section ("presets");
    CHECK (kNumFactoryPresets >= 40, "only %d presets", kNumFactoryPresets);
    int perCat[5] = {};
    for (const auto& f : kFactoryPresets) ++perCat[(int) f.category];
    for (int c = 0; c < 5; ++c) CHECK (perCat[c] == 8, "category %s has %d", kCategoryNames[c], perCat[c]);
    for (int i = 0; i < kNumFactoryPresets; ++i)
    {
        for (int j = i + 1; j < kNumFactoryPresets; ++j)
        {
            CHECK (std::strcmp (kFactoryPresets[i].name, kFactoryPresets[j].name) != 0, "dup name %s", kFactoryPresets[i].name);
            bool diff = false; for (int k = 0; k < kNumParams; ++k) if (kFactoryPresets[i].p.*(kParams[k].member) != kFactoryPresets[j].p.*(kParams[k].member)) diff = true;
            CHECK (diff, "%s == %s", kFactoryPresets[i].name, kFactoryPresets[j].name);
        }
        for (int k = 0; k < kNumParams; ++k) { const float v = kFactoryPresets[i].p.*(kParams[k].member); CHECK (v >= kParams[k].min && v <= kParams[k].max, "%s.%s=%g out of range", kFactoryPresets[i].name, kParams[k].id, (double) v); }
    }
    // each preset renders finite, audible, distinct audio
    std::vector<double> sig;
    for (int i = 0; i < kNumFactoryPresets; ++i)
    {
        std::unique_ptr<GranularEngine> e (mk (48000));
        Params p = kFactoryPresets[i].p;
        HostInfo h; h.bpmValid = true; h.bpm = 110; h.playing = true; h.ppqValid = true;
        // feed 1 s of audio unfrozen first so freeze presets have something to hold
        Params warm = p; warm.freeze = 0.f; auto in = [] (long k) { return 0.4f * (float) std::sin (2 * kPi * 330 * (double) k / 48000) * (((k / 6000) % 2) ? 0.2f : 1.f); };
        run (*e, warm, 48000, in, in, 256, h);
        auto o = run (*e, p, 96000, in, in, 256, h, 0.0, 48000);
        CHECK (finiteBounded (o, 4.f), "%s non-finite", kFactoryPresets[i].name);
        const double r = rms (o.L, 0, o.L.size());
        CHECK (r > 1e-3, "%s silent (rms %g)", kFactoryPresets[i].name, r);
        sig.push_back (r);
    }
    // text round-trip, clamping, factory protection
    PresetData d; d.name = "My Test"; d.category = "User"; d.p = kFactoryPresets[5].p; d.p.size = 333.25f;
    PresetData back; CHECK (parsePreset (serializePreset (d), back), "parse failed");
    bool same = back.name == d.name; for (int k = 0; k < kNumParams; ++k) same &= std::fabs (back.p.*(kParams[k].member) - d.p.*(kParams[k].member)) < 1e-4f;
    CHECK (same, "round trip mismatch");
    PresetData bad; CHECK (! parsePreset ("garbage", bad), "accepted garbage");
    CHECK (parsePreset ("AFTERIMAGE-PRESET 1\nname=X\nsize=99999\nmode=99\npitch=nan\n", bad) && bad.p.size == 1000.f && bad.p.mode == 6.f, "clamp");
    CHECK (isFactoryName ("Ghost Vocal") && ! isFactoryName ("My Test"), "factory protection");
}

static void testNoAllocAndCpu()
{
    section ("real-time safety (allocations) and CPU");
    const double sr = 48000; std::unique_ptr<GranularEngine> e (mk (sr));
    Params p; p.density = 100; p.size = 1000; p.overlap = 1.f; p.mix = 1.f; p.feedback = 0.5f; p.mode = ModeRandom; p.lfo1Dest = DestPitch; p.lfo2Dest = DestSize; p.lfo2Shape = ShapeRandom;
    e->setParams (p);
    std::vector<float> L (512, 0.1f), R (512, 0.1f); HostInfo h; h.bpmValid = true; h.bpm = 130; h.ppqValid = true; h.playing = true;
    for (int i = 0; i < 20; ++i) e->process (L.data(), R.data(), 512, h);
    gAllocs = 0; gCount = true;
    const auto t0 = std::chrono::steady_clock::now();
    const int blocks = (int) (20 * sr / 512);
    for (int i = 0; i < blocks; ++i)
    {
        for (int k = 0; k < 512; ++k) L[(size_t) k] = R[(size_t) k] = 0.3f * (float) std::sin (0.05 * (double) (i * 512 + k));
        p.pitch = (float) (i % 48) - 24; p.freeze = (i % 200) > 150 ? 1.f : 0.f; p.sync = (i % 300) > 150; e->setParams (p); e->process (L.data(), R.data(), 512, h);
    }
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    gCount = false;
    CHECK (gAllocs.load() == 0, "%ld allocations inside process()", gAllocs.load());
    std::printf ("  info: worst-case (96 voices, 100/s x 1 s grains, 4x overlap) = %.2fx realtime, ~%.1f%% of one core @48k\n", 20.0 / secs, 100.0 * secs / 20.0);
}

static void testThreadedViz()
{
    section ("viz data read concurrently from another thread (run under TSan for race check)");
    const double sr = 48000; std::unique_ptr<GranularEngine> e (mk (sr));
    std::atomic<bool> stop { false }; std::atomic<long> reads { 0 };
    std::thread gui ([&] { while (! stop.load()) { float s = 0; for (int i = 0; i < kVizBins; ++i) s += e->viz.peaks[i].load (std::memory_order_relaxed); for (auto& g : e->viz.grains) s += g.env.load (std::memory_order_relaxed); (void) s; ++reads; } });
    Params p; p.mode = ModeTexture; p.mix = 1.f; auto o = run (*e, p, (long) (6 * sr), noise(), noise());
    stop = true; gui.join();
    CHECK (finiteBounded (o), "output"); CHECK (reads.load() > 0, "gui thread ran");
}

int main()
{
    std::printf ("AFTERIMAGE DSP tests\n====================\n");
    testSilenceImpulse(); testMixBehaviour(); testMono(); testFreeze(); testReverse(); testPitch();
    testDensitySize(); testAutomationFuzz(); testClicks(); testHostSync(); testLfo(); testFeedback();
    testPresets(); testNoAllocAndCpu(); testThreadedViz();
    std::printf ("\n%d checks passed, %d failed\n", gPass, gFail);
    return gFail ? 1 : 0;
}
