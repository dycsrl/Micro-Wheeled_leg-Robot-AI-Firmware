[English](README.md) | [简体中文](README.zh-CN.md)

# MuShiBot ESP32 Firmware

Firmware source for a micro wheeled-leg robot: native standing control, an on-device neural Actor, a browser control panel, and telemetry. This README is an engineering entry point for people and coding agents: start with the facts and reading order below, then follow the source paths before changing behavior.

**Hardware deployment is prohibited for the bundled candidate model:** `hardware_deployment_prohibited: true` remains in [model/active-model.json](model/active-model.json). Compilation, offline tests, and identity checks are not hardware safety or stability approval. Do not flash build-only binaries. Startup `initFOC()` can move the wheels for alignment even when the application starts stopped.

## 1. Quick facts and scope

| Item | Contract |
|---|---|
| Target | ESP32, Arduino, PlatformIO environment `esp32dev`; this board setting does not identify the physical module or flash capacity |
| Build versions | PlatformIO Core 6.1.18; espressif32 6.10.0; Arduino package 3.20017.241212+sha.dcc1105b; Xtensa 8.4.0+2021r2-patch5 |
| Native controller | Zero-joystick standing with PID/filter state; not a runtime joystick, jump, or arbitrary-height interface |
| AI controller | Local u2594 Actor, `63 → 64 → 64 → 4`, tanh; no cloud service, LLM, or firmware Critic |
| Inputs and outputs | 21 values × 3 history frames; outputs: left leg position, right leg position, left wheel voltage, right wheel voltage |
| Networking | HTTP 80 for page/status/control; TCP 9871 for a single telemetry client; trusted LAN only |
| Included | Firmware, generated Actor/contract, patched vendored libraries, build tools, offline tests, and component licenses |
| Not included | Real network credentials, device NVS, training checkpoints, compilers, binaries, caches, and old Git history |

## 2. Read in this order

1. This README: scope, safety boundaries, build modes, and runtime semantics.
2. [SETUP.md](SETUP.md) and [docs/CONFIGURATION.md](docs/CONFIGURATION.md): setup context, pin assignments, units, limits, network configuration, and timing.
3. [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), then [src/app/main.cpp](src/app/main.cpp): state ownership and the actual step 1–9 execution order.
4. The module you intend to change, using the map below. Trace callers and the final hardware submission first.
5. [model/active-model.json](model/active-model.json) and [deployment/direct_contract.py](deployment/direct_contract.py) before touching any AI adapter or action semantics.
6. [THIRD_PARTY.md](THIRD_PARTY.md), [dependencies.lock.json](dependencies.lock.json), and [lib/PATCHES.md](lib/PATCHES.md) before touching dependencies. [docs/INDEX.md](docs/INDEX.md) indexes the existing documents, which are currently Chinese.

## 3. Engineering map and data flow

| Path | Responsibility and boundary |
|---|---|
| [src/app/main.cpp](src/app/main.cpp) | Startup, mode state, and sole control-loop execution order; the control core owns controller state |
| [src/control/native/](src/control/native/) | Native standing PID/filter state; resume re-anchors time/distance while retaining state, not a claim of bumpless transfer |
| [src/control/ai/](src/control/ai/) | Coordinate adaptation, three-frame history, inference, and action progression; `FourCore.h` has no hardware access |
| [src/control/Safety.h](src/control/Safety.h) | Shared validation, hard-fault conditions, and output limits |
| [src/hardware/](src/hardware/) | Sole owners of IMU, battery, wheel motors, and servos; `sample()` reads, `apply()` submits |
| [src/telemetry/Bridge.h](src/telemetry/Bridge.h) | Fixed-capacity cross-core snapshots, commands, status, and events; control never waits for networking |
| [src/network/Network.cpp](src/network/Network.cpp) / [Page.h](src/network/Page.h) | Core 0 owns Wi-Fi/NVS/HTTP/TCP; the page has independent heartbeat and status polling |
| [protocol/fields.def](protocol/fields.def) / [Frame.h](protocol/Frame.h) | Shared C++/Python wire layout: 516 bytes per frame |
| [model/](model/) / [deployment/](deployment/) | Generated parameters, model identity, and deployment contract checks; checkpoints are not compilation dependencies |
| [tools/](tools/) / [tests/](tests/) | Build, identity checks, offline decoding, and hardware-free regressions |
| [lib/](lib/) / [licenses/](licenses/) | Vendored patched libraries and retained licenses; do not replace with unpatched upstream versions |

Control flow: hardware sample → finite-input/stop checks → native or AI controller → candidate-output protection and limiting → the unique `hardware::apply()` submission. Preserve the exact order in `main.cpp`; do not write motors from other modules. In AI, observation → previous-action progression → inference completion → new-action acceptance is ordered and must not be rearranged.

Network flow: browser commands → bounded bridge → control core; frozen control snapshots → bounded bridge → HTTP status/TCP logs. Networking consumes copies, never runs control calculations, and cannot mutate frozen frames.

## 4. Quick start: compilation only

Clone using the standard HTTPS URL, then run commands from the project root:

