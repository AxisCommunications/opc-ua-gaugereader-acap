---
description: "Repository guidance for the OPC UA Gauge Reader ACAP: C++20 application code, manifest contracts, settings UI, packaging, builds, and validation."
applyTo: "**"
---

# OPC UA Gauge Reader ACAP

## Scope and architecture

- This is an AXIS ACAP v4 native application.
- Always write the official platform names as `AXIS OS` and `AXIS ACAP`.
- It acquires an NV12 VDO frame, converts it to BGR with OpenCV, and reads the value from a gauge.
- It publishes the gauge value through OPC UA, Axis events, and an AXIS OS dynamic string.
- Keep existing component responsibilities:
  - [`ImageProvider`](../include/ImageProvider.hpp) handles VDO frames.
  - [`Gauge`](../include/Gauge.hpp) owns gauge image analysis and gauge-value extraction.
  - [`ParamHandler`](../include/ParamHandler.hpp) owns axparameter access and callbacks.
  - [`OpcUaServer`](../include/OpcUaServer.hpp) owns `open62541` and its thread lifecycle.
  - [`EventPusher`](../include/EventPusher.hpp) owns Axis event publication.
  - [`DynamicStringHandler`](../include/DynamicStringHandler.hpp) owns dynamic-string updates through the AXIS OS API.

## Contract changes

When adding or renaming a configurable parameter, update all applicable surfaces together:

- [`manifest.json`](../manifest.json): `paramConfig` name, type/range, and default.
- [`ParamHandler`](../include/ParamHandler.hpp) declarations and [implementation](../src/ParamHandler.cpp).
- [`html/js/opcuagaugereader.js`](../html/js/opcuagaugereader.js): `/axis-cgi/param.cgi` reads/writes with `opcuagaugereader.<Name>`.
- The settings HTML controls and relevant gauge-analysis or publication behavior.

Keep coordinate parameters (`minX`, `minY`, `maxX`, `maxY`, `centerX`, and `centerY`) consistent with the selected VDO stream resolution and the `Gauge` behavior.

Keep `appName` `opcuagaugereader` consistent with the executable, parameter group, and settings UI URLs.

## C++ conventions

- Build standard is C++20 with `-Wall -Wextra -Werror`; retain explicit error handling.
- Follow [`.clang-format`](../.clang-format): LLVM, Allman braces, 4 spaces, 120 columns, no packed parameters.
- New C++ headers/sources use the existing Apache-2.0 Axis header; headers use `#pragma once`.
- Use PascalCase methods, trailing member underscores, GLib types at GLib boundaries, and established Yoda-style null/value comparisons.
- Use `LOG_I` and `LOG_E` from [`include/common.hpp`](../include/common.hpp) for messages.
- Follow the existing `__FILE__/__FUNCTION__` context pattern for failures.
- `LOG_D` is compiled out unless `DEBUG_WRITE` is enabled.
- Frames from `GetLastFrameBlocking()` must always be returned with `ReturnFrame()`.
- Do not introduce unsynchronized access to shared gauge-analysis state.
- Scope third-party GCC diagnostic suppression narrowly with the established push/pop pattern.

## UI and packaging

- The settings UI is static HTML and vanilla JavaScript, with no bundler or framework.
- Preserve tab indentation in [`html/js/`](../html/js/) and the `fetch().then()` style for local flow changes.
- [`Dockerfile`](../Dockerfile) cross-compiles for `aarch64` and `armv7hf` with the AXIS ACAP SDK.
- Keep dependency versions and SHA256 values synchronized; Renovate manages those updates.
- Do not edit generated root artifacts: `*.eap`, `*_LICENSE.txt`, `opcuagaugereader`, `pa*.conf`.
- Regenerate packages through the container build.

## Build and validation

- Build both packages with `make -j "$(nproc)" dockerbuild` or `make -j "$(nproc)" podmanbuild`.
- Use `make aarch64.docker` or `make armv7hf.docker` for a focused Docker build.
- Use `make aarch64.podman` or `make armv7hf.podman` for a focused Podman build.
- There is no automated test suite. For C++ changes, run the relevant container build.
- For formatting, documentation, or configuration changes, run the matching Super-Linter check.
- [`LINT.md`](../LINT.md) documents the full and focused linter commands.
- Validate every altered cross-file contract before finishing.
- Do not modify unrelated generated packages or dependency pins.