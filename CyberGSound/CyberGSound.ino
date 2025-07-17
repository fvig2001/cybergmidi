#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <usb_midi.h>
size_t extmem_used = 0;
// Wrapper function
void* extmem_malloc_tracked(size_t size) {
  void* ptr = extmem_malloc(size);
  if (ptr != nullptr) {
    extmem_used += size;
    Serial.printf("EXTMEM malloc: %u bytes (Total used: %u bytes)\n", (unsigned)size, (unsigned)extmem_used);
  } else {
    Serial.printf("EXTMEM malloc FAILED: %u bytes (Total used: %u bytes)\n", (unsigned)size, (unsigned)extmem_used);
  }
  return ptr;
}
#define TOTAL_VOICES 24
int bufferMisses[TOTAL_VOICES] = {0}; 

// Audio setup
AudioPlayQueue      playQueue[TOTAL_VOICES];
AudioMixer4* finalMixerL;
AudioMixer4* finalMixerR;
AudioConnection* finalMixPatchL[(TOTAL_VOICES + 3) / 4];
AudioConnection* finalMixPatchR[(TOTAL_VOICES + 3) / 4];

AudioMixer4* mixers[(TOTAL_VOICES + 3) / 4];
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
const int SRC_RATIO = 4; // 44.1kHz / 11.025kHz

// Piano storage in SDRAM (keeping original 11kHz samples)
EXTMEM int16_t* samplesPiano[SAMPLE_COUNT_PIANO];
uint32_t sampleLengthsPiano[SAMPLE_COUNT_PIANO];

// Voice management
struct Voice {
  uint8_t note;
  uint8_t velocity;
  bool active;
  bool playing;
  uint32_t age;
  uint8_t channel;
  uint8_t assignedChannel;
  uint8_t baseNote;
  float position;             // Position in original sample rate (11kHz)
  float phaseIncrement;       // Playback rate (pitch shift control)
  uint8_t sampleIndex;
  float resampleFrac;         // Fractional position for resampling
  float resampleAccumulator;  // For tracking when to move to next sample
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
    case 0:  start = 0;  end = 4;  return true;
    case 1:  start = 5;  end = 10; return true;
    case 2:  start = 11; end = 13; return true;
    case 3:  start = 14; end = 16; return true;
    case 9: start = 17; end = 20; return true;
    case 255: start = 21; end = 21; return true;
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
  return -1;
}

void loadAllSamples() {
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
  
  uint32_t length = file.size() / 2; // 16-bit samples
  
  // Allocate memory in SDRAM (original 11kHz sample rate)
  samplesPiano[index] = (int16_t*)extmem_malloc_tracked(length * sizeof(int16_t));
  if (!samplesPiano[index]) {
    Serial.println("Failed to allocate SDRAM for sample!");
    return;
  }
  
  file.read(samplesPiano[index], length * 2);
  file.close();
  sampleLengthsPiano[index] = length;
  
  Serial.printf("Loaded sample %s (%d samples)\n", filename, length);
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

  finalMixerL = new AudioMixer4();
  finalMixerR = new AudioMixer4();
  for (int i = 0; i < 4; i++) {
    finalMixerL->gain(i, 0.5);
    finalMixerR->gain(i, 0.5);
  }

  for (int i = 0; i < numMixers; i++) {
    int ch = i % 4;
    if (i < 4) {
      finalMixPatchL[i] = new AudioConnection(*mixers[i], 0, *finalMixerL, ch);
      finalMixPatchR[i] = new AudioConnection(*mixers[i], 0, *finalMixerR, ch);
    }
  }

  new AudioConnection(*finalMixerL, 0, audioOut, 0);
  new AudioConnection(*finalMixerR, 0, audioOut, 1);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);
  
  AudioMemory(80);
  setupMixers();
  sgtl5000.enable();
  sgtl5000.volume(0.5);
  sgtl5000.lineOutLevel(13);
  
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("SD card initialization failed!");
    while (1);
  }
  Serial.println("SD card initialized successfully");
  
  loadAllSamples();
  
  for (int i = 0; i < TOTAL_VOICES; i++) {
    voices[i].active = false;
    voices[i].position = 0.0f;
    voices[i].resampleFrac = 0.0f;
    voices[i].resampleAccumulator = 0.0f;
  }
  
  usbMIDI.setHandleNoteOn(handleNoteOn);
  usbMIDI.setHandleNoteOff(handleNoteOff);
}

