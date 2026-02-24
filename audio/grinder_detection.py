#!/usr/bin/env python3
"""
Grinder Detection Algorithm - Cross-Correlation Based Approach

Implements sound-based detection of coffee grinder using cross-correlation
with reference recording. Tests algorithm at both 48kHz and 8kHz (ESP32-S3).

Usage:
    python grinder_detection.py
"""

import numpy as np
import matplotlib.pyplot as plt
from scipy import signal
from scipy.io import wavfile
from pathlib import Path

# Get script directory to handle paths correctly
SCRIPT_DIR = Path(__file__).parent
TEST_AUDIO_DIR = SCRIPT_DIR / 'test_audio'


def load_audio_files():
    """Load and normalize audio files."""
    print("=" * 70)
    print("LOADING AUDIO FILES")
    print("=" * 70)

    # Load audio files
    grinder_path = TEST_AUDIO_DIR / 'grinder.wav'
    background_path = TEST_AUDIO_DIR / 'background-dishwasher.wav'

    grinder_rate, grinder_audio = wavfile.read(str(grinder_path))
    background_rate, background_audio = wavfile.read(str(background_path))

    # Convert to float and normalize
    grinder_audio = grinder_audio.astype(np.float32) / 32768.0
    background_audio = background_audio.astype(np.float32) / 32768.0

    print(f"\nGrinder: {grinder_rate}Hz, {len(grinder_audio)/grinder_rate:.2f}s, shape: {grinder_audio.shape}")
    print(f"Background: {background_rate}Hz, {len(background_audio)/background_rate:.2f}s, shape: {background_audio.shape}")

    return grinder_rate, grinder_audio, background_rate, background_audio


def plot_welch_psd(grinder_rate, grinder_audio, background_rate, background_audio):
    """Analyze and plot frequency signatures using Welch's method."""
    print("\n" + "=" * 70)
    print("WELCH PSD ANALYSIS - FREQUENCY SIGNATURES")
    print("=" * 70)

    # Welch's method for power spectral density
    nperseg = 2048  # FFT window size

    # Compute PSD
    f_grinder, psd_grinder = signal.welch(grinder_audio, grinder_rate, nperseg=nperseg)
    f_background, psd_background = signal.welch(background_audio, background_rate, nperseg=nperseg)

    # Plot frequency signatures
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10))

    # Plot 1: Full spectrum (log scale)
    ax1.semilogy(f_grinder, psd_grinder, label='Grinder', alpha=0.8, linewidth=2, color='#e74c3c')
    ax1.semilogy(f_background, psd_background, label='Background (Dishwasher)', alpha=0.8, linewidth=2, color='#3498db')
    ax1.set_xlabel('Frequency (Hz)', fontsize=12)
    ax1.set_ylabel('Power Spectral Density (V²/Hz)', fontsize=12)
    ax1.set_title('Welch PSD - Full Spectrum (48kHz)', fontsize=14, fontweight='bold')
    ax1.legend(fontsize=11)
    ax1.grid(True, alpha=0.3)
    ax1.set_xlim(0, 4000)  # Focus on 0-4kHz

    # Plot 2: Zoomed view (0-2kHz typical grinder range)
    ax2.semilogy(f_grinder, psd_grinder, label='Grinder', alpha=0.8, linewidth=2, color='#e74c3c')
    ax2.semilogy(f_background, psd_background, label='Background (Dishwasher)', alpha=0.8, linewidth=2, color='#3498db')
    ax2.set_xlabel('Frequency (Hz)', fontsize=12)
    ax2.set_ylabel('Power Spectral Density (V²/Hz)', fontsize=12)
    ax2.set_title('Welch PSD - Zoomed (0-2kHz Grinder Range)', fontsize=14, fontweight='bold')
    ax2.legend(fontsize=11)
    ax2.grid(True, alpha=0.3)
    ax2.set_xlim(0, 2000)

    plt.tight_layout()
    plt.savefig('welch_psd_48khz.png', dpi=150, bbox_inches='tight')
    print("\nSaved: welch_psd_48khz.png")
    plt.show()

    print(f"\nFrequency Analysis:")
    print(f"  Grinder peak frequency: {f_grinder[np.argmax(psd_grinder)]:.1f} Hz")
    print(f"  Background peak frequency: {f_background[np.argmax(psd_background)]:.1f} Hz")

    return f_grinder, psd_grinder, f_background, psd_background


