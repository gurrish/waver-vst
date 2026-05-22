# waver-vst 🎸

**VST3 plugin that simulates a double-tracked electric guitar from a single recording.**

Takes a mono guitar signal and outputs a processed mono signal — the plugin never mixes the original (dry) audio into its output.

Works in **real-time during playback and mixing**, and also via **Direct Offline Processing** in Cubase (F7). Full PDC (Plugin Delay Compensation) support means Cubase automatically keeps the track in time with the rest of the session.

> Related: [waver](https://github.com/gurrish/waver) — Python CLI version of the same algorithm for batch processing outside the DAW.

---

## How it works

A real double-track sounds natural because the second take has a slightly different:
- **Timing** — the guitarist doesn't play in perfect sync
- **Pitch** — small inconsistencies in fretting and picking angle
- **Tone** — different mic position, different early room reflections

Waver processes the input into a dedicated processed (wet) signal. It never mixes the original dry audio into the output. The wet path applies the following stages (mono):

1. **Granular pitch shift** — dual-pointer circular buffer with sin² crossfade, tunable in cents
2. **Variable delay** — Haas-style base delay plus slow IIR-smoothed random timing drift (no sine LFO → no chorus artefact)
3. **EQ tilt** — optional high-shelf cut to simulate a different mic/position
4. **Internal short IR (FIR)** — a small internal impulse applied to the wet path (polarity is flipped before this internal IR)
5. **IR convolution (optional)** — convolve the wet signal with any external room/mic impulse response

The processor always outputs the wet/processed signal (mono). To create a stereo image in the host, duplicate the track and pan the original and processed tracks left/right.

---

## Parameters

### Double-tracking

| Control | Range | Default | Description |
|---|---|---|---|
| **Delay** | 5–40 ms | 22 ms | Base Haas delay of the simulated take |
| **Pitch** | ±25 ct | 2.0 ct | Detune of the simulated take in cents |
| **Drift** | 0–5 ms | 0.00 ms | Depth of random timing variation (500 ms smoothing) |
| **Modulation** | 0–1 | 0.00 | Modulation scale for delay drift |
| **Level** | −12–+6 dB | 0.0 dB | Level of simulated take relative to original |
| **EQ tilt** | on/off | on | −2.5 dB high-shelf cut on wet signal @ 4 kHz |


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

> The plugin is mono-in / mono-out. It outputs only processed (wet) audio and never mixes the original dry signal. To create a stereo image, duplicate the track and pan original/processed left and right in your DAW.

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
| Timing drift | IIR-smoothed LCG random walk, 500 ms time constant (cubic interpolation in delay reads) |
| Internal short IR | Small FIR-style impulse applied to wet path; polarity is flipped before this stage |
| IR convolution | `juce::dsp::Convolution`, normalised, async load (optional external IR)
| EQ | Single high-shelf biquad on wet signal |
| Output | tanh soft clipper (drive 1.5×, unity-gain preserving); plugin outputs processed mono only |

---

## Project structure

```
waver-vst/
├── CMakeLists.txt
├── JUCE/                              ← git submodule
└── Source/
    ├── PluginProcessor.h / .cpp       ← AudioProcessor, APVTS, convolution, state
    ├── PluginEditor.h  / .cpp         ← GUI (4 knobs + 2 toggles + IR section)
    └── DSP/
        ├── PitchShifter.h / .cpp      ← Granular dual-pointer pitch shift
        └── VariableDelay.h / .cpp     ← IIR-smoothed variable fractional delay (cubic interpolation)
```

---

## License

This project is licensed under the **GNU General Public License v3.0** — see [LICENSE](LICENSE) for details.

This is required because the project is built on [JUCE](https://juce.com), which is GPL v3 for open source use. If you want to use this code in a proprietary (closed-source) product, you must obtain a commercial JUCE license from Raw Material Software Limited.

