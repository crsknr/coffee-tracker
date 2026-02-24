# Coffee Tracker

> A self-hosted, serverless coffee pool tracker for small offices — built with an ESP32-S3, Nextion touchscreen, and Google Sheets.

---

## Overview

Coffee Tracker is a shared coffee accounting system for small teams. Members log coffees on a touchscreen device; the system tracks consumption, inventory, and balances. When a user's balance drops below a threshold, it shows a PayPal QR code for quick settlement. The treasurer receives Telegram notifications for payments and low stock.

No server required. Google Sheets acts as the database; a Google Apps Script handles all business logic.

---

## Features

- **Multi-user** — up to 16 users, paginated in groups of 8
- **Consume** — log single/double espresso + cow or oat milk portions per user
- **Purchase** — log inventory restocks (beans, water, milk); balance credited to the buyer
- **Settle** — PayPal QR code auto-generated from outstanding balance; Telegram notification sent to treasurer
- **Undo** — reverse the most recent transaction directly from the done screen
- **Offline mode** — transactions logged to SD card when WiFi is down; auto-replayed on reconnect
- **Grinder wake** — INMP441 microphone detects the coffee grinder and wakes the display from sleep
- **Inventory traffic lights** — red/yellow/green status per item based on configurable thresholds
- **Telegram alerts** — daily inventory report + instant settlement notifications
- **Google Sheets backend** — no server, accessible spreadsheet for manual edits

---

## Hardware

| Component | Model |
| --------- | ----- |
| Microcontroller | ESP32-S3 DevKit (any variant with spare GPIO) |
| Display | Nextion NX4832K035 (3.5", 480×320) |
| Microphone | INMP441 I2S MEMS |
| Storage | microSD card module (SPI) |
| Enclosure | Custom 3D-printed case |

3D files (FreeCAD + STL) are in [`case/`](case/).

---

## Project Structure

```text
coffee-tracker/
├── esp32/                        # PlatformIO firmware (ESP32-S3)
│   ├── platformio.ini
│   ├── include/
│   │   └── config.example.h      # Template — copy to config.h and fill in
│   ├── src/
│   │   └── main.cpp              # Boot sequence + main loop
│   └── lib/
│       ├── nextion/              # Nextion serial protocol
│       ├── backend/              # HTTP client (Google Apps Script)
│       ├── storage/              # SD card (cache, offline log, audit log)
│       ├── state/                # Application state
│       ├── pages/                # Page display functions (all 10 screens)
│       ├── commands/             # Command dispatcher (core business logic)
│       └── audio/                # Grinder detection (I2S + RMS)
├── backend/
│   ├── backend.gs                # Google Apps Script (paste into Apps Script editor)
│   └── test-api.sh               # cURL-based API test script
├── nextion/
│   ├── coffee-tracker.HMI        # Nextion Editor project file
│   └── media/                    # Source assets (icons, backgrounds)
├── audio/
│   ├── grinder_detection.py      # Python detection prototype
│   ├── detection.ipynb           # Jupyter frequency analysis notebook
│   └── test_audio/               # Sample WAV files for development
├── case/                         # FreeCAD project + exported STL files
└── docs/
    ├── backend.md                # API reference (endpoints, request/response format)
    ├── esp32.md                  # Firmware specification
    ├── ui-ux.md                  # Screen specs, command protocol, flow diagrams
    └── audio-wake-up.md          # Grinder detection algorithm
```

---

## Quick Start

### 1. Backend (Google Sheets + Apps Script)

1. Create a new Google Sheet
2. Open **Extensions → Apps Script**
3. Paste the contents of [`backend/backend.gs`](backend/backend.gs)
4. Run `initializeSheets()` to create all sheet tabs and columns
5. Fill in the **Config** tab: `treasurer`, `paypal_link`, `balance_threshold`
6. (Optional) Add a Telegram bot token and chat ID for notifications, then run `createDailyTrigger()`
7. **Deploy as Web App** (Execute as: Me, Access: Anyone) → copy the URL
8. Test with [`backend/test-api.sh`](backend/test-api.sh) (fill in your URL first)

Full details: [docs/backend.md](docs/backend.md)

### 2. ESP32 Firmware

Requires [PlatformIO](https://platformio.org/).

```bash
cd esp32
cp include/config.example.h include/config.h
```

Edit `include/config.h`:

- `WIFI_SSID` / `WIFI_PASSWORD`
- `BACKEND_URL` — your deployed Apps Script URL
- GPIO pin assignments for your specific ESP32-S3 board

```bash
pio run              # compile
pio run -t upload    # flash
pio device monitor   # serial debug output
```

Full details: [docs/esp32.md](docs/esp32.md)

### 3. Nextion Display

1. Install [Nextion Editor](https://nextion.tech/nextion-editor/)
2. Open [`nextion/coffee-tracker.HMI`](nextion/coffee-tracker.HMI)
3. Compile and upload to the display (via USB-TTL or directly from the Editor)

Full screen and command protocol: [docs/ui-ux.md](docs/ui-ux.md)

---

## Configuration

All compile-time settings live in `esp32/include/config.h` (gitignored; copy from `config.example.h`):

| Constant | Default | Description |
| -------- | ------- | ----------- |
| `WIFI_SSID` / `WIFI_PASSWORD` | — | WiFi credentials |
| `BACKEND_URL` | — | Google Apps Script web app URL |
| `ENABLE_AUDIO_WAKE` | `false` | Enable grinder detection wake |
| `NEXTION_RX_PIN` / `TX_PIN` | 38 / 39 | Nextion UART pins |
| `SD_CS_PIN` etc. | 10-13 | SPI pins for SD card |
| `I2S_WS_PIN` / `SCK_PIN` / `SD_PIN` | 4 / 5 / 6 | I2S pins for INMP441 |
| `GRINDER_DETECTION_THRESHOLD` | 3000 | RMS energy threshold for grinder detection |

Runtime configuration (users, prices, thresholds, PayPal link) is managed in the Google Sheet **Config** tab and fetched at boot.

---

## Docs

| Document | Contents |
| -------- | -------- |
| [docs/backend.md](docs/backend.md) | API endpoints, request/response schemas, sheet structure, Telegram integration |
| [docs/ui-ux.md](docs/ui-ux.md) | All 10 screens, Nextion command protocol, flow diagrams, sleep/wake behavior |
| [docs/esp32.md](docs/esp32.md) | Firmware architecture, offline mode, SD card layout, state machine |
| [docs/audio-wake-up.md](docs/audio-wake-up.md) | Grinder detection algorithm, tuning guide, Python prototype |

---

## License

MIT — see [LICENSE](LICENSE)