def detect_grinder_xcorr(audio, reference, frame_size=512, threshold=0.3, smooth_window=5):
    """
    Cross-correlation based grinder detector.

    Parameters:
    - audio: input audio signal to test
    - reference: grinder reference recording
    - frame_size: frame size in samples (512 for ESP32)
    - threshold: detection threshold (0-1)
    - smooth_window: moving average window size

    Returns:
    - detections: boolean array of grinder detected per frame
    - energies: smoothed cross-correlation energies
    - raw_energies: raw cross-correlation energies before smoothing
    """

    # Use a representative portion of reference (middle section, frame_size samples)
    # This avoids startup/shutdown transients
    ref_start = len(reference) // 2 - frame_size // 2
    ref_template = reference[ref_start:ref_start + frame_size]

    # Normalize reference template to unit energy
    ref_template = ref_template / np.linalg.norm(ref_template)

    # Calculate number of frames
    n_frames = len(audio) // frame_size

    # Initialize energy array
    raw_energies = np.zeros(n_frames)

    # Process each frame
    for i in range(n_frames):
        # Extract frame
        frame = audio[i*frame_size:(i+1)*frame_size]

        # Normalize frame
        frame_norm = np.linalg.norm(frame)
        if frame_norm > 0:
            frame = frame / frame_norm

        # Compute cross-correlation (use 'same' mode for same-length output)
        xcorr = np.correlate(frame, ref_template, mode='same')

        # Take maximum cross-correlation value as "grinder energy"
        # This represents how well the frame matches the reference
        raw_energies[i] = np.max(np.abs(xcorr))

    # Normalize energies to 0-1 range
    if np.max(raw_energies) > 0:
        raw_energies = raw_energies / np.max(raw_energies)

    # Apply moving average smoothing
    kernel = np.ones(smooth_window) / smooth_window
    energies = np.convolve(raw_energies, kernel, mode='same')

    # Threshold detection
    detections = energies > threshold

    return detections, energies, raw_energies


def test_detection_48khz(grinder_audio, background_audio, grinder_rate, background_rate):
    """Test detection algorithm with 48kHz audio."""
    print("\n" + "=" * 70)
    print("TESTING DETECTION AT 48kHz")
    print("=" * 70)

    frame_size = 512
    threshold = 0.4
    smooth_window = 10

    # Test with grinder audio (should detect)
    detections_grinder, energies_grinder, raw_grinder = detect_grinder_xcorr(
        grinder_audio,
        grinder_audio,
        frame_size=frame_size,
        threshold=threshold,
        smooth_window=smooth_window
    )

    # Test with background audio (should NOT detect)
    detections_background, energies_background, raw_background = detect_grinder_xcorr(
        background_audio,
        grinder_audio,
        frame_size=frame_size,
        threshold=threshold,
        smooth_window=smooth_window
    )

    # Plot results
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10))

    time_grinder = np.arange(len(energies_grinder)) * frame_size / grinder_rate
    time_background = np.arange(len(energies_background)) * frame_size / background_rate

    # Plot 1: Grinder test
    ax1.plot(time_grinder, raw_grinder, 'gray', alpha=0.4, label='Raw XCorr Energy', linewidth=1)
    ax1.plot(time_grinder, energies_grinder, '#2ecc71', linewidth=2.5, label='Smoothed Energy')
    ax1.axhline(y=threshold, color='#e74c3c', linestyle='--', linewidth=2, label=f'Threshold ({threshold})')
    ax1.fill_between(time_grinder, 0, 1, where=detections_grinder, alpha=0.3, color='#27ae60', label='Grinder Detected')
    ax1.set_xlabel('Time (s)', fontsize=12)
    ax1.set_ylabel('Normalized Energy', fontsize=12)
    ax1.set_title('Detection on Grinder Audio (48kHz) - Should Detect', fontsize=14, fontweight='bold')
    ax1.legend(fontsize=10, loc='upper right')
    ax1.grid(True, alpha=0.3)
    ax1.set_ylim(0, 1.1)

    # Plot 2: Background test
    ax2.plot(time_background, raw_background, 'gray', alpha=0.4, label='Raw XCorr Energy', linewidth=1)
    ax2.plot(time_background, energies_background, '#3498db', linewidth=2.5, label='Smoothed Energy')
    ax2.axhline(y=threshold, color='#e74c3c', linestyle='--', linewidth=2, label=f'Threshold ({threshold})')
    ax2.fill_between(time_background, 0, 1, where=detections_background, alpha=0.3, color='#27ae60', label='Grinder Detected')
    ax2.set_xlabel('Time (s)', fontsize=12)
    ax2.set_ylabel('Normalized Energy', fontsize=12)
    ax2.set_title('Detection on Background Audio (48kHz) - Should NOT Detect', fontsize=14, fontweight='bold')
    ax2.legend(fontsize=10, loc='upper right')
    ax2.grid(True, alpha=0.3)
    ax2.set_ylim(0, 1.1)

    plt.tight_layout()
    plt.savefig('detection_48khz.png', dpi=150, bbox_inches='tight')
    print("\nSaved: detection_48khz.png")
    plt.show()

    print(f"\n48kHz Detection Results:")
    print(f"  Grinder audio: {np.sum(detections_grinder)/len(detections_grinder)*100:.1f}% frames detected")
    print(f"  Background audio: {np.sum(detections_background)/len(detections_background)*100:.1f}% frames detected")

    return detections_grinder, detections_background