```sh
git clone https://github.com/dycsrl/Micro-Wheeled_leg-Robot-AI-Firmware.git
cd Micro-Wheeled_leg-Robot-AI-Firmware
python -m pip install -r requirements-build.txt
python tools/build.py --build-only
```

Python 3 and PlatformIO Core are required; the pinned Core version is in [requirements-build.txt](requirements-build.txt). The first build downloads the pinned platform/framework/toolchain. Use `python tools/build.py --pio "<PlatformIO executable>" --build-only` for an existing installation. The wrapper locates the project relative to itself, so it can also be invoked by absolute script path from another directory.

`--build-only` generates **synthetic network configuration** without reading or copying local passwords. The build hook rejects `upload`, `uploadfs`, and `program` in this mode. The wrapper itself only builds. Default log: `build/build.log`; output: `.pio/build/esp32dev/`. `--log <path>` changes the log destination. Generated output is not source and must not be committed or flashed.

**Source-check caveat:** `python tools/check_source.py` checks the Actor contract, vendored hashes, and Python syntax, but currently also asserts that the project root has no `.git`. Run it only in an exported source directory without Git metadata (for example, an extracted GitHub source archive). It fails in a normal clone even when those checks pass; do not delete `.git` or bypass the assertion to hide this limitation. No code fix is included in this README-only change.

## 5. Ordinary local build and secrets

1. Copy [include/secrets.example.h](include/secrets.example.h) to `include/secrets.h` on your own machine.
2. Fill `STA_SSID`, `STA_PASS`, `AP_SSID`, and `AP_PSW`, then remove the template `#error`. An empty default STA SSID disables default STA startup. Choose open/encrypted AP configuration according to ESP32 Wi-Fi rules and your risk assessment; the source adds no password-length assertion.
3. Run `python tools/build.py`. Missing local secrets fail explicitly; ordinary builds do not silently fall back to synthetic configuration.
4. Keep `include/secrets.h`, device NVS, logs, and build output out of Git. Ordinary binaries can embed credentials; never send passwords in chat.

Existing `wifi_cfg` NVS can override compiled STA defaults. This source copy does not read or migrate a device's NVS during setup. The current page has no network-settings save endpoint; do not invent one. See [configuration](docs/CONFIGURATION.md) for precedence and static-address fallback.

Manual upload is **not** a quick-start step. Only after separate model, wiring, supply-voltage, direction, and mechanical-protection review may an operator use a self-configured ordinary build environment:

```sh
pio run -e esp32dev -t upload --upload-port <port>
pio device monitor --port <port> --baud 115200
```

Do not upload synthetic configurations. Changing a JSON prohibition flag is not deployment approval. No flashing, serial connection, or robot operation is performed by this documentation update.

## 6. Runtime modes and safety boundaries

Connect through your locally configured AP or STA network; both operate in AP+STA mode. Read the assigned address from serial startup diagnostics rather than assuming a fixed IP, then open it over HTTP port 80. There is no HTTPS, user management, or public-network hardening; the control lease is not TLS authentication. Do not expose the service to the Internet.

- Startup application mode is stopped with servo-power intent off; FOC initialization can still move wheels.
- Claim a control session before commanding a mode. Readable status does not prove ownership. HTTP 202 means queued, not completed: inspect fresh actual mode/status.
- Native standing is a cold entry. AI takeover requires at least 500ms in native, `abs(pitch-nativePitchZero) < 20deg`, valid input, and successful seeding; otherwise `NotStanding` is reported.
- Return-to-native preserves native state and re-anchors it. Stop/retract stops wheels but retains leg-power intent. Servo-disable stops wheels and releases leg power.
- **Closing the page does not guarantee stopping.** A 1500ms heartbeat lease expiry returns running AI to native when hard checks pass; already-native control can remain native. Hard faults and manual stops are not automatically cleared by disconnection.
- Hard checks cover finite values, `accZ > 1.8g`, valid battery, `abs(pitch) > 25deg` or `abs(roll) > 35deg`, and `elapsed > 100000us`. Hard faults stop; a control deadline/Actor failure or lost heartbeat falls back to native only in a running state with hard checks passed.

HTTP routes: `GET /`, `/status`, `/diagnostics`; `POST /claim`, `/heartbeat`, `/command` (token and code). Codes: `0` stop, `1` native, `2` AI, `3` return native, `4` servo-disable. Commands expire after 500ms; stop latch has priority. HTTP 403 means unauthorized; 409 means an active owner/conflict. Reclaim deliberately rather than automatically fighting another operator. Use independent mechanical protection.

## 7. AI contract and change constraints

