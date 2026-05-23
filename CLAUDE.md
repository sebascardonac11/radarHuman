# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

POC: ESP8266 (WeMos D1 Mini / ESP-12E) reads the OUT pin of an HLK-LD2410B 24GHz mmWave human presence radar sensor and drives a relay via an NPN transistor (BC547/2N2222).

## Build & Flash (PlatformIO)

```bash
# Build
pio run

# Flash to device (port: /dev/cu.usbmodem14301)
pio run --target upload

# Serial monitor (115200 baud, with timestamps and color)
pio device monitor

# Build + flash + monitor in one
pio run --target upload && pio device monitor
```

PlatformIO targets the `esp8266` env defined in [platformio.ini](platformio.ini). Only `src/main.cpp` is compiled (`build_src_filter = +<main.cpp>`); the `.ino` file in `src/radarHuman/` is an archived prototype and is excluded from the build.

## Hardware Architecture

**Active file:** [src/main.cpp](src/main.cpp) — targets ESP8266-12F.

**UART connections:**
| Signal | ESP8266-12F pin | Connects to |
|---|---|---|
| RX (UART0) | GPIO3 | LD2410B TX |
| TX (UART0) | GPIO1 | LD2410B RX |
| TX (UART1, debug) | GPIO2 (D4) | USB-serial adapter (development only) |
| `RELAY_PIN` (OUTPUT) | GPIO4 | NPN transistor base via 1 kΩ |

`Serial` (UART0) runs at 256 000 baud for the LD2410B. `Serial1` (UART1, TX-only) runs at 115 200 baud for debug output during development — connect a USB-serial adapter to GPIO2 to see logs.

**Relay drive circuit:**
```
GPIO4 ──[1kΩ]── Base (BC547 / 2N2222)
                Collector ── IN (5V relay module, active-low)
                Emitter  ── GND
GPIO4 HIGH → transistor ON → relay IN = LOW → relay energized
GPIO4 LOW  → transistor OFF → relay IN = HIGH → relay off
```

**Frame parser:** `parseByte()` implements a 4-state machine (SYNC → RD_LEN → RD_DATA → RD_END) over the LD2410B binary protocol. `onFrame()` checks `frameBuf[8]` (target state) — any non-zero value means presence. The relay turns ON immediately on first detection and stays ON for `HOLD_MS` (2 s) after the last detected frame.

**Archived prototype:** [src/radarHuman/radarHuman.ino](src/radarHuman/radarHuman.ino) — earlier ESP-01 variant that drives an LED directly instead of a relay. Not part of the active build.

## Key Hardware Docs

- LD2410B serial protocol and BLE config tool: `Doc/HLK-LD2410B human presence detection with BLE/`
- ESP8266 pinout reference: `Doc/ESP8266-WeMos-D1-Mini-pinout-gpio-pin.webp`
- BC547 transistor datasheet: `Doc/BC547.PDF`
- KiCad schematic & PCB: `Doc/radarHuman/`
- Official docs (Google Drive): https://drive.google.com/drive/folders/16zI-fium_BZeP08EyQke0rWp0BJTMvw3
