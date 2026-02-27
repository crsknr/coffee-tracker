#ifndef AUDIO_H
#define AUDIO_H

void startAudio();
bool processAudioFrame();  // Returns true if grinder detected (sustained above threshold)
void stopAudio();
void recordReference();    // Capture reference snippet from mic, save to SD

// Runtime threshold control (adjustable from audio diagnostics page)
float getSmoothedEnergy();
float getXcorrThreshold();
void  adjustXcorrThreshold(float delta);  // clamped to [0.05, 0.95]

#endif
