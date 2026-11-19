# AFTERIMAGE — granular audio effect (VST3, Windows 10/11 x64)

*Turn any sound into something unheard.*

AFTERIMAGE captures incoming audio into a 4-second circular buffer and resynthesises it with up to 96
overlapping grains (Hermite-interpolated, windowed, click-free). Seven processing modes, two LFOs,
host-tempo sync, freeze, feedback, 40 factory presets and a custom grain-visualising UI.

> **Build status — read this first.** The DSP engine, presets and test-suite were compiled and run
> (g++ 13, Linux) and pass 2,889 checks, also clean under AddressSanitizer + UBSan and ThreadSanitizer.
> The JUCE plugin layer (`Source/`, `GUI/`, `CMakeLists.txt`) **has not been compiled yet**: the
> authoring environment had no JUCE, no CMake, no Windows toolchain and no network. A Windows
> `.vst3` therefore does **not** exist yet — it must be built on a Windows machine or via the included
> GitHub Actions workflow (instructions below). Expect that first compile may surface a few JUCE API
> typos to fix; the GUI code in particular is unverified.

## 1. Getting the `.vst3`

### Option A — GitHub Actions (no local setup)
Push this folder to a GitHub repo. `.github/workflows/build-windows.yml` builds on `windows-latest`,
runs the DSP tests, and uploads `AFTERIMAGE.vst3` (and, if Inno Setup installs, a Windows installer)
as a downloadable artefact.

### Option B — build locally on Windows
Requirements: Windows 10/11 x64, Visual Studio 2022 (workload “Desktop development with C++”), CMake ≥ 3.22, Git.
```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```
CMake downloads JUCE 8.0.4 (which contains the VST3 SDK) automatically — internet required once.

**Output:** `build\AFTERIMAGE_artefacts\Release\VST3\AFTERIMAGE.vst3` (a folder bundle containing
`Contents\x86_64-win\AFTERIMAGE.vst3`). A standalone test app is built at
`build\AFTERIMAGE_artefacts\Release\Standalone\AFTERIMAGE.exe`.

### DSP tests only (any OS with g++)
```bash
./Build/run_native_tests.sh
```

## 2. Install in FL Studio
1. Copy the **whole folder** `AFTERIMAGE.vst3` to `C:\Program Files\Common Files\VST3\` (needs admin), or run the installer (`Installer\AFTERIMAGE.iss` → `Installer\Output\AFTERIMAGE-1.0.0-Setup.exe`).
2. Open FL Studio → **Options ▸ Manage plugins** (Plugin Manager).
3. Make sure `C:\Program Files\Common Files\VST3` is listed under plugin search paths, then click **Find installed plugins** (use *Verify plugins* if asked).
4. Find **AFTERIMAGE** in the list (category *Fx*), tick it and press the star/“add to favourites” if you like.
5. Open a Mixer insert → click an empty effect slot → choose **AFTERIMAGE** (or *Add ▸ More plugins* if it was not auto-listed).
6. Play audio through the insert, open the preset menu and load e.g. *Ghost Vocal* or *Floating Keys*; turn **MIX** up to hear the effect.

Tips: the effect needs audio to have flowed through it before it has anything to granulate. Presets marked
Freeze (*Vocal Freeze, Frozen Piano, Drum Freeze*) hold whatever was last in the buffer — play audio, then load them.

## 3. Uninstall
Delete `C:\Program Files\Common Files\VST3\AFTERIMAGE.vst3` (or use *Apps ▸ AFTERIMAGE ▸ Uninstall*
if you used the installer) and re-scan in FL Studio. User presets live in
`%APPDATA%\AFTERIMAGE\Presets` and are never removed automatically.

## 4. Troubleshooting
| Symptom | Fix |
|---|---|
| Not found after scan | Folder bundle must be copied whole to `...\Common Files\VST3\AFTERIMAGE.vst3\`; confirm the path is in Plugin Manager; click *Find installed plugins*; check the plugin is 64-bit FL Studio. |
| Scan crashes/“failed verification” | Install the Microsoft Visual C++ Redistributable (x64); rebuild in Release; try the Standalone exe to see error output. |
| Silence at 100 % mix | Nothing has played through yet (empty buffer), or Freeze is on with an empty buffer. Turn Freeze off and play audio. Also check Position/Speed aren’t pointing at empty buffer regions (Position 100 % = 4 s ago). |
| Mix 0 % sounds identical to bypass | Intended: 0 % is bit-exact dry. |
| LIMIT LED lit | Safety soft-limiter (above −1 dBFS on the wet path, ±24 dBFS fault guard on output) is acting; reduce Feedback/Density or Output. |
| Sync doesn’t follow tempo | Requires the host to supply tempo (FL does while playing). With no tempo the plugin runs free at the Density setting; with tempo but stopped transport it free-runs at the tempo-derived rate. |
| Crackle at tiny buffer sizes | Raise the FL buffer size; CPU load of worst-case settings ≈ 3.5 % of one core @48 kHz in testing here. |
| Presets missing | Settings ▸ *Rescan presets*. User presets: `%APPDATA%\AFTERIMAGE\Presets\*.aipreset`. |

## 5. Controls
**Primary:** Size 5–1000 ms · Density 1–100 /s · Position (0 % = live, 100 % = 4 s back) · Pitch ±24 st · Speed 0.25–4× (moves the playhead through the buffer independently of pitch; 1× holds position) · Spray · Freeze · Mix (equal-power) · Width 0–200 % (mid/side) · Feedback (soft-limited, low-passed, ≤ 92 % loop gain) · Reverse · Output −36…+12 dB.
**Extra:** Envelope (Hann → flat-top), Overlap (0.25×–4× trigger-rate multiplier), Tempo Sync + Division (1/1 … 1/32 incl. dotted & triplets).
**Modes:**
- NORMAL – coherent resynthesis, light timing jitter.
- TEXTURE – ≥100 ms grains, ≥4× overlap, wide random pan, jittered timing.
- STUTTER – short grains that repeat a latched slice (re-latches every 4 grains); in Sync, grain length is gated to the grid step.
- PITCH – 20–70 ms grains at fixed 4× overlap = granular pitch shifter; Spray becomes detune.
- REVERSE – all grains backwards.
- TIME – 40–250 ms grains, forced overlap; Speed scans the buffer (stretch) while pitch stays put.
- RANDOM – random sizes/positions, random reversal, pitch jumps drawn from a consonant set (−12, −7, −5, 0, +5, +7, +12).
**Modulation:** 2 LFOs (Sine/Triangle/Square/Random), free (0.05–20 Hz) or tempo-locked to ppq; targets Position, Pitch, Size, Density, Spray, Width; Amount = depth.
**Interaction:** double-click = reset, right-click = type exact value, hover = tooltip, resizable (fixed aspect).

## 6. Architecture
```
DSP/      GranularEngine.{h,cpp}  JUCE-free engine (allocates only in prepare())
          Params.h / ParamTable.h single source of truth for every parameter
          Presets.h / PresetIO.h  40 factory presets, text preset format
