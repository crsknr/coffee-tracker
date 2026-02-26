# Grinder Detection — Audio Wake

The Coffee Tracker wakes its display when the coffee grinder is detected by an INMP441 I2S microphone. Detection is active only while the display is sleeping, and can be disabled entirely with a compile-time flag.

---

## Implementation

### ESP32 Firmware (`audio.h` / `audio.cpp`)

Detection uses cross-correlation against a short reference snippet taken from a real grinder recording. This makes the detector shape-specific (grinder waveform) rather than just volume-specific (any loud noise).

**Per-frame algorithm** (ported from `audio/detection.ipynb`, Config 2 — 16 kHz):

1. **I2S capture** — reads a DMA buffer (4 × `XCORR_FRAME_SIZE` = 256 samples, ≈ 16 ms); split into 4 × 64-sample frames
2. **Float conversion** — `int16 / 32768.0` → float32 in `[-1, 1]`
3. **L2 normalise** — each frame and the reference are scaled to unit norm; silent frames (norm < 1e-9) are skipped
4. **Cross-correlation** — `xcorrMax(frame, reference, 64)`: slides the reference across the frame (mode='same', 64 lags), returns the maximum absolute dot-product
5. **EMA smoothing** — `energy = alpha × rawEnergy + (1-alpha) × energy` where `alpha = 1 / XCORR_SMOOTH_WINDOW`
6. **Sustained detection** — if `energy > XCORR_THRESHOLD` for `GRINDER_SUSTAINED_MS` consecutive milliseconds, `processAudioFrame()` returns `true` and the display wakes

### Reference Snippet

A 64-sample float32 vector normalised to unit L2 norm. Loaded in priority order at `startAudio()`:

1. **SD card** — `/grinder_ref.bin` (256 bytes). Copy there with `audio/extract_reference.py` or record on-device (see below). Can be updated without reflashing.
2. **Flash fallback** — hardcoded array in `audio.cpp`, generated from `test_audio/grinder.wav` by `extract_reference.py`. Always available.

### On-Device Reference Recording (`RECORD_REF`)

To capture a reference snippet directly from your microphone (best for real-world performance):

1. Set `ENABLE_AUDIO_WAKE true` and flash
2. Place the microphone where it will be during normal use
3. Open serial monitor (`pio device monitor`)
4. Start the grinder and immediately send `RECORD_REF` (+ Enter) over serial
5. The device records `RECORD_DURATION_MS` of audio, extracts the middle 64 samples, L2-normalises, and saves to `/grinder_ref.bin` on the SD card
6. Serial confirms: `[AUDIO] Reference saved to /grinder_ref.bin`

The new reference is loaded immediately and persists across reboots.

### Lifecycle

- `startAudio()` — called on display sleep (`0x86`); initialises I2S and loads the reference snippet
- `processAudioFrame()` — called every loop iteration while sleeping; returns `true` on sustained detection
- `stopAudio()` — called on detection or touch wake (`0x87`); frees the I2S driver
- `recordReference()` — called from the `RECORD_REF` serial command while awake

### Config

All constants are in `include/config.h`:

| Constant | Default | Description |
| -------- | ------- | ----------- |
| `ENABLE_AUDIO_WAKE` | `false` | Set `true` to enable; `false` compiles out all I2S / audio code |
| `I2S_SAMPLE_RATE` | `16000` | Sample rate in Hz |
| `XCORR_FRAME_SIZE` | `64` | Samples per xcorr frame (4 ms @ 16 kHz) |
| `XCORR_THRESHOLD` | `0.4` | Detection threshold (0–1; lower = more sensitive) |
| `XCORR_SMOOTH_WINDOW` | `64` | EMA window in frames (≈ 256 ms) |
| `GRINDER_SUSTAINED_MS` | `1500` | Consecutive ms above threshold before triggering |
| `RECORD_DURATION_MS` | `3000` | Length of on-device reference recording |
| `GRINDER_REF_PATH` | `"/grinder_ref.bin"` | SD card path for the reference snippet |
| `I2S_WS_PIN` / `SCK_PIN` / `SD_PIN` | 4 / 5 / 6 | INMP441 wiring pins |

### Disabling for testing

```c
#define ENABLE_AUDIO_WAKE false
```

When `false`, all I2S / audio code is compiled out via `#if ENABLE_AUDIO_WAKE` guards. The display still wakes on touch normally.

---

## Tuning

1. Set `ENABLE_AUDIO_WAKE true` and flash
2. Open serial monitor
3. Watch the `[AUDIO] energy=X.XXX sustained=XXXms` log lines:
   - **Grinder on:** energy should climb above `XCORR_THRESHOLD` (0.4) quickly
   - **Background noise:** energy should stay well below 0.4
4. If there are false positives: raise `XCORR_THRESHOLD` (e.g. 0.5) or increase `GRINDER_SUSTAINED_MS`
5. If the grinder is missed: lower `XCORR_THRESHOLD` or record a new on-device reference with `RECORD_REF`

---

## Python Prototype & Tools (`audio/`)

| File | Purpose |
| ---- | ------- |
| `detection.ipynb` | Jupyter notebook — algorithm development, cross-correlation + EMA analysis at 48 kHz and 16 kHz |
| `extract_reference.py` | Generates `grinder_ref.bin` (for SD) and the C array fallback from `test_audio/grinder.wav` |
| `test_audio/grinder.wav` | Sample grinder recording (48 kHz) |
| `test_audio/background-dishwasher.wav` | Sample background noise for false-positive testing |

### Running the tools

```bash
cd audio
python3 -m venv .venv && source .venv/bin/activate
pip install numpy scipy
python extract_reference.py
# → writes grinder_ref.bin (copy to SD card root)
# → prints C array to paste into audio.cpp if updating the flash fallback
```

---

## Future Improvements

- **ULP coprocessor** — for true ultra-low-power standby (current implementation keeps the main CPU running during display sleep).
- **Multi-frame reference** — average several frames to get a more representative template.
- **Adaptive EMA baseline** — track long-term ambient energy to auto-adjust the threshold.
