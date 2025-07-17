#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <usb_midi.h>
#define TOTAL_VOICES 24
int bufferMisses[TOTAL_VOICES] = {0}; 
// Audio setup
AudioPlayQueue      playQueue[TOTAL_VOICES];
//AudioMixer4        mixer1;
//AudioMixer4        mixer2;
AudioMixer4* finalMixerL;
AudioMixer4* finalMixerR;
AudioConnection* finalMixPatchL[(TOTAL_VOICES + 3) / 4];
AudioConnection* finalMixPatchR[(TOTAL_VOICES + 3) / 4];


AudioMixer4* mixers[(TOTAL_VOICES + 3) / 4];  // Create enough mixers
AudioConnection* queuePatchCords[TOTAL_VOICES];
AudioOutputI2S     audioOut;

AudioControlSGTL5000 sgtl5000;

// Sample management
#define SAMPLE_COUNT_PIANO 11
const int sampleNotesPiano[SAMPLE_COUNT_PIANO] = {21, 33, 45, 57, 69, 77, 87, 89, 96, 100, 108};
const char* samplePathPiano = "/sample/channel1/DefaultPiano-%d.raw";

// Audio constants
const float SAMPLE_RATE = 44100.0f;
const float ORIGINAL_RATE = 11025.0f;
//const float RATE_RATIO = SAMPLE_RATE / ORIGINAL_RATE;
//const float RATE_RATIO = 1.0f;  // already resampled

// Piano storage in SDRAM
EXTMEM int16_t* samplesPiano[SAMPLE_COUNT_PIANO];
uint32_t sampleLengthsPiano[SAMPLE_COUNT_PIANO];
// Sample rate conversion (11.025kHz to 44.1kHz)
const int SRC_RATIO = 4;
// Voice management
struct Voice {
  uint8_t note;               // Current MIDI note
  uint8_t velocity;           // MIDI velocity
  bool active;                // Whether the voice is active
  bool playing;               // Currently producing audio
  uint32_t age;               // For least-recently-used logic
  uint8_t channel;            // MIDI channel (1–16)
  uint8_t assignedChannel;    // Logical voice group
  uint8_t baseNote;           // Closest base sample for pitch shift
  float position;             // Current playback position (floating point index into sample)
  float phaseIncrement;       // Playback rate (pitch shift control)
  uint8_t sampleIndex;        // Index to the loaded sample to play
};

Voice voices[TOTAL_VOICES];

struct ChannelVoiceMap {
  uint8_t channel;
  uint8_t start;
  uint8_t count;
};

const ChannelVoiceMap channelMap[] = {
  {1, 0, 5},   // Piano
  {2, 5, 6},   // Guitar
  {3,11, 3},   // Bass
  {4,14, 5},   // Accompaniment
  {10,19, 4},  // Drums
  {255,23, 1}  // Other (FX / announcer / beeps)
};
bool getVoiceRangeForChannel(uint8_t channel, int &start, int &end) {
  switch (channel) {
    case 0:  start = 0;  end = 4;  return true;  // Piano
    case 1:  start = 5;  end = 10; return true;  // Guitar
    case 2:  start = 11; end = 13; return true;  // Accompaniment
    case 3:  start = 14; end = 16; return true;  // Bass
    case 9: start = 17; end = 20; return true;  // Drums
    case 255: start = 21; end = 21; return true; // Announcer
    default: return false;
  }
}

int getVoiceSlotForChannel(byte channel) {
  for (uint8_t i = 0; i < sizeof(channelMap)/sizeof(channelMap[0]); i++) {
    if (channelMap[i].channel == channel || channelMap[i].channel == 255) {
      for (int j = 0; j < channelMap[i].count; j++) {
        int idx = channelMap[i].start + j;
        if (!voices[idx].active) return idx;
      }
      // Steal longest-running voice in this group
      float maxPos = -1.0f;
      int victim = channelMap[i].start;
      for (int j = 0; j < channelMap[i].count; j++) {
        int idx = channelMap[i].start + j;
        if (voices[idx].position > maxPos) {
          maxPos = voices[idx].position;
          victim = idx;
        }
      }
      return victim;
    }
  }
  return -1; // fallback shouldn't happen
}