elapsedMicros voiceTimers[TOTAL_VOICES];
const uint32_t BUFFER_INTERVAL_US = 2900;

void loop() {
  usbMIDI.read();
  updateVoices();
  playActiveVoices();
}

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
  return pow(2.0f, (targetNote - sampleNote) / 12.0f);
}

void assignVoice(int voiceIndex, uint8_t note, uint8_t velocity) {
  Voice& v = voices[voiceIndex];

  v.note = note;
  v.velocity = velocity;
  v.active = true;
  v.playing = true;
  v.age = millis();
  v.assignedChannel = 1;

  v.sampleIndex = findClosestSample(note);
  v.baseNote = sampleNotesPiano[v.sampleIndex];
  v.phaseIncrement = calculatePitchRatio(note, v.baseNote);
  v.position = 0.0f;
  v.resampleFrac = 0.0f;
  v.resampleAccumulator = 0.0f;

  // Queue the first audio block
  if (playQueue[voiceIndex].available()) {
    int16_t* buffer = playQueue[voiceIndex].getBuffer();
    if (buffer) {
      memset(buffer, 0, AUDIO_BLOCK_SAMPLES * sizeof(int16_t));
      
      // Real-time resampling from 11kHz to 44kHz
      for (int j = 0; j < AUDIO_BLOCK_SAMPLES; j++) {
        // Linear interpolation for resampling
        uint32_t index = (uint32_t)v.position;
        float frac = v.position - index;
        
        if (index + 1 < sampleLengthsPiano[v.sampleIndex]) {
          buffer[j] = (int16_t)(samplesPiano[v.sampleIndex][index] * (1.0f - frac) +
                               samplesPiano[v.sampleIndex][index + 1] * frac);
        } else {
          buffer[j] = samplesPiano[v.sampleIndex][index];
        }
        
        // Advance position at 11kHz rate (with pitch adjustment)
        v.position += v.phaseIncrement / SRC_RATIO;
        
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
      voices[i].assignedChannel = 1;
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
    }
  }
}

void playActiveVoices() {
  for (int i = 0; i < TOTAL_VOICES; i++) {
    if (!voices[i].active) continue;

    if (voiceTimers[i] < BUFFER_INTERVAL_US) continue;

    if (!playQueue[i].available()) {
      bufferMisses[i]++;
      continue;
    }

    int16_t* buffer = playQueue[i].getBuffer();
    if (!buffer) continue;

    voiceTimers[i] = 0;

    for (int j = 0; j < AUDIO_BLOCK_SAMPLES; j++) {
      // Linear interpolation for resampling
      uint32_t index = (uint32_t)voices[i].position;
      float frac = voices[i].position - index;
      
      if (index + 1 < sampleLengthsPiano[voices[i].sampleIndex]) {
        buffer[j] = (int16_t)(samplesPiano[voices[i].sampleIndex][index] * (1.0f - frac) +
                             samplesPiano[voices[i].sampleIndex][index + 1] * frac);
      } else {
        buffer[j] = samplesPiano[voices[i].sampleIndex][index];
      }
      
      // Advance position at 11kHz rate (with pitch adjustment)
      voices[i].position += voices[i].phaseIncrement / SRC_RATIO;
      
      if (voices[i].position >= sampleLengthsPiano[voices[i].sampleIndex]) {
        voices[i].active = false;
        break;
      }
    }

    playQueue[i].playBuffer();
  }
}
