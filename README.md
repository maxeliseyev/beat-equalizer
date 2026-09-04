# Beat Equalizer

Multi-mic drum alignment plugin (C++ / JUCE). Static N-in/N-out alignment is
working; current development is the standalone editor for events, per-hit
alignment, crossfades and later WSOLA.

- Onboarding: `docs/onboarding.md`
- Product plan: `docs/drum-editor-plan.md`
- MVP implementation plan: `docs/plan.md`
- Hit detection (no neural net until the ladder is exhausted): `docs/detector-design-recommendations.md`
- Where we left off: `docs/status.md`
- Repo / agent contract: `AGENTS.md`
- Version: `VERSION` (semver) — see `docs/versioning.md`

## Build

Needs CMake 3.22+, Ninja, and a C++20 compiler (Apple Clang / MSVC / GCC).
JUCE 8.0.15 and Catch2 are fetched on first configure.
macOS developer setup is in `docs/onboarding.md`.

```bash
make              # Release: tests + VST3 (+ AU on macOS) + Standalone
make debug        # same, Debug
make test
make vst3
make au           # macOS only
make standalone
make run          # open Standalone
make where        # artefact paths
```

On macOS a plugin build copies VST3/AU into `~/Library/Audio/Plug-Ins/`.
Rescan in Reaper after `make`. Routing: `docs/reaper-testing.md`.

Real-kit tests use `BEAT_REAL_KIT_DIR` and skip when it is unset. Never commit
audio; record only numbers in `docs/real-kit-protocol.md`.

## License

Development builds use JUCE in GPL mode (splash screen on). A commercial JUCE license is required before shipping a closed-source binary.
