# waver-vst 🎸

**VST3 plugin that simulates a double-tracked electric guitar from a single recording.**

Takes a mono guitar signal and outputs a full stereo pair — original take on L, simulated second take on R — saving the time of recording a real second take while preserving a natural, convincing stereo width.

Works in **real-time during playback and mixing**, and also via **Direct Offline Processing** in Cubase (F7). Full PDC (Plugin Delay Compensation) support means Cubase automatically keeps the track in time with the rest of the session.

> Related: [waver](https://github.com/gurrish/waver) — Python CLI version of the same algorithm for batch processing outside the DAW.

---

## How it works

A real double-track sounds natural because the second take has a slightly different:
- **Timing** — the guitarist doesn't play in perfect sync
- **Pitch** — small inconsistencies in fretting and picking angle
- **Tone** — different mic position, different early room reflections

Waver simulates all three independently on the B track:

1. **Variable delay** — base Haas delay + slow IIR-smoothed random timing drift (no sine LFO → no chorus artefact)
2. **Granular pitch shift** — dual-pointer circular buffer with sin² crossfade, tunable in cents
3. **EQ tilt** — optional high-shelf cut on the B track to simulate a different mic distance
4. **IR convolution** *(optional)* — convolve the B track with any room/mic impulse response to simulate a physically different recording chain
5. **Linkwitz-Riley crossover** — the low band is kept mono to prevent phase cancellation on bass frequencies when summed; only the high band is double-tracked

---

## Parameters

### Double-tracking

| Control | Range | Default | Description |
|---|---|---|---|
| **Delay** | 5–40 ms | 22 ms | Base Haas delay of the simulated take |
| **Pitch** | ±25 ct | +8 ct | Detune of the simulated take in cents |
| **Drift** | 0–5 ms | 1.8 ms | Depth of random timing variation (500 ms smoothing) |
| **Level** | −12–+6 dB | −1.5 dB | Level of simulated take relative to original |
| **Crossover** | 60–300 Hz | 150 Hz | Frequency below which low band is kept mono |
| **EQ tilt** | on/off | on | −2.5 dB high-shelf cut on B track @ 4 kHz |
| **Swap L/R** | on/off | off | Swap which take goes to L vs R |

### Impulse Response (optional)

| Control | Description |
|---|---|
| **Load IR** | Load a WAV impulse response file. Applied only to the B track (simulated take). |
| **IR enabled** | Toggle IR convolution on/off without unloading the file |
| **IR mix** | 0–100 % blend between dry and convolved B track |

> **Tip:** Short room IRs (20–80 ms) or mic scatter IRs (< 10 ms) work best. The IR is normalised on load so it won't spike the level on hot recordings. The loaded IR path is saved in the Cubase project and reloads automatically.

---

## Workflow in Cubase

### Option A — Real-time insert (playback & mixing)

1. Record guitar on a **mono** audio track
2. **Duplicate** the track (Edit → Duplicate Track)
3. On the **duplicate track**, insert **Waver** as a stereo insert effect
4. Pan the original track hard L, the processed track hard R
5. Done ✅ — Cubase PDC keeps everything in sync automatically

### Option B — Direct Offline Processing (renders to audio)

1. Record guitar on a **mono** audio track
2. **Duplicate** the track (Edit → Duplicate Track)
3. Select the audio clip on the duplicate → open **Direct Offline Processing** (F7)
4. Add **Waver** → Apply
5. Pan the original track hard L, the processed track hard R
6. Done ✅

> The plugin is mono-in / stereo-out. In either mode Cubase handles the channel widening automatically.

---

## Cubase compatibility notes

- **PDC** — latency is reported to Cubase (`PitchShifter::kBufSize / 2 ≈ 186 ms at 44.1 kHz`); Cubase compensates automatically so the track stays in sync
- **Tail flush** — `getTailLengthSeconds()` is implemented so Cubase flushes the delay buffer at the end of a clip
- **Transport reset** — `reset()` clears all ring buffers on loop restart or transport stop; no artefacts on punch in/out
- **Project state** — all parameters and the IR file path are saved in the Cubase project XML and restored on reload

---

## Distorted and hot recordings

- Works on pre-distorted signals (DI through pedals/amp sim, or recorded from a mic'd cab)
- Output stage applies a **tanh soft clipper** — starts acting around −6 dBFS, no hard clipping, transparent on normal levels, graceful saturation on overshoots
- IR is normalised on load — high-energy room IRs won't cause level spikes
- For best pitch-shift quality on heavily distorted signals, keep **Pitch** ≤ 8 cents

---

## Download

Pre-built Windows x64 VST3 binaries are available on the [Releases page](https://github.com/gurrish/waver-vst/releases).

1. Download `Waver-windows-x64.zip`
2. Extract `Waver.vst3` to `C:\Program Files\Common Files\VST3\`
3. Rescan plugins in Cubase (Studio → VST Plug-in Manager → Rescan)

---

## Building from source

### Prerequisites
- Visual Studio 2022 or later (Community / Professional / Enterprise)
- CMake ≥ 3.22

### Steps

```bash
git clone --recurse-submodules https://github.com/gurrish/waver-vst
cd waver-vst
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The compiled `Waver.vst3` is output to `build/WaverVST_artefacts/Release/VST3/`.

---

## DSP architecture

| Stage | Algorithm |
|---|---|
| Pitch shift | Dual-pointer granular, sin² crossfade, 16384-sample buffer |
| Timing drift | IIR-smoothed LCG random walk, 500 ms time constant |
| Crossover | 4th-order Linkwitz-Riley (sums to flat, no phase artefacts) |
| EQ | Single high-shelf biquad on B track |
| IR convolution | `juce::dsp::Convolution`, normalised, async load |
| Output | tanh soft clipper (drive 1.5×, unity-gain preserving) |

---

## Project structure

```
waver-vst/
├── CMakeLists.txt
├── JUCE/                              ← git submodule
└── Source/
    ├── PluginProcessor.h / .cpp       ← AudioProcessor, APVTS, crossover, convolution, state
    ├── PluginEditor.h  / .cpp         ← GUI (5 knobs + 3 toggles + IR section)
    └── DSP/
        ├── PitchShifter.h / .cpp      ← Granular dual-pointer pitch shift
        └── VariableDelay.h / .cpp     ← IIR-smoothed variable fractional delay
```
