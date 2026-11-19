// AFTERIMAGE - 40 original factory presets (real parameter values).
#pragma once
#include "Params.h"
#include <cstring>

namespace afterimage
{
enum class Category { Vocal, Melodic, Drums, Atmosphere, Experimental, User };
inline constexpr const char* kCategoryNames[] = { "Vocal", "Melodic", "Drums", "Atmosphere", "Experimental", "User" };

struct FactoryPreset { const char* name; Category category; Params p; };

// Division indices: 0=1/1 1=1/2 2=1/4 3=1/8 4=1/8. 5=1/8T 6=1/16 7=1/16T 8=1/32
inline const FactoryPreset kFactoryPresets[] = {
 // ---------------- VOCAL ----------------
 { "Ghost Vocal",      Category::Vocal, { .size=160, .density=18, .position=0.12f, .spray=0.25f, .mix=0.55f, .width=1.4f, .feedback=0.25f, .mode=ModeTexture, .lfo1Rate=0.3f, .lfo1Dest=DestPosition, .lfo1Amount=0.15f } },
 { "Vocal Bloom",      Category::Vocal, { .size=320, .density=24, .position=0.2f, .pitch=12, .spray=0.3f, .mix=0.5f, .width=1.6f, .feedback=0.35f, .mode=ModeTexture, .lfo1Rate=0.2f, .lfo1Shape=ShapeTriangle, .lfo1Dest=DestSize, .lfo1Amount=0.3f } },
 { "Angel Dust",       Category::Vocal, { .size=90, .density=45, .position=0.1f, .pitch=19, .spray=0.2f, .mix=0.45f, .width=1.8f, .feedback=0.2f, .mode=ModePitch, .lfo1Rate=0.6f, .lfo1Dest=DestPitch, .lfo1Amount=0.06f } },
 { "Vocal Fragments",  Category::Vocal, { .size=70, .density=14, .position=0.3f, .spray=0.5f, .mix=0.7f, .width=1.2f, .mode=ModeRandom, .shape=0.4f } },
 { "Distant Voice",    Category::Vocal, { .size=400, .density=12, .position=0.5f, .pitch=-5, .speed=0.8f, .spray=0.35f, .mix=0.6f, .width=1.3f, .feedback=0.45f, .output=-2 } },
 { "Vocal Freeze",     Category::Vocal, { .size=220, .density=30, .position=0.35f, .spray=0.12f, .freeze=1, .mix=0.8f, .width=1.5f, .mode=ModeTexture, .lfo1Rate=0.15f, .lfo1Dest=DestPosition, .lfo1Amount=0.08f } },
 { "Dream Choir",      Category::Vocal, { .size=450, .density=16, .position=0.25f, .pitch=7, .spray=0.3f, .mix=0.55f, .width=1.9f, .feedback=0.4f, .mode=ModeTexture, .lfo1Rate=0.12f, .lfo1Dest=DestPitch, .lfo1Amount=0.02f, .lfo2Rate=0.09f, .lfo2Dest=DestWidth, .lfo2Amount=0.3f } },
 { "Pitch Ghost",      Category::Vocal, { .size=55, .density=40, .position=0.05f, .pitch=-12, .spray=0.1f, .mix=0.5f, .width=1.1f, .feedback=0.15f, .mode=ModePitch } },
 // ---------------- MELODIC ----------------
 { "Floating Keys",    Category::Melodic, { .size=260, .density=20, .position=0.18f, .pitch=12, .speed=0.5f, .spray=0.1f, .mix=0.45f, .width=1.5f, .feedback=0.3f, .mode=ModeTime, .lfo1Rate=0.25f, .lfo1Dest=DestPosition, .lfo1Amount=0.1f } },
 { "Dream Sequence",   Category::Melodic, { .size=180, .position=0.3f, .pitch=7, .spray=0.1f, .mix=0.55f, .width=1.4f, .feedback=0.35f, .sync=1, .division=4, .lfo1Dest=DestPosition, .lfo1Amount=0.2f, .lfo1Sync=1, .lfo1Div=1 } },
 { "Glass Melody",     Category::Melodic, { .size=40, .density=70, .position=0.15f, .pitch=24, .spray=0.08f, .mix=0.35f, .width=1.7f, .feedback=0.2f, .mode=ModePitch, .shape=0.2f } },
 { "Liquid Notes",     Category::Melodic, { .size=120, .density=30, .position=0.15f, .spray=0.2f, .mix=0.5f, .width=1.3f, .feedback=0.3f, .lfo1Rate=0.8f, .lfo1Dest=DestPosition, .lfo1Amount=0.12f, .lfo2Rate=0.35f, .lfo2Dest=DestDensity, .lfo2Amount=0.2f } },
 { "Celestial",        Category::Melodic, { .size=600, .density=12, .position=0.4f, .pitch=12, .speed=0.7f, .spray=0.4f, .mix=0.6f, .width=2.0f, .feedback=0.5f, .mode=ModeTexture, .lfo1Rate=0.07f, .lfo1Dest=DestPosition, .lfo1Amount=0.2f } },
 { "Melodic Dust",     Category::Melodic, { .size=30, .density=90, .position=0.3f, .spray=0.6f, .mix=0.4f, .width=1.6f, .feedback=0.1f, .output=-2, .mode=ModeRandom } },
 { "Endless Chords",   Category::Melodic, { .size=800, .density=10, .position=0.2f, .spray=0.15f, .mix=0.65f, .width=1.5f, .feedback=0.75f, .mode=ModeTexture } },
 { "Frozen Piano",     Category::Melodic, { .size=350, .density=22, .position=0.3f, .spray=0.1f, .freeze=1, .mix=0.85f, .width=1.4f, .lfo1Rate=0.1f, .lfo1Dest=DestSpray, .lfo1Amount=0.3f } },
 // ---------------- DRUMS ----------------
 { "Glitch Percussion",Category::Drums, { .size=45, .density=16, .position=0.2f, .spray=0.15f, .mix=0.75f, .width=0.9f, .feedback=0.1f, .mode=ModeStutter, .shape=0.7f, .sync=1, .division=6 } },
 { "Drum Fragments",   Category::Drums, { .size=60, .density=25, .position=0.3f, .spray=0.4f, .mix=0.8f, .mode=ModeRandom, .shape=0.5f } },
 { "Broken Rhythm",    Category::Drums, { .size=80, .position=0.35f, .pitch=-3, .spray=0.25f, .mix=0.85f, .feedback=0.2f, .mode=ModeStutter, .shape=0.6f, .sync=1, .division=5, .lfo1Shape=ShapeSquare, .lfo1Dest=DestPosition, .lfo1Amount=0.3f, .lfo1Sync=1, .lfo1Div=1 } },
 { "Stutter Machine",  Category::Drums, { .size=120, .position=0.1f, .spray=0.02f, .mix=1.0f, .width=0.8f, .mode=ModeStutter, .shape=0.8f, .sync=1, .division=7 } },
 { "Digital Drums",    Category::Drums, { .size=25, .density=60, .position=0.15f, .pitch=5, .spray=0.3f, .mix=0.6f, .mode=ModePitch, .shape=0.6f } },
 { "Reverse Percussion",Category::Drums,{ .size=150, .density=12, .position=0.25f, .spray=0.15f, .mix=0.8f, .feedback=0.2f, .mode=ModeReverse, .shape=0.3f } },
 { "Rhythmic Decay",   Category::Drums, { .size=100, .position=0.2f, .spray=0.1f, .mix=0.6f, .feedback=0.85f, .output=-3, .mode=ModeStutter, .shape=0.4f, .sync=1, .division=3 } },
 { "Drum Freeze",      Category::Drums, { .size=90, .position=0.4f, .spray=0.05f, .freeze=1, .mix=0.9f, .mode=ModeStutter, .shape=0.7f, .sync=1, .division=6 } },
 // ---------------- ATMOSPHERE ----------------
 { "Afterglow",        Category::Atmosphere, { .size=700, .density=9, .position=0.3f, .speed=0.9f, .spray=0.35f, .mix=0.5f, .width=1.8f, .feedback=0.55f, .mode=ModeTexture, .lfo1Rate=0.05f, .lfo1Dest=DestPosition, .lfo1Amount=0.15f } },
 { "Deep Space",       Category::Atmosphere, { .size=900, .density=7, .position=0.55f, .pitch=-12, .speed=0.5f, .spray=0.5f, .mix=0.6f, .width=2.0f, .feedback=0.6f, .mode=ModeTexture, .lfo1Rate=0.05f, .lfo1Dest=DestPosition, .lfo1Amount=0.25f } },
 { "Empty Room",       Category::Atmosphere, { .size=500, .density=6, .position=0.7f, .spray=0.6f, .mix=0.3f, .width=1.6f, .feedback=0.35f } },
 { "Ghost Frequency",  Category::Atmosphere, { .size=140, .density=35, .position=0.2f, .pitch=19, .spray=0.3f, .mix=0.4f, .width=1.7f, .feedback=0.45f, .mode=ModePitch, .lfo1Rate=0.18f, .lfo1Shape=ShapeTriangle, .lfo1Dest=DestPitch, .lfo1Amount=0.04f } },
 { "Lost Signal",      Category::Atmosphere, { .size=70, .density=12, .position=0.6f, .pitch=-7, .speed=1.5f, .spray=0.55f, .mix=0.55f, .width=1.2f, .feedback=0.3f, .mode=ModeRandom, .lfo1Rate=0.4f, .lfo1Shape=ShapeRandom, .lfo1Dest=DestSpray, .lfo1Amount=0.3f } },
 { "Endless Atmosphere",Category::Atmosphere,{ .size=1000, .density=8, .position=0.35f, .speed=0.6f, .spray=0.3f, .mix=0.6f, .width=2.0f, .feedback=0.85f, .mode=ModeTexture } },
 { "Dream State",      Category::Atmosphere, { .size=380, .density=14, .position=0.25f, .pitch=5, .speed=0.75f, .spray=0.3f, .mix=0.55f, .width=1.8f, .feedback=0.5f, .mode=ModeTime, .lfo1Rate=0.1f, .lfo1Dest=DestPosition, .lfo1Amount=0.2f, .lfo2Rate=0.13f, .lfo2Dest=DestWidth, .lfo2Amount=0.4f } },
 { "Dark Matter",      Category::Atmosphere, { .size=600, .density=10, .position=0.8f, .pitch=-17, .speed=0.4f, .spray=0.4f, .mix=0.65f, .width=1.4f, .feedback=0.6f, .output=-3, .mode=ModeTexture } },
 // ---------------- EXPERIMENTAL ----------------
 { "Corruption",       Category::Experimental, { .size=20, .density=100, .position=0.5f, .pitch=-3, .speed=2, .spray=0.9f, .mix=0.8f, .width=1.3f, .feedback=0.5f, .mode=ModeRandom, .shape=0.5f } },
 { "Meltdown",         Category::Experimental, { .size=250, .density=25, .position=0.2f, .pitch=-14, .speed=0.25f, .spray=0.45f, .mix=0.75f, .feedback=0.7f, .mode=ModeTime, .lfo1Rate=0.5f, .lfo1Dest=DestPitch, .lfo1Amount=0.25f } },
 { "Random Access",    Category::Experimental, { .size=50, .position=0.5f, .spray=1.0f, .mix=0.85f, .mode=ModeRandom, .sync=1, .division=6 } },
 { "Broken Reality",   Category::Experimental, { .size=300, .density=5, .position=0.45f, .pitch=7, .spray=0.7f, .mix=0.7f, .width=1.6f, .feedback=0.4f, .reverse=1, .mode=ModeRandom, .lfo1Shape=ShapeSquare, .lfo1Dest=DestPosition, .lfo1Amount=0.4f, .lfo1Sync=1, .lfo1Div=2 } },
 { "Time Collapse",    Category::Experimental, { .size=200, .density=20, .position=0.3f, .pitch=-12, .speed=0.25f, .spray=0.2f, .mix=0.8f, .feedback=0.6f, .mode=ModeTime, .lfo1Rate=0.2f, .lfo1Shape=ShapeTriangle, .lfo1Dest=DestSize, .lfo1Amount=0.5f } },
 { "Digital Decay",    Category::Experimental, { .size=12, .density=80, .position=0.2f, .pitch=12, .speed=4, .spray=0.35f, .mix=0.7f, .feedback=0.8f, .shape=0.8f } },
 { "Frequency Shift",  Category::Experimental, { .size=70, .density=50, .position=0.1f, .pitch=5, .spray=0.0f, .mix=0.6f, .mode=ModePitch, .lfo1Rate=0.3f, .lfo1Dest=DestPitch, .lfo1Amount=0.5f, .lfo2Rate=0.17f, .lfo2Shape=ShapeTriangle, .lfo2Dest=DestPitch, .lfo2Amount=0.3f } },
 { "Chaos Theory",     Category::Experimental, { .size=100, .density=45, .position=0.5f, .spray=0.8f, .mix=0.8f, .width=2.0f, .feedback=0.65f, .mode=ModeRandom, .shape=0.3f, .lfo1Rate=3.1f, .lfo1Shape=ShapeRandom, .lfo1Dest=DestDensity, .lfo1Amount=0.5f, .lfo2Rate=0.7f, .lfo2Shape=ShapeRandom, .lfo2Dest=DestSize, .lfo2Amount=0.4f } },
};
inline constexpr int kNumFactoryPresets = (int) (sizeof (kFactoryPresets) / sizeof (kFactoryPresets[0]));

inline bool isFactoryName (const char* name)
{
    for (const auto& f : kFactoryPresets) if (std::strcmp (f.name, name) == 0) return true;
    return false;
}
} // namespace afterimage
