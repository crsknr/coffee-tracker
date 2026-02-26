#!/usr/bin/env python3
"""
extract_reference.py — Generate grinder reference snippet for ESP32 xcorr detector.

Reads test_audio/grinder.wav (48 kHz), decimates to 16 kHz, extracts the middle
FRAME_SIZE samples, L2-normalises them, then writes:
  - grinder_ref.bin   : 64 × float32 little-endian  (copy to SD card root)
  - Prints a C array literal to paste into audio.cpp as the flash fallback
"""

import struct
import sys
from pathlib import Path
from scipy import signal
from scipy.io import wavfile
import numpy as np

FRAME_SIZE      = 64
SOURCE_RATE     = 48000
TARGET_RATE     = 16000
DECIMATE_FACTOR = SOURCE_RATE // TARGET_RATE   # 3

WAV_PATH = Path(__file__).parent / "test_audio" / "grinder.wav"
BIN_PATH = Path(__file__).parent / "grinder_ref.bin"


def main():
    # ── Load WAV ──────────────────────────────────────────────────────────────
    rate, data = wavfile.read(WAV_PATH)
    if rate != SOURCE_RATE:
        print(f"Warning: expected {SOURCE_RATE} Hz, got {rate} Hz — adjusting decimate factor")
        factor = rate // TARGET_RATE
    else:
        factor = DECIMATE_FACTOR

    # Convert to mono float32 [-1, 1]
    if data.ndim > 1:
        data = data[:, 0]
    audio = data.astype(np.float32) / 32768.0

    # ── Decimate to 16 kHz ────────────────────────────────────────────────────
    audio_ds = signal.decimate(audio, factor, ftype="iir").astype(np.float32)
    print(f"Loaded {WAV_PATH.name}: {len(audio)} samples @ {rate} Hz "
          f"→ {len(audio_ds)} samples @ {TARGET_RATE} Hz")

    # ── Extract middle FRAME_SIZE samples ────────────────────────────────────
    mid = len(audio_ds) // 2
    start = mid - FRAME_SIZE // 2
    ref = audio_ds[start : start + FRAME_SIZE].copy()
    if len(ref) != FRAME_SIZE:
        print("Error: could not extract full reference frame", file=sys.stderr)
        sys.exit(1)

    # ── L2-normalise ─────────────────────────────────────────────────────────
    norm = np.linalg.norm(ref)
    if norm > 0:
        ref /= norm
    print(f"Reference snippet: {FRAME_SIZE} samples, norm before={norm:.6f}, "
          f"norm after={np.linalg.norm(ref):.6f}")
    print(f"  min={ref.min():.6f}  max={ref.max():.6f}")

    # ── Write binary for SD card ──────────────────────────────────────────────
    with open(BIN_PATH, "wb") as f:
        f.write(struct.pack(f"<{FRAME_SIZE}f", *ref))
    print(f"\nWrote {BIN_PATH} ({BIN_PATH.stat().st_size} bytes) — copy to SD card root")

    # ── Print C array for flash fallback ──────────────────────────────────────
    print("\n// Paste into audio.cpp as grinder_ref_default[]:")
    print(f"static const float grinder_ref_default[{FRAME_SIZE}] = {{")
    for i in range(0, FRAME_SIZE, 8):
        chunk = ref[i : i + 8]
        vals  = ", ".join(f"{v:+.8f}f" for v in chunk)
        print(f"    {vals},")
    print("};")


if __name__ == "__main__":
    main()
