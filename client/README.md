# MMS Monitor

CMake-based C++ project framework for deployment-ready builds.

## Layout

- `include/`: public headers
- `src/`: library and application sources
- `tests/`: test targets
- `cmake/`: package config templates

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Presets (including cross-compile)

```bash
cmake --preset default-debug
cmake --build --preset default-debug
```

Cross-compile for Linux ARM64:

```bash
cmake --preset linux-aarch64-release
cmake --build --preset linux-aarch64-release
```

If your cross environment uses a sysroot, pass it at configure time:

```bash
cmake --preset linux-aarch64-release -DCMAKE_SYSROOT=/path/to/sysroot
```

## Install

```bash
cmake --install build --prefix ./dist
```

Installed package exports `MMSMonitor::mms_monitor` and supports `find_package(MMSMonitor CONFIG REQUIRED)`.

## Package Artifact

```bash
cmake --build build --target package
```

This generates a `.tar.gz` package via CPack.
