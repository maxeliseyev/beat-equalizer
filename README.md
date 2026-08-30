# Beat Equalizer

Multi-mic drum alignment plugin (C++ / JUCE). MVP is **static** alignment: one delay and polarity per channel, no ARA, no slicing.

- Product plan: `DOCUMENTATION/drum-editor-plan.md`
- MVP implementation plan: `DOCUMENTATION/plan.md`
- Where we left off: `DOCUMENTATION/STATUS.md`
- Repo / agent contract: `AGENTS.md`

## Build (macOS)

Needs CMake 3.22+, Ninja, and Apple Clang. JUCE 8.0.15 and Catch2 are fetched on first configure.

```bash
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
```

Release:

```bash
cmake --preset release
cmake --build --preset release
```

Targets: VST3, AU, Standalone. After a successful plugin build JUCE copies VST3/AU into `~/Library/Audio/Plug-Ins/`.

## License

Development builds use JUCE in GPL mode (splash screen on). A commercial JUCE license is required before shipping a closed-source binary.