Source/   PluginProcessor.*       APVTS layout generated from ParamTable, host info, state, preset manager
GUI/      PluginEditor.*, Knob.h, Visualizer.h, LookAndFeel.h, Theme.h
Tests/    test_engine.cpp         native test-suite        Tools/export_presets.cpp  -> Presets/
Presets/  40 .aipreset files (generated)     Installer/  Inno Setup script     Assets/logo.svg
```
Real-time design: parameters are copied from lock-free atomics once per block; control work is done every 32
samples; grain voices are a fixed array of 96; the GUI reads engine state through relaxed atomics only;
FTZ/DAZ is set during processing; grains that would cross the write head (e.g. when Freeze toggles) are
faded out over 3 ms; wet path has a DC blocker and a C1-continuous soft limiter whose activity is shown by the LIMIT LED.
Bypass (`processBlockBypassed`) ramps mix to 0 and output to 0 dB over 10 ms while still capturing audio.
Mono buses are supported (processed as dual-mono, summed back).

## 7. Build report
| Item | Status |
|---|---|
| DSP engine, presets, preset I/O | Implemented; compiled (g++ 13, `-Wall -Wextra` clean); **2,889 checks pass** |
| ASan + UBSan, TSan runs | Clean |
| JUCE processor / editor / CMake / CI / installer script | Written; **not compiled** (no JUCE/CMake/MSVC/network in authoring environment) |
| `AFTERIMAGE.vst3` | **Not produced yet** — build via CI or on Windows (§1). Path once built: `build\AFTERIMAGE_artefacts\Release\VST3\AFTERIMAGE.vst3` |
| Tested in FL Studio | **No** |

Tests cover: silence, impulse, continuous audio, stereo & mono, all 7 modes, freeze (holds buffer, ignores new input, control proves
live input otherwise passes), reverse (grain direction), pitch ±12 (spectral peak), density (grain counts), size (duty cycle),
dry/wet (bit-exact dry at 0 %, no dry at 100 %, level within ±4 dB), randomised automation of every parameter every few
samples at 44.1/48/88.2/96/192 kHz with block sizes 1…4096, re-prepare with new rate/block size, click check (max
sample step 0.038 on a 0.5-amplitude sine through freeze toggles and parameter jumps), host sync grain counts
(tempo, stopped transport, no host), transport toggling, LFO range/periodicity, feedback = 100 % stability (bounded ≤ 1.0), DC offset,
preset validity/uniqueness/audibility (all 40), preset round-trip & factory-name protection, **zero heap allocations inside
`process()`**, and a concurrent GUI-reader thread (TSan). Worst-case CPU (96 voices, 1 s grains, 100/s × 4 overlap):
≈ 28× realtime ≈ 3.5 % of one core @ 48 kHz on the test VM.

## 8. Known limitations
- JUCE layer uncompiled/untested on a host; undo/redo relies on JUCE’s `UndoManager` integration with the parameter tree and is unverified.
- Windows only target; no code signing (Windows SmartScreen may warn on the installer).
- Live (non-frozen) pitch-up / speed-up is limited by the write head: grains are placed so they never overtake it, so at Position 0 with +12 st grains read slightly further back (adds latency of up to roughly one grain length).
- Freeze presets need audio to have passed through the plugin first. Feedback does not write into a frozen buffer.
- Interpolation is 4-point Hermite without oversampling; large up-shifts of bright material can alias.
- Hosts that report no ppq fall back to tempo-derived free-running (not bar-locked).
- JUCE is GPLv3/commercial: distributing binaries requires GPL compliance or a JUCE licence.
- `Mix` uses equal-power crossfade, so 50 % is −3 dB per path by design.