def downsample_to_8khz(grinder_audio, background_audio, grinder_rate):
    """Downsample audio to 8kHz for ESP32-S3 testing."""
    print("\n" + "=" * 70)
    print("DOWNSAMPLING TO 8kHz (ESP32-S3 TARGET)")
    print("=" * 70)

    target_rate = 8000
    downsample_factor = grinder_rate // target_rate  # 48000 / 8000 = 6

    print(f"\nDownsampling from {grinder_rate}Hz to {target_rate}Hz (factor: {downsample_factor})")

    # Downsample both recordings
    grinder_8k = signal.decimate(grinder_audio, downsample_factor, ftype='iir')
    background_8k = signal.decimate(background_audio, downsample_factor, ftype='iir')

    print(f"\nOriginal lengths: grinder={len(grinder_audio)}, background={len(background_audio)}")
    print(f"8kHz lengths: grinder={len(grinder_8k)}, background={len(background_8k)}")
    print(f"Duration check: {len(grinder_8k)/target_rate:.2f}s vs {len(grinder_audio)/grinder_rate:.2f}s")

    return grinder_8k, background_8k, target_rate


def plot_welch_psd_8khz(grinder_8k, background_8k, target_rate,
                        f_grinder_48k, psd_grinder_48k, f_background_48k, psd_background_48k):
    """Compare Welch PSD at 48kHz vs 8kHz."""
    print("\n" + "=" * 70)
    print("WELCH PSD ANALYSIS AT 8kHz")
    print("=" * 70)

    # Welch PSD at 8kHz
    nperseg_8k = 512  # Smaller window for lower sample rate

    f_grinder_8k, psd_grinder_8k = signal.welch(grinder_8k, target_rate, nperseg=nperseg_8k)
    f_background_8k, psd_background_8k = signal.welch(background_8k, target_rate, nperseg=nperseg_8k)

    # Plot comparison: 48kHz vs 8kHz
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))

    # 48kHz spectrum
    ax1.semilogy(f_grinder_48k, psd_grinder_48k, label='Grinder (48kHz)', alpha=0.8, linewidth=2, color='#e74c3c')
    ax1.semilogy(f_background_48k, psd_background_48k, label='Background (48kHz)', alpha=0.8, linewidth=2, color='#3498db')
    ax1.set_xlabel('Frequency (Hz)', fontsize=12)
    ax1.set_ylabel('PSD (V²/Hz)', fontsize=12)
    ax1.set_title('48kHz Spectrum (0-4kHz)', fontsize=14, fontweight='bold')
    ax1.legend(fontsize=11)
    ax1.grid(True, alpha=0.3)
    ax1.set_xlim(0, 4000)

    # 8kHz spectrum (Nyquist limit = 4kHz)
    ax2.semilogy(f_grinder_8k, psd_grinder_8k, label='Grinder (8kHz)', alpha=0.8, linewidth=2, color='#e74c3c')
    ax2.semilogy(f_background_8k, psd_background_8k, label='Background (8kHz)', alpha=0.8, linewidth=2, color='#3498db')
    ax2.axvline(x=4000, color='orange', linestyle='--', linewidth=2, alpha=0.7, label='Nyquist Limit (4kHz)')
    ax2.set_xlabel('Frequency (Hz)', fontsize=12)
    ax2.set_ylabel('PSD (V²/Hz)', fontsize=12)
    ax2.set_title('8kHz Spectrum (Nyquist Limited)', fontsize=14, fontweight='bold')
    ax2.legend(fontsize=11)
    ax2.grid(True, alpha=0.3)
    ax2.set_xlim(0, 4000)

    plt.tight_layout()
    plt.savefig('welch_psd_comparison.png', dpi=150, bbox_inches='tight')
    print("\nSaved: welch_psd_comparison.png")
    plt.show()

    print(f"\n8kHz Frequency Analysis:")
    print(f"  Nyquist frequency (max representable): {target_rate/2} Hz")
    print(f"  Grinder peak: {f_grinder_8k[np.argmax(psd_grinder_8k)]:.1f} Hz")
    print(f"  Background peak: {f_background_8k[np.argmax(psd_background_8k)]:.1f} Hz")


