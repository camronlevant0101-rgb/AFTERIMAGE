// AFTERIMAGE - single source of truth for parameter metadata.
#pragma once
#include "Params.h"

namespace afterimage
{
enum class Kind { Float, Bool, Choice };

struct ParamInfo
{
    const char* id;
    const char* name;
    Kind kind;
    float min, max, def;
    float centre;       // skew mid-point for float params, <=0 : linear
    const char* unit;
    float scale;        // display multiplier (100 for percent)
    int decimals;
    const char* choices; // '|' separated for Kind::Choice
    const char* tip;
    float Params::* member;
};

#define AI_DIVS "1/1|1/2|1/4|1/8|1/8 dotted|1/8 triplet|1/16|1/16 triplet|1/32"
#define AI_SHAPES "Sine|Triangle|Square|Random"
#define AI_DESTS "Off|Position|Pitch|Size|Density|Spray|Width"

inline constexpr ParamInfo kParams[] = {
 { "size",     "Size",     Kind::Float, 5.f, 1000.f, 120.f, 80.f, "ms", 1.f, 0, nullptr, "Grain duration, 5 ms - 1000 ms.", &Params::size },
 { "density",  "Density",  Kind::Float, 1.f, 100.f, 20.f, 15.f, "/s", 1.f, 1, nullptr, "New grains per second (1 - 100). Ignored while tempo Sync is active.", &Params::density },
 { "position", "Position", Kind::Float, 0.f, 1.f, 0.25f, 0.f, "%", 100.f, 0, nullptr, "Where grains read from the captured 4 s buffer. 0% = live input, 100% = 4 s ago.", &Params::position },
 { "pitch",    "Pitch",    Kind::Float, -24.f, 24.f, 0.f, 0.f, "st", 1.f, 2, nullptr, "Transpose grains, -24 to +24 semitones.", &Params::pitch },
 { "speed",    "Speed",    Kind::Float, 0.25f, 4.f, 1.f, 1.f, "x", 1.f, 2, nullptr, "Playhead speed through the buffer (pitch independent). 1x holds position.", &Params::speed },
 { "spray",    "Spray",    Kind::Float, 0.f, 1.f, 0.15f, 0.f, "%", 100.f, 0, nullptr, "Random scatter of grain start positions.", &Params::spray },
 { "freeze",   "Freeze",   Kind::Bool,  0.f, 1.f, 0.f, 0.f, "", 1.f, 0, nullptr, "Hold the current buffer. New input is ignored while frozen.", &Params::freeze },
 { "mix",      "Mix",      Kind::Float, 0.f, 1.f, 0.5f, 0.f, "%", 100.f, 0, nullptr, "Dry/wet balance (equal power). 0% = dry only, 100% = processed only.", &Params::mix },
 { "width",    "Width",    Kind::Float, 0.f, 2.f, 1.f, 0.f, "%", 100.f, 0, nullptr, "Stereo width of the processed signal. 0% = mono, 200% = extra wide.", &Params::width },
 { "feedback", "Feedback", Kind::Float, 0.f, 1.f, 0.f, 0.f, "%", 100.f, 0, nullptr, "Feeds the output back into the capture buffer. Soft-limited and filtered for safety.", &Params::feedback },
 { "reverse",  "Reverse",  Kind::Bool,  0.f, 1.f, 0.f, 0.f, "", 1.f, 0, nullptr, "Play grains backwards.", &Params::reverse },
 { "output",   "Output",   Kind::Float, -36.f, 12.f, 0.f, 0.f, "dB", 1.f, 1, nullptr, "Final output level.", &Params::output },
 { "mode",     "Mode",     Kind::Choice, 0.f, 6.f, 0.f, 0.f, "", 1.f, 0, "Normal|Texture|Stutter|Pitch|Reverse|Time|Random", "Processing mode. Each mode changes how grains are triggered, shaped and placed.", &Params::mode },
 { "shape",    "Envelope", Kind::Float, 0.f, 1.f, 0.f, 0.f, "%", 100.f, 0, nullptr, "Grain envelope. 0% = Hann, 100% = near-rectangular with short fades.", &Params::shape },
 { "overlap",  "Overlap",  Kind::Float, 0.f, 1.f, 0.5f, 0.f, "%", 100.f, 0, nullptr, "Grain overlap: scales the trigger rate from 0.25x (sparse) to 4x (dense). 50% = 1x.", &Params::overlap },
 { "sync",     "Sync",     Kind::Bool,  0.f, 1.f, 0.f, 0.f, "", 1.f, 0, nullptr, "Trigger grains on the host tempo grid. Falls back to free-running without a host tempo.", &Params::sync },
 { "division", "Division", Kind::Choice, 0.f, 8.f, 6.f, 0.f, "", 1.f, 0, AI_DIVS, "Grain trigger division when Sync is on.", &Params::division },
 { "lfo1Rate",   "LFO1 Rate",   Kind::Float, 0.05f, 20.f, 0.5f, 1.5f, "Hz", 1.f, 2, nullptr, "LFO 1 rate (free-running).", &Params::lfo1Rate },
 { "lfo1Shape",  "LFO1 Shape",  Kind::Choice, 0.f, 3.f, 0.f, 0.f, "", 1.f, 0, AI_SHAPES, "LFO 1 waveform.", &Params::lfo1Shape },
 { "lfo1Dest",   "LFO1 Dest",   Kind::Choice, 0.f, 6.f, 0.f, 0.f, "", 1.f, 0, AI_DESTS, "LFO 1 modulation target.", &Params::lfo1Dest },
 { "lfo1Amount", "LFO1 Amount", Kind::Float, 0.f, 1.f, 0.5f, 0.f, "%", 100.f, 0, nullptr, "LFO 1 modulation depth.", &Params::lfo1Amount },
 { "lfo1Sync",   "LFO1 Sync",   Kind::Bool,  0.f, 1.f, 0.f, 0.f, "", 1.f, 0, nullptr, "Lock LFO 1 to host tempo.", &Params::lfo1Sync },
 { "lfo1Div",    "LFO1 Div",    Kind::Choice, 0.f, 8.f, 2.f, 0.f, "", 1.f, 0, AI_DIVS, "LFO 1 cycle length when synced.", &Params::lfo1Div },
 { "lfo2Rate",   "LFO2 Rate",   Kind::Float, 0.05f, 20.f, 0.25f, 1.5f, "Hz", 1.f, 2, nullptr, "LFO 2 rate (free-running).", &Params::lfo2Rate },
 { "lfo2Shape",  "LFO2 Shape",  Kind::Choice, 0.f, 3.f, 1.f, 0.f, "", 1.f, 0, AI_SHAPES, "LFO 2 waveform.", &Params::lfo2Shape },
 { "lfo2Dest",   "LFO2 Dest",   Kind::Choice, 0.f, 6.f, 0.f, 0.f, "", 1.f, 0, AI_DESTS, "LFO 2 modulation target.", &Params::lfo2Dest },
 { "lfo2Amount", "LFO2 Amount", Kind::Float, 0.f, 1.f, 0.5f, 0.f, "%", 100.f, 0, nullptr, "LFO 2 modulation depth.", &Params::lfo2Amount },
 { "lfo2Sync",   "LFO2 Sync",   Kind::Bool,  0.f, 1.f, 0.f, 0.f, "", 1.f, 0, nullptr, "Lock LFO 2 to host tempo.", &Params::lfo2Sync },
 { "lfo2Div",    "LFO2 Div",    Kind::Choice, 0.f, 8.f, 2.f, 0.f, "", 1.f, 0, AI_DIVS, "LFO 2 cycle length when synced.", &Params::lfo2Div },
};
inline constexpr int kNumParams = (int) (sizeof (kParams) / sizeof (kParams[0]));

inline int findParam (const char* id)
{
    for (int i = 0; i < kNumParams; ++i)
    {
        const char* a = kParams[i].id; const char* b = id;
        while (*a && *a == *b) { ++a; ++b; }
        if (*a == *b) return i;
    }
    return -1;
}

inline Params defaultParams() { return Params{}; }
} // namespace afterimage
