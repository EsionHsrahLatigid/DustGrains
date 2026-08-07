# Design

## Source of truth

- Status: Active
- Primary product surfaces: macOS Standalone, VST3 editor, AUv2 editor, Windows Standalone, Windows VST3
- Product identity: `audio.2bit.dustgrains`, `audio.2bit.DustGrains`, AU subtype `DsGr`

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

## Visual language

- Near-black field with a sharp DustGrains accent line.
- Compact labels and values; no decorative animation.
- Warning copy stays terse because the product can generate dense bursts.

## Build surfaces

- Debug preset builds and runs the C++20 engine test only.
- Release preset builds tests plus Standalone and VST3 everywhere.
- Release preset additionally builds AUv2 on Apple platforms.

## Compatibility notes

- The project is self-contained except for an adjacent `../yup` checkout or the pinned fallback fetch.
- A local macOS workaround overrides YUP's PNG-to-ICNS conversion hook until the upstream helper works reliably on current macOS installs.