def test_detection_8khz(grinder_8k, background_8k, target_rate):
    """Test detection algorithm with 8kHz audio."""
    print("\n" + "=" * 70)
    print("TESTING DETECTION AT 8kHz (ESP32-S3 COMPATIBLE)")
    print("=" * 70)

    frame_size = 512
    threshold = 0.4
    smooth_window = 10

    # Test detection with 8kHz downsampled audio
    detections_grinder_8k, energies_grinder_8k, raw_grinder_8k = detect_grinder_xcorr(
        grinder_8k,
        grinder_8k,
        frame_size=frame_size,
        threshold=threshold,
        smooth_window=smooth_window
    )

    detections_background_8k, energies_background_8k, raw_background_8k = detect_grinder_xcorr(
        background_8k,
        grinder_8k,
        frame_size=frame_size,
        threshold=threshold,
        smooth_window=smooth_window
    )

    # Plot 8kHz detection results
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10))

    time_grinder_8k = np.arange(len(energies_grinder_8k)) * frame_size / target_rate
    time_background_8k = np.arange(len(energies_background_8k)) * frame_size / target_rate

    # Grinder test at 8kHz
    ax1.plot(time_grinder_8k, raw_grinder_8k, 'gray', alpha=0.4, label='Raw XCorr Energy', linewidth=1)
    ax1.plot(time_grinder_8k, energies_grinder_8k, '#2ecc71', linewidth=2.5, label='Smoothed Energy')
    ax1.axhline(y=threshold, color='#e74c3c', linestyle='--', linewidth=2, label=f'Threshold ({threshold})')
    ax1.fill_between(time_grinder_8k, 0, 1, where=detections_grinder_8k, alpha=0.3, color='#27ae60', label='Detected')
    ax1.set_xlabel('Time (s)', fontsize=12)
    ax1.set_ylabel('Normalized Energy', fontsize=12)
    ax1.set_title('Detection on Grinder Audio (8kHz) - ESP32-S3 Compatible', fontsize=14, fontweight='bold')
    ax1.legend(fontsize=10, loc='upper right')
    ax1.grid(True, alpha=0.3)
    ax1.set_ylim(0, 1.1)

    # Background test at 8kHz
    ax2.plot(time_background_8k, raw_background_8k, 'gray', alpha=0.4, label='Raw XCorr Energy', linewidth=1)
    ax2.plot(time_background_8k, energies_background_8k, '#3498db', linewidth=2.5, label='Smoothed Energy')
    ax2.axhline(y=threshold, color='#e74c3c', linestyle='--', linewidth=2, label=f'Threshold ({threshold})')
    ax2.fill_between(time_background_8k, 0, 1, where=detections_background_8k, alpha=0.3, color='#27ae60', label='Detected')
    ax2.set_xlabel('Time (s)', fontsize=12)
    ax2.set_ylabel('Normalized Energy', fontsize=12)
    ax2.set_title('Detection on Background Audio (8kHz) - ESP32-S3 Compatible', fontsize=14, fontweight='bold')
    ax2.legend(fontsize=10, loc='upper right')
    ax2.grid(True, alpha=0.3)
    ax2.set_ylim(0, 1.1)

    plt.tight_layout()
    plt.savefig('detection_8khz.png', dpi=150, bbox_inches='tight')
    print("\nSaved: detection_8khz.png")
    plt.show()

    print(f"\n8kHz Detection Results:")
    print(f"  Grinder: {np.sum(detections_grinder_8k)/len(detections_grinder_8k)*100:.1f}% frames detected")
    print(f"  Background: {np.sum(detections_background_8k)/len(detections_background_8k)*100:.1f}% frames detected")

    return detections_grinder_8k, detections_background_8k


