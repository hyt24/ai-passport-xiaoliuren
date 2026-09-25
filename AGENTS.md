# Repository Guidelines

## Project Structure & Module Organization

This repository is the Xiao Liu Ren application and its ESP-IDF BSP for the ESP32-C3-based FoloToy AI Passport.

- `components/bsp/include/`: public BSP APIs and the hardware pin/configuration source of truth (`bsp_pins.h`).
- `components/bsp/src/`: display, button, audio, battery, and shared-I2C implementations.
- `main/`: Xiao Liu Ren pages, casting/calendar logic, networking and assets. The entry point starts `xlr_app`; there is no hardware-demo menu.
- `sdkconfig.defaults`: reproducible target, console, LVGL, and memory defaults.
- `README.md`: wiring, known hardware traps, and the required on-device acceptance checklist.

Keep reusable hardware logic in `components/bsp`; keep board demonstration and UI behavior in `main`.

## Build, Test, and Development Commands

Use ESP-IDF 5.5.x:

```bash
get_idf553                    # Enter the repository's ESP-IDF 5.5.3 environment
idf.py set-target esp32c3     # Configure a fresh checkout
idf.py build                  # Compile firmware and validate dependencies
idf.py flash monitor          # Flash the connected board and open logs
idf.py fullclean              # Remove generated build state when configuration is stale
```

Host checks are in `tests/`; run the commands in README.md. Treat a clean `idf.py build` as the minimum check, then run every applicable item in the README acceptance checklist on real hardware.

## Coding Style & Naming Conventions

Write C using four-space indentation and K&R-style braces, following nearby files. Use `snake_case` for functions and locals, `BSP_*` for public hardware constants, and `s_` for file-local state. Keep BSP APIs prefixed with `bsp_`. Prefer `static` for internal symbols. User-facing UI text stays Chinese; explanatory comments may be Chinese. Preserve comments documenting hardware-specific register values and memory constraints.

## Testing Guidelines

Before submitting, build from the repository root and inspect warnings. On hardware, verify menu navigation and the affected Display, Button, Audio, or Battery page. For pin, display-rotation, codec-clock, ADC, or DMA changes, explicitly record the observed hardware result in the PR. Do not increase LVGL buffers or audio allocations without checking ESP32-C3 internal RAM usage; the board has no PSRAM.

## Commit & Pull Request Guidelines

History follows Conventional Commit-style subjects such as `feat(bsp): ...`, `feat(demo): ...`, `fix(bsp): ...`, and `docs: ...`. Keep commits focused by subsystem. Pull requests should explain the hardware/revision tested, summarize behavior changes, list build and on-device results, and include photos or screenshots for display changes. Link related issues and call out wiring, pin-map, or compatibility impacts.
