# Grinder Detection — Audio Wake

The Coffee Tracker wakes its display when the coffee grinder is detected by an INMP441 I2S microphone. Detection is active only while the display is sleeping, and can be disabled entirely with a compile-time flag.

---

## Implementation

### ESP32 Firmware (`audio.h` / `audio.cpp`)

Detection uses simple RMS energy thresholding:

1. **I2S capture** — 1024 samples per frame at 16 kHz mono 16-bit (≈ 64 ms window)
2. **RMS energy** — root mean square of the frame
3. **Threshold check** — if RMS exceeds `GRINDER_DETECTION_THRESHOLD`, increment a sustained counter
4. **Sustained detection** — if the counter × frame duration exceeds `GRINDER_SUSTAINED_MS`, trigger wake
5. **Reset** — any quiet frame resets the counter

The grinder produces sustained broadband noise (~1-8 kHz), distinct from brief ambient events like voices or door slams.

### Lifecycle

- `startAudio()` — called when the display enters sleep (`0x86` event), initialises the I2S driver
- `processAudioFrame()` — called every loop iteration while sleeping; returns `true` on detection
- `stopAudio()` — called immediately on detection or touch wake (`0x87`), frees the I2S driver

### Config

All constants are in `src/config.h`:

| Constant | Default | Description |
| -------- | ------- | ----------- |
| `ENABLE_AUDIO_WAKE` | `false` | Set `true` to enable; `false` compiles out all I2S code |
| `I2S_SAMPLE_RATE` | `16000` | Sample rate in Hz |
| `I2S_BUFFER_SIZE` | `1024` | Samples per DMA frame (≈ 64 ms) |
| `GRINDER_DETECTION_THRESHOLD` | `3000` | RMS level that counts as "loud" |
| `GRINDER_SUSTAINED_MS` | `1500` | Consecutive ms above threshold before triggering |
| `I2S_WS_PIN` / `SCK_PIN` / `SD_PIN` | 4 / 5 / 6 | INMP441 wiring pins |

### Disabling for testing

```c
#define ENABLE_AUDIO_WAKE false
```

When `false`, all I2S code is compiled out via `#if ENABLE_AUDIO_WAKE` guards. The display still wakes on touch normally.

---

## Tuning

1. Set `ENABLE_AUDIO_WAKE true` and flash
2. Open serial monitor (`pio device monitor`)
3. Run the grinder — watch for `[AUDIO] Grinder detected (RMS=XXXX, sustained=XXXXms)` lines
4. In a quiet room, note the ambient RMS; with the grinder on, note the grinder RMS
5. Set `GRINDER_DETECTION_THRESHOLD` to a value between the two
6. Adjust `GRINDER_SUSTAINED_MS` if the detection triggers too eagerly or too slowly

Typical values depend heavily on microphone placement and enclosure acoustics — expect to tune empirically on your actual hardware.

---

## Python Prototype (`audio/`)

The `audio/` directory contains a development prototype used to analyse audio signatures before implementing the embedded version:

| File | Purpose |
| ---- | ------- |
| `grinder_detection.py` | Cross-correlation + Welch PSD analysis, tests at 48 kHz and 8 kHz |
| `detection.ipynb` | Jupyter notebook with frequency spectrum visualisations |
| `test_audio/grinder.wav` | Sample grinder recording |
| `test_audio/background-dishwasher.wav` | Sample background noise recording |

### Running the prototype

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install numpy scipy matplotlib jupyter
python audio/grinder_detection.py
```

The prototype confirmed that RMS energy thresholding is sufficient for this use case — the grinder is significantly louder than typical office background noise.

---

## Future Improvements

- **Spectral flatness check** — grinder is broadband; speech has harmonic peaks. An FFT-based flatness measure would reduce false positives.
- **Adaptive threshold** — baseline the ambient noise floor automatically at boot.
- **Debounce** — suppress re-triggers for N seconds after detection.
- **ULP coprocessor** — for true ultra-low-power standby (current implementation keeps the main CPU running during display sleep).