void loadAllSamples() {
  //for piano
  for (int i = 0; i < SAMPLE_COUNT_PIANO; i++) {
    char path[100];
    snprintf(path, sizeof(path), samplePathPiano, sampleNotesPiano[i]);
    loadSample(path, i);
  }
}
void loadSample(const char* filename, int index) {
  File file = SD.open(filename);
  if (!file) {
    Serial.printf("Failed to open %s\n", filename);
    return;
  }
  
  // Get original sample length
  uint32_t origLength = file.size() / 2; // 16-bit samples
  
  // Calculate resampled length
  uint32_t resampledLength = origLength * SRC_RATIO;
  
  // Allocate memory in SDRAM
  samplesPiano[index] = (int16_t*)extmem_malloc(resampledLength * sizeof(int16_t));
  if (!samplesPiano[index]) {
    Serial.println("Failed to allocate SDRAM for sample!");
    return;
  }
  
  // Read and resample
  int16_t* origData = (int16_t*)malloc(origLength * sizeof(int16_t));
  if (!origData) {
    Serial.println("Failed to allocate memory for temp buffer");
    return;
  }
  
  file.read(origData, origLength * 2);
  file.close();
  
  // Simple linear interpolation resampling
  for (uint32_t i = 0; i < resampledLength; i++) {
    float origPos = (float)i / SRC_RATIO;
    uint32_t origIdx = (uint32_t)origPos;
    float frac = origPos - origIdx;
    
    if (origIdx + 1 < origLength) {
      samplesPiano[index][i] = origData[origIdx] * (1.0f - frac) + origData[origIdx + 1] * frac;
    } else {
      samplesPiano[index][i] = origData[origIdx];
    }
  }
  
  sampleLengthsPiano[index] = resampledLength;
  free(origData);
  
  Serial.printf("Loaded sample %s (%d -> %d samples)\n", filename, origLength, resampledLength);
}

void setupMixers() {
  int numMixers = (TOTAL_VOICES + 3) / 4;

  for (int i = 0; i < numMixers; i++) {
    mixers[i] = new AudioMixer4();
    for (int j = 0; j < 4; j++) {
      mixers[i]->gain(j, 0.25);
    }
  }

  for (int i = 0; i < TOTAL_VOICES; i++) {
    int mixerIndex = i / 4;
    int channel = i % 4;
    queuePatchCords[i] = new AudioConnection(playQueue[i], 0, *mixers[mixerIndex], channel);
  }

  // Final L/R output mixers
  finalMixerL = new AudioMixer4();
  finalMixerR = new AudioMixer4();
  for (int i = 0; i < 4; i++) {
    finalMixerL->gain(i, 0.5);
    finalMixerR->gain(i, 0.5);
  }

  // Connect up to 4 mixers to left and right outputs
  for (int i = 0; i < numMixers; i++) {
    int ch = i % 4;
    if (i < 4) {
      finalMixPatchL[i] = new AudioConnection(*mixers[i], 0, *finalMixerL, ch);
      finalMixPatchR[i] = new AudioConnection(*mixers[i], 0, *finalMixerR, ch);
    } else {
      // Handle overflow (more than 4 mixer groups)
      // You'd need to add additional mixing layers or reduce TOTAL_VOICES
    }
  }

  new AudioConnection(*finalMixerL, 0, audioOut, 0);
  new AudioConnection(*finalMixerR, 0, audioOut, 1);
}
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);
  
  // Initialize audio with more memory
  AudioMemory(80);
  //AudioMemory(256);
  setupMixers();
  // Enable the audio shield
  sgtl5000.enable();
  //sgtl5000.volume(0.7);
  sgtl5000.volume(0.5);
  sgtl5000.lineOutLevel(13);
  
  // Initialize built-in SD card on Teensy 4.1
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("SD card initialization failed!");
    while (1);
  }
  Serial.println("SD card initialized successfully");
  
  // Load all samples to SDRAM
  loadAllSamples();
  
  // Initialize voices
  for (int i = 0; i < TOTAL_VOICES; i++) {
    voices[i].active = false;
    voices[i].position = 0.0f;
  }
  
  // Start MIDI
  usbMIDI.setHandleNoteOn(handleNoteOn);
  usbMIDI.setHandleNoteOff(handleNoteOff);
}

//const uint32_t BUFFER_INTERVAL_MS = 3; 
elapsedMicros voiceTimers[TOTAL_VOICES];  // per-voice microsecond timers
const uint32_t BUFFER_INTERVAL_US = 2900;  // 2.9ms in microseconds
void loop() {
  usbMIDI.read();
  updateVoices();
  
  //if (millis() - lastUpdate >= BUFFER_INTERVAL_MS) {
    playActiveVoices();
    //lastUpdate = millis();
  //}
}

// [Previous loadAllSamples() and loadSample() functions remain the same]

int findClosestSample(int note) {
  int closest = 0;
  int smallestDiff = abs(note - sampleNotesPiano[0]);
  
  for (int i = 1; i < SAMPLE_COUNT_PIANO; i++) {
    int diff = abs(note - sampleNotesPiano[i]);
    if (diff < smallestDiff) {
      smallestDiff = diff;
      closest = i;
    }
  }
  return closest;
}

float calculatePitchRatio(int targetNote, int sampleNote) {
  // Calculate semitone difference
  float semitones = targetNote - sampleNote;
  // Convert semitones to frequency ratio
  return pow(2.0f, semitones / 12.0f);
}

