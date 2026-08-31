# Beat Equalizer

Multi-mic drum alignment plugin (C++ / JUCE). MVP is **static** alignment: one delay and polarity per channel, no ARA, no slicing.

- Product plan: `docs/drum-editor-plan.md`
- MVP implementation plan: `docs/plan.md`
- Where we left off: `docs/status.md`
- Repo / agent contract: `AGENTS.md`

## Build

Needs CMake 3.22+, Ninja, and a C++20 compiler (Apple Clang / MSVC / GCC).
JUCE 8.0.15 and Catch2 are fetched on first configure.

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

## License

Development builds use JUCE in GPL mode (splash screen on). A commercial JUCE license is required before shipping a closed-source binary.
