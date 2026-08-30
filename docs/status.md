# Status

Updated: 2026-08-30
Stage: 1 (static alignment)
Plan step: feat — oscilloscope strips (sMexoscope-style)
Branch: `feat/scope`
PR: https://github.com/maxeliseyev/beat-equalizer/pull/5
Blockers: none

## Done

- PR 1–3 in `main`. GitHub #4 (Makefile + `docs/`) may still be open.
- Per-channel output oscilloscope: min/max per pixel, rising trigger on Reference,
  delay/invert visible on the trace.

## Now

Scope is wired: `processBlock` → `ScopeRing` → editor timer → cyan strips.
Proved by `make test-gui` (PNG `build/release/scope-gui-test.png`).
Reaper still showing peak meters = old module in memory, not missing GUI code.

## Next

After this PR: **Analyze engine** from fresh `main` (after #4 if still open).

## Resume

1. `git fetch && git checkout feat/scope && git pull`
2. `make test && make test-gui && make vst3`
3. Fully quit Reaper (Cmd+Q), re-insert. Title must say `| scope`. Play the track.

## Open

- GitHub Require PR on `main`.
