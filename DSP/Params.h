// AFTERIMAGE - parameter model (pure C++, no JUCE dependency).
// Every field is a float in real units so one member-pointer table can drive
// the engine, the presets, the JUCE parameter layout and the tests.
#pragma once

namespace afterimage
{
enum Mode { ModeNormal, ModeTexture, ModeStutter, ModePitch, ModeReverse, ModeTime, ModeRandom, NumModes };
enum LfoShape { ShapeSine, ShapeTriangle, ShapeSquare, ShapeRandom, NumLfoShapes };
enum ModDest { DestNone, DestPosition, DestPitch, DestSize, DestDensity, DestSpray, DestWidth, NumDests };

inline constexpr int kNumDivisions = 9;
// 1/1 1/2 1/4 1/8 1/8. 1/8T 1/16 1/16T 1/32   (length in quarter-note beats)
inline constexpr double kDivisionBeats[kNumDivisions] = { 4.0, 2.0, 1.0, 0.5, 0.75, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };

// Declaration order matters: factory presets use C++20 designated initialisers.
struct Params
{
    float size = 120.f;       // ms
    float density = 20.f;     // grains / s
    float position = 0.25f;   // 0 = now, 1 = 4 s ago
    float pitch = 0.f;        // semitones
    float speed = 1.f;        // scan speed through buffer
    float spray = 0.15f;      // 0..1
    float freeze = 0.f;       // bool
    float mix = 0.5f;         // 0..1
    float width = 1.f;        // 0..2
    float feedback = 0.f;     // 0..1
    float reverse = 0.f;      // bool
    float output = 0.f;       // dB
    float mode = ModeNormal;  // choice
    float shape = 0.f;        // 0 = Hann .. 1 = flat-top
    float overlap = 0.5f;     // 0..1 -> density multiplier 0.25x..4x
    float sync = 0.f;         // bool
    float division = 6.f;     // choice (1/16)
    float lfo1Rate = 0.5f;
    float lfo1Shape = ShapeSine;
    float lfo1Dest = DestNone;
    float lfo1Amount = 0.5f;
    float lfo1Sync = 0.f;
    float lfo1Div = 2.f;
    float lfo2Rate = 0.25f;
    float lfo2Shape = ShapeTriangle;
    float lfo2Dest = DestNone;
    float lfo2Amount = 0.5f;
    float lfo2Sync = 0.f;
    float lfo2Div = 2.f;
};
} // namespace afterimage
