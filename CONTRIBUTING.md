# Contributing to kindpath-q

kindpath-q is a C++20 audio analysis application — the frequency field scientist for KindPath. It maps acoustic signature, divergence, and resonance in creative works.

## Before You Contribute

Read [CLAUDE.md](./CLAUDE.md) for the detailed agent and development instructions. Read [KINDFIELD.md](https://github.com/S4mu3lD4v1d/KindField/blob/main/KINDFIELD.md) for the epistemological foundation.

## Build Requirements

- CMake 3.21+
- C++20 compiler (AppleClang 14+, GCC 12+, or MSVC 17.4+)
- macOS Monterey (Intel) is the primary target platform
- JUCE framework (included as submodule)

## Building

```bash
git clone --recurse-submodules https://github.com/KindPath-Collective/kindpath-q.git
cd kindpath-q
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --parallel
```

## Code Standards

- C++20 features encouraged (concepts, ranges, coroutines where appropriate)
- JUCE coding conventions for UI components
- Comprehensive error handling — no silent failures in audio processing
- Const correctness throughout
- RAII for resource management

## Testing

```bash
cd build
ctest --output-on-failure
```

All new audio processing code must have unit tests. UI code should have at minimum a smoke test.

## Pull Request Process

1. Ensure the build succeeds on macOS Monterey (Intel)
2. All tests pass (`ctest`)
3. No new compiler warnings
4. Describe: what changed, why, and what audio processing behaviour is affected

## Plugin Development

When adding or modifying plugins (in `plugins/`):
- Document the plugin's audio processing algorithm
- Include parameter ranges and default values
- Plugins must not have side effects outside their designated audio buffers
