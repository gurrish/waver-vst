# waver-vst 🎸

**VST3 plugin version of [waver](https://github.com/gurrish/waver) — double-tracked electric guitar simulation.**

Apply as an insert effect or via **Direct Offline Processing** in Cubase. Takes a mono guitar signal and outputs a stereo pair: original take on L, simulated second take on R.

## Parameters

| Knob | Range | Default | Description |
|---|---|---|---|
| **Delay** | 5–40 ms | 22 ms | Base timing offset of the simulated take (Haas effect) |
| **Pitch** | ±25 ct | +8 ct | Detune of the simulated take in cents |
| **Drift** | 0–5 ms | 1.8 ms | RMS depth of slow random timing variation |
| **Level** | −12–+6 dB | −1.5 dB | Level offset of simulated take vs original |
| **EQ tilt** | on/off | on | Subtle −2.5 dB high-shelf cut on simulated take @ 4 kHz |
| **Swap L/R** | on/off | off | Swap which take goes to L vs R |

## Workflow in Cubase (Direct Offline Processing)

1. Record guitar on a **mono** audio track
2. **Duplicate** the track (Edit → Duplicate Track)
3. Select the audio clip on the duplicate → open **Direct Offline Processing** (F7)
4. Add **Waver** → Apply
5. **Pan** the original track hard L, the processed track hard R
6. Done ✅

The Waver plugin is mono-in / stereo-out, so Cubase will automatically widen the duplicate track to stereo during rendering.

## Building

### Prerequisites
- Visual Studio 2022 or later (Community / Professional / Enterprise)
- CMake ≥ 3.22
- JUCE (added as a git submodule below)

### Steps

```bash
git clone https://github.com/gurrish/waver-vst
cd waver-vst

# Add JUCE as a submodule
git submodule add https://github.com/juce-framework/JUCE

# Configure and build
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The compiled `Waver.vst3` is output to `build/WaverVST_artefacts/Release/VST3/`.

Copy it to `C:\Program Files\Common Files\VST3\` and rescan in Cubase.

## DSP

The pitch shift uses a **dual-pointer granular algorithm** with sin² crossfade — no STFT latency, works equally well in real-time and offline mode. The timing drift uses an **IIR-smoothed random walk** (500 ms time constant), not a sine LFO, to avoid chorus/flanger artefacts.

## Project structure

```
waver-vst/
├── CMakeLists.txt
├── JUCE/               ← git submodule
└── Source/
    ├── PluginProcessor.h / .cpp   ← AudioProcessor, APVTS, routing
    ├── PluginEditor.h  / .cpp     ← GUI (4 knobs + 2 toggles)
    └── DSP/
        ├── PitchShifter.h / .cpp  ← Granular dual-pointer pitch shift
        └── VariableDelay.h / .cpp ← IIR-smoothed variable fractional delay
```
