# Basic and Advanced UI modes

Date: 2026-09-03
Status: accepted

## Context

Standalone editor already shows many controls at once: static delay, polarity,
rotator, Analyze, Detect, glide strength, transport, monitor mix, grid,
correlation, phase, per-hit delay and waveform markers. This is useful for
debugging the algorithms, but it makes the first screen read like an expert
bench rather than a tool a drummer or engineer can trust quickly.

The product still needs those diagnostics: source-centric matching, close-pair
spread, bleed observations and room/OH return are the gate before cutting or
warping. Hiding them forever would make the algorithm impossible to audit.

## Decision

The editor has two conceptual UI modes:

- **Basic** is the default workflow. It shows loading/routing, reference choice,
  one automatic alignment/detect action, transport, A/B, clear status, and only
  the few controls needed to trust or reject the automatic result.
- **Advanced** exposes the bench: per-channel delay, polarity, rotator, phase
  coherence, per-hit/source-centric diagnostics, monitor mix, grid, glide
  strength, and future manual repair tools.

Advanced mode may reuse the same underlying components. Basic mode is not a
different algorithm and does not write a different project format; it is a
visibility layer over the same document, parameters and diagnostics.

New diagnostic UI added before the actual mode switch should be written so it
can be hidden as one advanced block later. Do not make source-centric tables,
matrices or per-hit debug numbers part of the future Basic first screen.

## Consequences

- `processBlock` and DSP state stay unchanged; UI mode is message-thread state.
- Stable APVTS alignment parameters remain stable. A future persisted UI-mode
  parameter may be added as `global.uiMode`, but it must not rename or reorder
  existing channel parameters.
- Basic copy should avoid explaining algorithms on screen. It should present
  outcome and action: aligned/not enough material/review needed.
- Advanced remains the place for engineering numbers: raw TDOA, MAD, residuals,
  full-align offset, room/OH return, bleed and source ownership.
- Current source-centric diagnostics are advanced content even if the explicit
  Basic/Advanced toggle has not landed yet.

## Implementation sketch

Roll the decision out as separate PRs:

1. Foundation: add `global.uiMode`, a `UiMode` enum in the plugin UI layer, a
   compact header toggle, named visible blocks, and block-level tests. Basic is
   the default; Advanced restores the current bench.
2. Channel table: make `ChannelColumns` and `ChannelRow` mode-aware. Basic gets
   fewer columns and more waveform width; Advanced keeps every current column.
3. Basic outcome/status: replace engineering strings in Basic with a short
   user-facing result and next action, while keeping detailed text in Advanced.
4. Advanced diagnostics v2: add the full source-centric table, role-aware
   room/OH return, and manual diagnostic controls.
