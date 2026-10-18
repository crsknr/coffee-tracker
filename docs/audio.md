# Grinder wake

An INMP441 microphone listens while the display is asleep and wakes it when the coffee grinder starts. By the time the beans are ground, the screen is already on. It's off by default: set `ENABLE_AUDIO_WAKE true` in `config.h`. With it off, all I2S code is compiled out.

## How detection works

Instead of reacting to loudness, the detector looks for the grinder's waveform shape. Each frame is cross-correlated against a short reference snippet from a real grinder recording. This is the algorithm from [`audio/detection.ipynb`](../audio/detection.ipynb), ported to the ESP32.

1. Read 256 samples from I2S at 16 kHz and split them into four 64-sample frames.
2. Normalise each frame to unit L2 norm (silent frames are skipped).
3. Cross-correlate with the 64-sample reference and take the maximum absolute value.
4. Smooth with an EMA over `XCORR_SMOOTH_WINDOW` frames.
5. If the smoothed value stays above the threshold for `GRINDER_SUSTAINED_MS`, wake the display.

The reference is loaded from `/grinder_ref.bin` on the SD card if it exists. Otherwise the firmware falls back to a built-in array in `audio.cpp`.

## Recording your own reference

Every grinder sounds different, so record a reference with the microphone where it will actually sit:

- on the display: **Info → Audio**, start the grinder, tap **Record**
- or over serial: start the grinder and send `RECORD_REF`

The device records `RECORD_DURATION_MS` of audio, takes 64 samples from the middle and saves them to `/grinder_ref.bin`.

## Tuning

The **Info → Audio** page plots the threshold (channel 0) against the live correlation (channel 1). Start the grinder and use **+ / −** to put the threshold between the grinder's peak and the background noise. The change applies immediately but is lost on reboot. To keep it, update `XCORR_THRESHOLD` in `config.h`.

| Setting                | Default |                                                   |
| ---------------------- | ------- | ------------------------------------------------- |
| `XCORR_THRESHOLD`      | `0.3`   | Detection threshold (0–1)                         |
| `XCORR_SMOOTH_WINDOW`  | `64`    | EMA window in frames (~256 ms)                    |
| `GRINDER_SUSTAINED_MS` | `1500`  | How long the signal must stay above the threshold |
| `RECORD_DURATION_MS`   | `3000`  | Length of an on-device recording                  |

## Python tools

The [`audio/`](../audio) folder has the notebook used to develop the algorithm and two sample recordings (`grinder.wav`, plus `background-dishwasher.wav` for false-positive testing). `extract_reference.py` builds `grinder_ref.bin` from `grinder.wav` and prints the C array used as the built-in fallback:

```bash
cd audio
python3 -m venv .venv && source .venv/bin/activate
pip install numpy scipy
python extract_reference.py
```