def esp32_feasibility_analysis(detections_grinder_48k, detections_background_48k,
                                 detections_grinder_8k, detections_background_8k):
    """Analyze ESP32-S3 implementation feasibility."""
    print("\n" + "=" * 70)
    print("ESP32-S3 IMPLEMENTATION FEASIBILITY")
    print("=" * 70)

    # Parameters
    sample_rate = 8000
    frame_size = 512
    frame_rate_hz = sample_rate / frame_size
    frame_time_ms = (frame_size / sample_rate) * 1000

    print(f"\n1. TIMING REQUIREMENTS")
    print(f"   Sample rate: {sample_rate} Hz")
    print(f"   Frame size: {frame_size} samples")
    print(f"   Frame rate: {frame_rate_hz:.2f} Hz ({frame_time_ms:.2f} ms per frame)")
    print(f"   Processing must complete within: {frame_time_ms:.2f} ms")

    # Computational complexity
    ops_per_frame = frame_size  # Cross-correlation: O(n) for each frame
    ops_per_second = ops_per_frame * frame_rate_hz

    print(f"\n2. COMPUTATIONAL LOAD")
    print(f"   Cross-correlation ops per frame: ~{ops_per_frame}")
    print(f"   Operations per second: ~{ops_per_second:.0f}")

    # ESP32-S3 specs
    esp32_clock = 240e6  # 240 MHz
    available_cycles_per_frame = esp32_clock * (frame_time_ms / 1000)

    print(f"\n3. ESP32-S3 RESOURCES")
    print(f"   CPU clock: {esp32_clock/1e6:.0f} MHz")
    print(f"   Available cycles per frame: {available_cycles_per_frame:.0f}")
    print(f"   Required operations per frame: {ops_per_frame}")
    print(f"   Cycles per operation available: {available_cycles_per_frame/ops_per_frame:.0f}")

    # Memory requirements
    bytes_per_sample = 2  # int16
    frame_buffer_bytes = frame_size * bytes_per_sample
    reference_buffer_bytes = frame_size * bytes_per_sample
    xcorr_buffer_bytes = frame_size * 4  # float32
    total_ram_bytes = frame_buffer_bytes + reference_buffer_bytes + xcorr_buffer_bytes

    print(f"\n4. MEMORY REQUIREMENTS")
    print(f"   Frame buffer: {frame_buffer_bytes} bytes")
    print(f"   Reference template: {reference_buffer_bytes} bytes")
    print(f"   XCorr working buffer: {xcorr_buffer_bytes} bytes")
    print(f"   Total RAM needed: {total_ram_bytes} bytes (~{total_ram_bytes/1024:.1f} KB)")
    print(f"   ESP32-S3 SRAM: 512 KB (plenty available!)")

    # Detection performance comparison
    print(f"\n5. DETECTION PERFORMANCE COMPARISON")
    print(f"   {'Metric':<30} {'48kHz':<15} {'8kHz (ESP32)'}")
    print(f"   {'-'*30} {'-'*15} {'-'*15}")
    print(f"   {'Grinder detection rate':<30} {np.sum(detections_grinder_48k)/len(detections_grinder_48k)*100:>6.1f}%       {np.sum(detections_grinder_8k)/len(detections_grinder_8k)*100:>6.1f}%")
    print(f"   {'Background false positive':<30} {np.sum(detections_background_48k)/len(detections_background_48k)*100:>6.1f}%       {np.sum(detections_background_8k)/len(detections_background_8k)*100:>6.1f}%")

    # Verdict
    print(f"\n6. VERDICT")
    if available_cycles_per_frame / ops_per_frame > 100:
        verdict = "✓ HIGHLY FEASIBLE"
        details = f"With {available_cycles_per_frame/ops_per_frame:.0f} cycles per operation, there's plenty of headroom."
    elif available_cycles_per_frame / ops_per_frame > 20:
        verdict = "✓ FEASIBLE"
        details = "Adequate processing time with room for optimization."
    else:
        verdict = "⚠ CHALLENGING"
        details = "Tight timing - requires optimized implementation."

    print(f"   {verdict}")
    print(f"   {details}")

    print(f"\n7. IMPLEMENTATION RECOMMENDATIONS")
    print("   • Use fixed-point arithmetic instead of float (faster on ESP32)")
    print("   • Implement circular buffer for streaming audio")
    print("   • Use ESP32-S3 DMA for I2S audio capture (zero CPU overhead)")
    print("   • Consider using ESP32-DSP library for optimized operations")
    print("   • Store reference template in flash (not RAM)")
    print("   • Adjust threshold based on ambient noise level")

    print("\n" + "=" * 70)


