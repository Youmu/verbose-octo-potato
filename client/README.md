# MMS Monitor

CMake-based C++ project framework for deployment-ready builds.

## Dependencies

`PotatoClient` uses OpenSSL (AES-256-CBC + Base64), libcurl (HTTP POST), and nlohmann/json (JSON body).

Install on Debian/Raspberry Pi OS:

```bash
sudo apt-get update
sudo apt-get install -y libssl-dev libcurl4-openssl-dev nlohmann-json3-dev pkg-config
```

## Layout

- `include/`: public headers
- `src/`: library and application sources
- `tests/`: test targets
- `cmake/`: package config templates

## Build

```bash
cd ..
cmake -S client -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j
ctest --test-dir build/release --output-on-failure
```

## Runtime Configuration

The client app reads Potato server settings from environment variables:

- `POTATO_EP`: Potato server endpoint URL
- `POTATO_KEY`: AES-256 key in base64 format (must decode to 32 bytes)

Example:

```bash
export POTATO_EP="http://127.0.0.1:8000/potato/msg"
export POTATO_KEY="your_base64_key_here"
../build/release/mms_monitor_app
```

## Install As Service

Use the install script in project root to register the app as a `systemd` service named `potato_client`:

```bash
cd ..
./install_potato_client_service.sh
```

The script installs:

- binary: `/usr/local/bin/potato_client`
- env file: `/etc/default/potato_client`
- unit file: `/etc/systemd/system/potato_client.service`

After updating environment values, restart the service:

```bash
sudo systemctl restart potato_client
sudo journalctl -u potato_client -f
```

## Presets (including cross-compile)

```bash
cmake -S client -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j
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