- u2594 uses 63 Actor inputs (21 × 3 frames), not the training-side 65-dimensional Critic observation. Input order/scales live in `actor_frame_names` / `actor_scales` in [the model contract](model/active-model.json); `critic_privileged` is empty, not evidence of extra firmware channels.
- Clip each action to ±1. Output order is left leg, right leg, left wheel, right wheel. Leg desired angle is `clip(-.3297 + .12*a, -.5, -.12)` **rad**. Wheel command is `7.4*a` **V**, held until the next accepted action, with no wheel LPF/P loop. Hardware output protection still applies.
- Each 1ms leg step limits target change to `.003rad`; estimate moves by `.025*(desired-estimate)`. This is a target-history estimate, **not measured servo feedback**. The adapter converts angles/rates to rad/rad/s and negates both wheels' angle, speed, and voltage; it does not subtract dynamic native pitch zero or swap wheels.
- Contract control step: **10ms**; simulation step: **1ms**; training action delay: **7ms**. These are not firmware sleeps. Firmware inference is synchronous: do not add a synthetic 7ms delay. Progression uses actual microsecond deltas; gaps over 100ms fail. The AI loop does not promise a forced 10ms wall-clock period. Non-AI `vTaskDelay(1)` plus a minimum 1500us wait is not a fixed 1ms control loop.
- Keep `model/Actor.h` byte-for-byte consistent with `actor_sha256`. Do not hand-format generated parameters, independently swap weights, reorder observations, alter units, or remove identity/protection checks. Model replacement requires matching export identity, contract, axes, history, timing, and offline forward equivalence, followed by separate hardware review.
- The checkpoint path in metadata is a source identifier, not a bundled runtime dependency. Identity checks do not verify the complete frozen training provenance. Hardware axes, zero offsets, and loaded servo behavior remain uncalibrated; retain `hardware_deployment_prohibited: true`.

## 8. Telemetry and offline regression

TCP 9871 sends logs only: one JSON identity line followed by 516-byte frames, single client, no authentication/encryption. Save the complete raw byte stream at the receiver. The 8-frame queue drops on overflow rather than waiting for networking; drops and sequence gaps mean logging is not lossless. A client with no send progress for 1500ms is disconnected. HTTP status is not the full log.

```sh
python tools/parse.py capture.raw --output frames.jsonl
```

The output path must not already exist. The summary reports identity, trailing partial frames, sequence gaps, and native/AI statistics. Distinguish commanded voltage, estimated leg angles, and measured sensors; they are not interchangeable.

Host tests require Python 3 (standard library), a C++11 compiler available as `c++`, and Node.js. The scripts invoke `c++` directly; an installed Windows firmware toolchain alone does not satisfy this host-test prerequisite.

```sh
python tests/run_host.py
python tests/review_main.py
python tests/test_direct_timing.py
python tests/test_direct_contract.py
python tests/test_protocol.py
python tests/test_telemetry_header.py
node tests/review_page.js
```

These cover the C++ control/Actor forward path, C++/Python wire decoding, hardware-stubbed main loop, inference boundaries, and browser sandbox. They do not access hardware.

## 9. Verification status and troubleshooting

Prior project verification distinguishes **Windows firmware compilation** from **macOS offline regressions**; it does not claim Windows host C++ tests or hardware deployment passed. This README-only update does not rerun firmware compilation or change code, model bytes, safety logic, or dependencies. Hardware stability/safety, loaded calibration, flashing, and end-to-end robot operation remain unverified here.

| Symptom | Check first |
|---|---|
| Missing secrets | Configure locally for ordinary builds, or use `--build-only` only for compilation |
| Source check fails on `.git` | Use an exported source directory as described in section 4; preserve repository metadata |
| Vendor/Actor fingerprint mismatch | Restore locked bytes; inspect `dependencies.lock.json` and the model contract, never remove checks |
| Stale status / unknown mode | Check network and independent heartbeat; never assume the robot stopped |
| AI entry refused | Inspect actual native duration, posture, and reason; button acceptance is not model validation |
| Startup failure | Read staged `[BOOT]` diagnostics; `/diagnostics` retains only 32 events and the first hard fault, not unlimited history |
| TCP gaps/disconnect | Inspect drops, sequence numbers, slow-client timeout, and trailing partial frames |

## 10. Origins and licensing

- Original robot hardware/control: [MuShibo/Micro-Wheeled_leg-Robot](https://github.com/MuShibo/Micro-Wheeled_leg-Robot), contributors **穆世博 (Mu Shibo / MuShibo)** and **李育锋 (Li Yufeng)**. Prior source review found no independent LICENSE; public readability does not imply an MIT license for the whole project. Retain original notices and terms.
- Deployment-interface reference: [星际浪子 / mujoco_esp32_robot_deploy_public](https://gitee.com/starimpact_admin/mujoco_esp32_robot_deploy_public), MIT, Copyright (c) 2024 星际浪子; retained text in [licenses/](licenses/).
- Vendored Simple FOC 2.2.1, MPU6050_tockn 1.5.2, and SCServo/SCSservo 1.0.1 retain their source notices, patches, and applicable licenses. SCServo includes a GPL-3.0 reference license; do not relabel it MIT.

Read [THIRD_PARTY.md](THIRD_PARTY.md) for exact provenance, component-license facts, and redistribution obligations, and [lib/PATCHES.md](lib/PATCHES.md) for local patches. There is **no blanket MIT relicensing** and no newly imposed project-wide reauthorization requirement. Training/mechanical assets not included here are not licensed by this README.