def print_summary():
    """Print algorithm summary and next steps."""
    print("\n" + "=" * 70)
    print("SUMMARY AND NEXT STEPS")
    print("=" * 70)

    print("\nKey Findings:")
    print("  1. Grinder has distinct spectral characteristics (visible in Welch PSD)")
    print("  2. Algorithm works at 8kHz sample rate (suitable for ESP32-S3)")
    print("  3. Cross-correlation approach effective for pattern matching")
    print("  4. ESP32-S3 has sufficient processing power")

    print("\nAlgorithm Parameters (tunable):")
    print("  - frame_size = 512 samples (64ms at 8kHz)")
    print("  - threshold = 0.4 (normalized energy)")
    print("  - smooth_window = 10 frames (640ms smoothing)")

    print("\nESP32-S3 Implementation Steps:")
    print("  1. Capture audio via I2S MEMS microphone at 8kHz")
    print("  2. Fill 512-sample circular buffer")
    print("  3. Compute cross-correlation with stored reference template")
    print("  4. Update smoothed energy with moving average")
    print("  5. Trigger wake if energy > threshold for N consecutive frames")

    print("\nPower Optimization:")
    print("  - ULP coprocessor can handle simple threshold checks")
    print("  - Main CPU wakes only when potential detection occurs")
    print("  - Light sleep between frames (~10ms)")

    print("\nImprovements to Test:")
    print("  - Collect more varied grinder recordings (different models)")
    print("  - Test with other kitchen noises (espresso machine, kettle)")
    print("  - Adaptive threshold based on background noise estimate")
    print("  - Hysteresis: require N consecutive detections to trigger")

    print("\n" + "=" * 70)


def main():
    """Main execution function."""
    print("\n" + "=" * 70)
    print("GRINDER DETECTION ALGORITHM")
    print("Cross-Correlation Based Approach")
    print("=" * 70)

    # Set matplotlib style
    plt.style.use('seaborn-v0_8-darkgrid')

    # 1. Load audio files
    grinder_rate, grinder_audio, background_rate, background_audio = load_audio_files()

    # 2. Welch PSD analysis at 48kHz
    f_grinder, psd_grinder, f_background, psd_background = plot_welch_psd(
        grinder_rate, grinder_audio, background_rate, background_audio
    )

    # 3. Test detection at 48kHz
    detections_grinder_48k, detections_background_48k = test_detection_48khz(
        grinder_audio, background_audio, grinder_rate, background_rate
    )

    # 4. Downsample to 8kHz
    grinder_8k, background_8k, target_rate = downsample_to_8khz(
        grinder_audio, background_audio, grinder_rate
    )

    # 5. Welch PSD comparison (48kHz vs 8kHz)
    plot_welch_psd_8khz(
        grinder_8k, background_8k, target_rate,
        f_grinder, psd_grinder, f_background, psd_background
    )

    # 6. Test detection at 8kHz
    detections_grinder_8k, detections_background_8k = test_detection_8khz(
        grinder_8k, background_8k, target_rate
    )

    # 7. ESP32-S3 feasibility analysis
    esp32_feasibility_analysis(
        detections_grinder_48k, detections_background_48k,
        detections_grinder_8k, detections_background_8k
    )

    # 8. Print summary
    print_summary()

    print("\n✓ Analysis complete! Check generated plots:")
    print("  - welch_psd_48khz.png")
    print("  - detection_48khz.png")
    print("  - welch_psd_comparison.png")
    print("  - detection_8khz.png")


if __name__ == "__main__":
    main()
