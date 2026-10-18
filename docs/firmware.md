# Firmware

PlatformIO project for the ESP32-S3 in [`esp32/`](../esp32). The only external dependency is ArduinoJson 7; WiFi, HTTPClient, SD and I2S all come with the Arduino core.

## Build and flash

```bash
cd esp32
cp include/config.example.h include/config.h   # set WiFi, BACKEND_URL, pins
pio run -t upload
pio device monitor
```

All compile-time settings (pins, timeouts, sleep timers, audio parameters, SD paths) are in [`config.example.h`](../esp32/include/config.example.h). `config.h` is gitignored.

## Wiring

| Peripheral | Pins (default) |
| --- | --- |
| Nextion UART (115200 baud) | RX 44, TX 43 |
| microSD (SPI, FAT32) | CS 10, MOSI 11, SCK 12, MISO 13 |
| INMP441 (I2S) | WS 4, SCK 5, SD 6 |

## Modules

| Module | What it does |
| --- | --- |
| `src/main.cpp` | Boot sequence, main loop, sleep/wake, reconnect and offline replay |
| `lib/nextion` | Sends commands (terminated with `0xFF 0xFF 0xFF`), reads `prints` commands and the sleep/wake events |
| `lib/commands` | Dispatches `PAGE:<PAGE>:<ACTION>[:<data>]` commands from the display |
| `lib/pages` | One `show…Page()` function per screen; fills in the page's text and visibility |
| `lib/backend` | HTTPS GET/POST to Apps Script, follows its redirect, generates transaction IDs |
| `lib/state` | Cached config (users, products, inventory) and runtime state |
| `lib/storage` | SD card: config cache, offline queue, audit log |
| `lib/audio` | I2S capture and grinder detection, see [audio.md](audio.md) |

The display holds no logic: it reports button presses and the ESP32 decides what to show next. The full command list and the page objects are in [display.md](display.md).

## Boot

1. Connect to WiFi (5 s timeout).
2. Sync time over NTP (CET/CEST) and set the Nextion RTC.
3. Fetch `getConfig` and cache it to `/config.json`.
4. Show the user page.

If WiFi or the backend is unreachable, the device boots from the SD cache and starts in offline mode. If there is no cache either, it stays on the boot screen and retries every minute.

## Offline mode

When a request fails, the device switches to offline mode and the status bar shows "Offline – logging locally". While offline:

- Consume, purchase and undo still work. The POST payload is appended to `/offline.json` and balances show "offline".
- Settling is blocked, because the QR code needs an accurate balance.
- The account and inventory pages are unavailable (they need live data).

Every 60 s the device tries `getUsers`. Once that succeeds it re-fetches the config and replays `/offline.json` in order. The backend ignores duplicate transaction IDs, so replaying is safe.

## SD card

| File | Contents |
| --- | --- |
| `/config.json` | Last `getConfig` response, used for offline boot |
| `/offline.json` | Queue of POST payloads waiting to be sent |
| `/transactions.log` | Append-only JSONL of every transaction attempt. The firmware never reads it; it's there for manual recovery |
| `/grinder_ref.bin` | Optional grinder reference snippet for audio wake |

The device runs without an SD card, but then it has no offline queue and no cache.

## Sleep

The Nextion sleeps on its own after `thsp` seconds without a touch (set per page in `config.h`) and wakes on touch (`thup=1`). It reports both events to the ESP32 (`0x86` for sleep, `0x87` for wake). While the display sleeps and `ENABLE_AUDIO_WAKE` is on, the ESP32 listens for the grinder and wakes the display with `sleep=0`. Every wake goes back to the user page.

## Serial log

Debug output goes to USB serial at 115200 baud. Each line is tagged with its source: `[BOOT]`, `[NET]`, `[SD]`, `[CMD]`, `[PAGE]`, `[AUDIO]`, `[SLEEP]`, `[WAKE]`. Sending `RECORD_REF` over serial records a new grinder reference.
