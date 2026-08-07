# DustGrains

DustGrains is a MIDI-triggered granular fragmentation instrument built with [YUP](https://github.com/kunitoki/yup). It uses a deterministic fixed-size grain pool and a synthetic circular source buffer, so the engine is repeatable and realtime-safe without allocation in the audio path.

The project builds a standalone app, VST3, and AUv2 synth on macOS. On Windows it builds the standalone app and VST3 target.

## Identity

- App ID: `audio.2bit.dustgrains`
- Plugin ID: `audio.2bit.DustGrains`
- AU subtype: `DsGr`
- Manufacturer: `2Bit`
- Product role: synth

## Requirements

- CMake 3.31 or newer
- Ninja
- C++20 compiler
- macOS with Xcode/macOS SDK for AUv2 builds
- A local YUP checkout at `../yup`, or network access for the pinned fallback checkout

YUP is pinned to commit `9a1c9bc699b6a714f6f52486462d98a140c8bf95` when the adjacent checkout is unavailable.

## Build and test

Fast DSP-only loop:

```sh
cmake --preset engine-debug
cmake --build --preset engine-debug
ctest --preset engine-debug
```

Release bundles:

```sh
cmake --preset plugin-release
cmake --build --preset plugin-release --parallel
ctest --preset plugin-release
```

Expected macOS artifacts:

- `build/plugin-release/dustgrains_standalone_plugin.app`
- `build/plugin-release/VST3/Release/dustgrains_vst3_plugin.vst3`
- `build/plugin-release/dustgrains_au_plugin.component`

Windows CI discovers the generated Release standalone executable and VST3 bundle recursively before packaging them.

## Continuous integration and releases

GitHub Actions tests and packages macOS 26 arm64 and Windows 2025 x64 builds. It uploads `DustGrains-latest-macos-arm64.zip` and `DustGrains-latest-windows-x64.zip`; a `v*` tag creates or updates one GitHub Release with both versioned ZIPs.

## Parameters

| Parameter | Function |
| --- | --- |
| Density | Scheduler density in grains per second |
| Duration | Per-grain lifetime |
| Position Jitter | Spread around the note-derived source position |
| Rate | Grain read-rate ratio |
| Rate Scatter | Randomized rate spread |
| Stereo Spread | Pan spread width |
| Output | Final gain before the bounded safety stage |

## Verification covered

The DSP regression test checks silence before trigger, deterministic rendering for the same seed, density-controlled event counts, release-to-silence behavior, finite bounded output under extreme/non-finite parameters, velocity-zero behavior, fixed voice-pool limits, and block/sample render equivalence.
