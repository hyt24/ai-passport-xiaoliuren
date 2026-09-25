English | [简体中文](README.zh_CN.md)

# Xiao Liu Ren · One Quiet Question

A Chinese divination companion for FoloToy AI Passport. Form a question, cast with three numbers or the current time, and read four short pages in a paper-and-ink interface.

## Use

1. Press `OK` on the home page. Hold it while silently forming a question, then release.
2. Choose number casting (`UP`) or time casting (`DOWN`). For numbers, select three values from 1–9 with `UP` / `DOWN` and confirm each with `OK`.
3. For time casting, press `DOWN` to set up Wi-Fi. Android and iPhone share one QR: scan, join the device hotspot, and choose your 2.4GHz home network on the setup page. Enter its password and save. If the page does not open automatically, visit `http://192.168.4.1`; device `OK` shows a backup URL QR.
4. After successful time synchronization, press `OK` to cast. Read the result with `UP` / `DOWN`; hold `OK` to return home or ask again from the result page.

Number casting works offline. Saved Wi-Fi reconnects on boot. Time casting requires a successful synchronization after each restart; a synchronized clock is usable offline for up to 24 hours. Time is Beijing time (UTC+8), using lunar month/day/hour counting, midnight date rollover and the original month number for leap months. Supported years: 2024–2099. Audio is neither stored nor uploaded. Readings are prompts for reflection.

## Install

Download `xiaoliuren-v0.2.0.bin` and `SHA256SUMS` from [Releases](https://github.com/hyt24/ai-passport-xiaoliuren/releases/latest). This complete image is **only for FoloToy AI Passport (ESP32-C3)** and flashes at `0x0`.

```sh
shasum -a 256 -c SHA256SUMS
esptool --chip esp32c3 --port <device-port> --baud 460800 write_flash 0x0 xiaoliuren-v0.2.0.bin
```

Installing replaces the current firmware. The merged image also resets saved Wi-Fi settings; scan to configure your network after installation. See the [agent installation guide](docs/AGENT_INSTALL_XIAOLIUREN.md).

## Develop

Use ESP-IDF 5.5.3. Do not change board pins based on another device model.

```sh
. /path/to/esp-idf-v5.5.3/export.sh
idf.py build
idf.py -p <device-port> flash
```

Host checks:

```sh
python3 tests/test_xlr_network.py
python3 tests/test_xlr_portal.py
cc -std=c11 -Imain tests/test_xlr_logic.c main/xlr_logic.c -o /tmp/test_xlr_logic
/tmp/test_xlr_logic
cc -std=c11 -Imain tests/test_xlr_time.c main/xlr_calendar.c main/xlr_logic.c main/xlr_net_form.c main/lunar.c -lm -o /tmp/test_xlr_time
/tmp/test_xlr_time
```

The network checks cover provisioning, access restrictions, NTP and fallback behavior. The portal check uses address/undefined-behavior sanitizers for DNS parsing. Build and host checks do not replace testing the screen, buttons and phone setup on hardware.

## Source map

- `main/xlr_app.c`: pages and button interactions.
- `main/xlr_logic.c`, `main/xlr_calendar.c`: casting and calendar conversion.
- `main/xlr_net.c`, `main/xlr_net_form.c`, `main/xlr_portal.c`: Wi-Fi, time synchronization and setup page.
- `components/bsp/`: board drivers and pin definitions.
- `tests/`: host checks; `scripts/`: font generation.
- [Porting notes](PORTING_XIAOLIUREN.md), [hardware guide](docs/AI_HARDWARE_DEVELOPMENT_GUIDE.md), [contributor rules](AGENTS.md).

This repository contains the Xiao Liu Ren application. The inherited hardware-demo pages and example-branch catalog have been removed.

## Credits

The interaction was inspired by the [Xiao Liu Ren web project](https://youvibe.run/s/zhgxVMPRMFb). The device support was adapted from the FoloToy AI Passport development baseline. See [third-party notices](THIRD_PARTY_NOTICES.md), [font license](FONT_LICENSE.md) and [lunar calendar license](main/LUNAR_LICENSE.txt).
