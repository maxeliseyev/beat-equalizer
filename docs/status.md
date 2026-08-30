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

Open PR for the scope. Not Analyze.

## Next

After this PR: **Analyze engine** from fresh `main` (after #4 if still open).

## Resume

1. `git fetch && git checkout feat/scope && git pull`
2. `make test && make vst3`
3. Reaper: two mics, delay one — traces should line up.

## Open

- GitHub Require PR on `main`.
