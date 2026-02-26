#ifndef AUDIO_H
#define AUDIO_H

void startAudio();
bool processAudioFrame();  // Returns true if grinder detected
void stopAudio();
void recordReference();    // Capture reference snippet from mic, save to SD

#endif
