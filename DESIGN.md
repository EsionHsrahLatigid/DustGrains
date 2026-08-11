# Design

## Source of truth

- Status: Active
- Primary product surfaces: macOS Standalone, VST3 editor, AUv2 editor, Windows Standalone, Windows VST3
- Product identity: `jp.ehl.dustgrains`, `jp.ehl.dustgrains`, AU subtype `DsGr`

## Product goals

- Expose a compact granular instrument with deterministic behavior and stable automation IDs.
- Keep the audio thread free of allocation, locks, file access, and nondeterministic system calls.
- Preserve a fixed grain-pool ceiling so density changes cannot grow realtime resource use.

## Non-goals

- No source dependency on any sibling product project.
- No product-suite targets or sibling plugin identities.
- No DAW installation, signing for distribution, or notarization in the build.

## Interaction model

- MIDI note-on retriggers the monophonic texture and alters deterministic scheduler state.
- MIDI note-off releases the texture when it matches the active note.
- The editor is a direct parameter grid with host-owned sliders and explicit value labels.
- DustGrains adds a built-in momentary Trigger for standalone auditioning; holding Space while the editor has keyboard focus drives the same gate.
- UI trigger state is written to processor-owned atomics as desired-gate state plus monotonic press/release edge counters. The audio thread consumes those edges into its own latch before rendering samples, one edge per sample, so rapid UI pulses are not collapsed by process-block timing. External MIDI continues through the MIDI buffer path.
- If MIDI note-on takes over while the standalone gate is held, the matching MIDI note-off restarts the standalone gate. Processor reset clears DSP state but does not clear a still-held UI gate request.
- Output activity is published by the audio thread as an atomic block peak and polled by the UI timer for a visible meter.

## Visual language

- Near-black field with a sharp DustGrains accent line.
- Compact labels and values; no decorative animation.
- Trigger and output meter are utility controls, not performance animations.
- Warning copy stays terse because the product can generate dense bursts.

## Build surfaces

- Debug preset builds and runs the C++20 engine test only.
- Release preset builds tests plus Standalone and VST3 everywhere.
- Release preset additionally builds AUv2 on Apple platforms.

## CI and Release Contract

- `CI Summary` is the stable required check. A Linux classifier always runs; it skips macOS and Windows only for the documented docs-only allowlist and otherwise chooses the conservative heavy path.
- macOS and Windows each build, test, package, and upload one `latest` ZIP plus a strict single-line `SHA256SUMS.txt`. Actions artifacts expire after 14 days.
- Tag pushes never compile. The Release workflow resolves the tag to its commit, requires the tag and CMake project versions to match, locates the unique successful canonical `CI` push run on `main` with the same `head_sha`, requires exactly the two named platform artifacts, verifies SHA-256 and ZIP integrity, sanitizes the draft asset list, and only then publishes exactly the two versioned release assets.
- Release provenance failures are terminal. Missing, expired, duplicate, or mismatched artifacts must not trigger an automatic rebuild or partial release.
- GitHub actions are pinned to immutable commit SHAs. The release runner requires GitHub CLI 2.x or newer and the minimal `actions: read` / `contents: write` permissions.

## Compatibility notes

- The project is self-contained except for an adjacent `../yup` checkout or the pinned fallback fetch.
- A local macOS workaround overrides YUP's PNG-to-ICNS conversion hook until the upstream helper works reliably on current macOS installs.