// Return base sample note (root note) for each MIDI channel
uint8_t getBaseNoteForChannel(uint8_t channel) {
  switch (channel) {
    case 1: return 60;  // Piano base note = C4
    case 2: return 64;  // Guitar base = E4
    case 3: return 48;  // Bass = C3
    case 4: return 55;  // Strings = G3
    case 10: return 35; // Drums = Kick
    default: return 60; // Fallback
  }
}

void applyPitchShift(int16_t* buffer, size_t sampleCount, int8_t semitoneShift) {
  // Pitch shift logic (resample, interpolate, etc.)
  // For now, this is a placeholder doing nothing
}
void assignVoice(int voiceIndex, uint8_t note, uint8_t velocity) {
  Voice& v = voices[voiceIndex];

  v.note = note;
  v.velocity = velocity;
  v.active = true;
  v.playing = true;
  v.age = millis();
  v.assignedChannel = 1;  // Only using channel 1 for now

  // Find closest sample and calculate pitch
  v.sampleIndex = findClosestSample(note);
  v.baseNote = sampleNotesPiano[v.sampleIndex];
  v.phaseIncrement = calculatePitchRatio(note, v.baseNote);
  v.position = 0.0f;

  // Queue the first audio block manually
  if (playQueue[voiceIndex].available()) {
    int16_t* buffer = playQueue[voiceIndex].getBuffer();
    if (buffer) {
      memset(buffer, 0, AUDIO_BLOCK_SAMPLES * sizeof(int16_t));

      for (int j = 0; j < AUDIO_BLOCK_SAMPLES; j++) {
        uint32_t index = (uint32_t)v.position;
        float frac = v.position - index;

        if (index + 1 < sampleLengthsPiano[v.sampleIndex]) {
          buffer[j] = (int16_t)(samplesPiano[v.sampleIndex][index] * (1.0f - frac) +
                                samplesPiano[v.sampleIndex][index + 1] * frac);
        } else {
          buffer[j] = samplesPiano[v.sampleIndex][index];
        }

        v.position += v.phaseIncrement;

        if (v.position >= sampleLengthsPiano[v.sampleIndex]) {
          v.active = false;
          break;
        }
      }

      playQueue[voiceIndex].playBuffer();
    }
  } else {
    Serial.printf("Queue not available for voice %d\n", voiceIndex);
    v.playing = false;
  }
}


void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
  int start, end;
  if (!getVoiceRangeForChannel(channel, start, end)) {
    Serial.printf("No voice range for channel %d\n", channel);
    return;
  }

  for (int i = start; i <= end; ++i) {
    if (!voices[i].active) {
      voices[i].assignedChannel = 1;  // Force to channel 1 for now
      assignVoice(i, note, velocity);
      return;
    }
  }

  Serial.printf("No available voices in channel %d range (%d–%d)\n", channel, start, end);
}
void handleNoteOff(byte channel, byte note, byte velocity) {
  for (int i = 0; i < TOTAL_VOICES; i++) {
    if (voices[i].active && voices[i].note == note) {
      voices[i].active = false;
    }
  }
}

void updateVoices() {
  for (int i = 0; i < TOTAL_VOICES; i++) {
    if (voices[i].active && voices[i].position >= sampleLengthsPiano[voices[i].sampleIndex]) {
      voices[i].active = false;
      Serial.printf("Voice %d ended naturally at pos %.1f\n", i, voices[i].position);
    }
  }
}

void playActiveVoices() {
  for (int i = 0; i < TOTAL_VOICES; i++) {
    if (!voices[i].active) continue;

    if (voiceTimers[i] < BUFFER_INTERVAL_US) continue;  // Not yet time for this voice

    if (!playQueue[i].available()) {
      bufferMisses[i]++;
      continue;
    }

    int16_t* buffer = playQueue[i].getBuffer();
    if (!buffer) continue;

    voiceTimers[i] = 0;  // Reset timer for this voice

    for (int j = 0; j < AUDIO_BLOCK_SAMPLES; j++) {
      uint32_t index = (uint32_t)voices[i].position;
      float frac = voices[i].position - index;

      if (index + 1 < sampleLengthsPiano[voices[i].sampleIndex]) {
        buffer[j] = (int16_t)(samplesPiano[voices[i].sampleIndex][index] * (1.0f - frac) +
                              samplesPiano[voices[i].sampleIndex][index + 1] * frac);
      } else {
        buffer[j] = samplesPiano[voices[i].sampleIndex][index];
      }

      voices[i].position += voices[i].phaseIncrement;

      if (voices[i].position >= sampleLengthsPiano[voices[i].sampleIndex]) {
        voices[i].active = false;
        break;
      }
    }

    playQueue[i].playBuffer();
  }
}