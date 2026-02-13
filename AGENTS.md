# Repository Guidelines

## Project Structure & Module Organization
This is a PlatformIO + Arduino ESP32 project.

- `src/`: runtime implementation (`main.cpp`, web portal, WiFi, auth, WOL, power orchestration, Bemfa MQTT).
- `include/`: public headers for each module (`*Service.h`, `ConfigStore.h`, `WebPortal.h`).
- `test/`: Unity-based test suites, one folder per module (for example `test/test_wifi_service/test_main.cpp`).
- `platformio.ini`: environments, board, framework, dependencies, upload/monitor ports.
- `README.md`, `TESTING.md`, `需求文档.md`: product and testing docs.
- `页面1.png`, `页面2.png`: UI reference assets.

## Build, Test, and Development Commands
Run from repository root:

- `platformio run -e esp32dev`: build firmware for ESP32 Dev Module.
- `platformio run -e esp32dev -t upload`: flash firmware to the configured serial port (currently `COM5`).
- `platformio device monitor -b 115200`: open serial monitor.
- `platformio test -e esp32dev_test --without-uploading --without-testing`: compile all tests only.
- `platformio test -e esp32dev_test`: run tests on connected hardware.

If `platformio` is not in PATH, use `C:\Users\25547\.platformio\penv\Scripts\platformio.exe`.

## Coding Style & Naming Conventions
- Language: C++ (Arduino framework).
- Indentation: 2 spaces; braces and spacing should match existing files.
- Types/classes: `PascalCase` (for example `PowerOnService`).
- Methods/variables: `camelCase`.
- Constants: `k` prefix + `PascalCase` (for example `kReconnectIntervalMs`).
- Keep modules decoupled: business logic in services, orchestration in `main.cpp`, HTTP/UI logic in `WebPortal`.

## Testing Guidelines
- Framework: Unity (PlatformIO test runner).
- Add/extend tests in `test/test_<module>/test_main.cpp`.
- Test names should describe behavior, prefixed with `test_`.
- For feature changes, add at least one positive-path and one failure/edge-case test.

## Commit & Pull Request Guidelines
Current history uses short Chinese subjects, often prefixed like `add：`, `优化`, `功能X`. Keep commit titles concise and action-oriented, e.g.:
- `add：巴法云状态上报`
- `优化：WiFi 重连逻辑`

For PRs, include:
- What changed and why.
- Affected modules/files.
- Build/test evidence (commands + result).
- UI screenshots when web page behavior changes.
