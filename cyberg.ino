#define DEBUG false
#define DEBUGNOTEPRINT false
#include <usb_midi.h>
#include <MIDI.h>
#include <vector>
#include <SD.h>
#include <Encoder.h>
#include "stateconfig.h"
#include "omniChordBacking.h"
#include "lookups.h"
#include <USBHost_t36.h>
#include <deque>
USBHost myusb;
USBHub hub1(myusb);       // Optional, in case there's a USB hub
MIDIDevice midi1(myusb);  // This will represent your USB MIDI device
//MIDIDevice_BigBuffer midi1(myusb);

extern "C" char* sbrk(int incr);                     // Get current heap end
bool debug2 = true;                                  //for testing
std::vector<SequencerNote> CyberGSequencerNotes;     //queue of actual notes that go to the device
std::vector<SequencerNote> SequencerNotes;           //queue of guitar notes
std::vector<SequencerNote> StaggeredSequencerNotes;  //queue of staggered guitar notes

//patterns in terms of Sequencer note

std::deque<int> lastCommandBTClassicSent;
std::deque<int> lastCommandBLEMidiSent;
std::vector<SequencerNote> SequencerPatternA;
std::vector<SequencerNote> SequencerPatternB;
std::vector<SequencerNote> SequencerPatternC;
volatile int encoderPosition = 0;
volatile uint32_t lastMoveTime = 0;
volatile int lastDirection = 0;

void onEncoderA() {
  uint32_t now = millis();

  bool a = digitalRead(VOLUME_LSB);  // CLK
  bool b = digitalRead(VOLUME_MSB);  // DT

  if (a) {  // Only react on A rising
    int dir = (b == 0) ? +1 : -1;

    // Ignore direction reversals that happen too soon (phantom step)
    if ((dir != lastDirection) && (now - lastMoveTime < 250)) {
      return;
    }

    lastDirection = dir;
    lastMoveTime = now;
    encoderPosition += dir;
  }
}

std::vector<SequencerNote>* getGuitarPattern(uint8_t pattern) {
  switch (pattern) {
    case 0:
      return &SequencerPatternA;
    case 1:
      return &SequencerPatternB;
    default:
      return &SequencerPatternC;
  }
}

std::vector<SequencerNote>* getBassPattern(uint8_t pattern) {
  switch (pattern) {
    default:
      return &BassSequencerPattern;
  }
}

std::vector<SequencerNote>* getAccompanimentPattern(uint8_t pattern) {
  switch (pattern) {
    default:
      return &AccompanimentSequencerPattern;
  }
}

std::vector<SequencerNote>* getDrumPattern(uint8_t pattern) {
  switch (pattern) {
    case DRUM_INTRO_ID:
      return &DrumIntroSequencer;
    case DRUM_LOOP_ID:
      return &DrumLoopSequencer;
    case DRUM_LOOPHALF_ID:
      return &DrumLoopHalfBarSequencer;
    case DRUM_FILL_ID:
      return &DrumFillSequencer;
    default:
      return &DrumEndSequencer;
  }
}

void addEntryToPattern(std::vector<SequencerNote>* pattern, uint8_t note, uint16_t offset, uint16_t holdTime, uint8_t velocity, int8_t relativeOctave, uint8_t channel) {
  if (pattern == NULL && debug) {
    Serial.printf("Error! Pattern passed is null!\n");
  }
  SequencerNote sn;
  sn.note = note;
  sn.offset = offset;
  sn.holdTime = holdTime;
  sn.velocity = velocity;
  sn.channel = channel;
  sn.relativeOctave = relativeOctave;
  pattern->push_back(sn);
}

void sendPatternData(std::vector<SequencerNote>* pattern, bool isSerialOut = true) {
  if (pattern == NULL && debug) {
    Serial.printf("Error! Pattern passed is null!\n");
  }
  SequencerNote sn;

  char buffer[48];
  snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", pattern->size());
  Serial.write(buffer);
  for (uint8_t i = 0; i < pattern->size(); i++) {
    sn = pattern->at(i);
    snprintf(buffer, sizeof(buffer), "OK00,%d,%d,%d,%d,%d,%d\r\n", sn.note, sn.offset, sn.holdTime, sn.velocity, sn.channel, sn.relativeOctave);
    if (!isSerialOut && debug) {
      Serial.printf("%s", buffer);
    } else {
      Serial.write(buffer);
    }
  }
}

unsigned int computeTickInterval(int bpm) {
  // 60,000,000 microseconds per minute / (BPM * 192 ticks per quarter note)
  return ((60000 / bpm) / (QUARTERNOTETICKS * 1.0)) * 1000;
}

// --- STATE TRACKING ---
bool b2Ignored = false;
bool prevNoteOffState = LOW;
bool prevCCState = LOW;
std::vector<uint8_t> lastGuitarNotes;
std::vector<uint8_t> lastGuitarNotesButtons;
std::vector<noteShift> lastOmniNotes;
#define MAX_BUFFER_SIZE 150

char dataBuffer1[MAX_BUFFER_SIZE + 1];  // +1 for null terminator
char dataBuffer2[MAX_BUFFER_SIZE + 1];  // +1 for null terminator
char dataBuffer3[MAX_BUFFER_SIZE + 1];  // +1 for null terminator
char dataBuffer4[MAX_BUFFER_SIZE + 1];  // +1 for null terminator
char dataBuffer5[MAX_BUFFER_SIZE + 1];  // +1 for null terminator
char dataBuffer6[MAX_BUFFER_SIZE + 1];  // +1 for null terminator
uint8_t bufferLen1 = 0;
uint8_t bufferLen2 = 0;
uint8_t bufferLen3 = 0;
uint8_t bufferLen4 = 0;
uint8_t bufferLen5 = 0;
uint8_t bufferLen6 = 0;
bool lastSimple = false;
TranspositionDetector detector;
Chord lastPressedChord;
std::vector<uint8_t> lastPressedGuitarChord;

bool hexStringAndMatches(const char* input, const char* target, int hexLen) {
  // hexLen = number of hex characters (should be even)
  if (hexLen % 2 != 0) return false;

  for (int i = 0; i < hexLen; i += 2) {
    char byteStrInput[3] = { input[i], input[i + 1], '\0' };
    char byteStrTarget[3] = { target[i], target[i + 1], '\0' };

    uint8_t byteInput = strtol(byteStrInput, NULL, 16);
    uint8_t byteTarget = strtol(byteStrTarget, NULL, 16);

    if ((byteInput & byteTarget) != byteTarget) {
      return false;
    }
  }

  return true;
}

//[preset][buttonpressed] //combines patterns for paddle and piano mode
std::vector<std::vector<AssignedPattern>> assignedFretPatternsByPreset;

AssignedPattern getActualAssignedChord(uint8_t b) {
  if (isSimpleChordMode[preset]) {
    return assignedFretPatternsByPreset[preset][(b / NECK_COLUMNS) * NECK_COLUMNS];
  } else {
    return assignedFretPatternsByPreset[preset][b];
  }
}

void preparePatterns() {
  SequencerPatternA.clear();
  SequencerPatternB.clear();
  SequencerPatternC.clear();
  BassSequencerPattern.clear();
  AccompanimentSequencerPattern.clear();
  SequencerNote p;
  //pattern 1
  p.note = 1;
  p.offset = 1;
  p.velocity = MAX_VELOCITY;
  p.holdTime = 48;
  SequencerPatternA.push_back(p);
  p.note = 2;
  p.offset = 25;
  p.velocity = MAX_VELOCITY;
  p.holdTime = 24;
  SequencerPatternA.push_back(p);
  p.note = 3;
  p.offset = 37;
  p.velocity = MAX_VELOCITY;
  p.holdTime = 36;
  SequencerPatternA.push_back(p);

  //pattern 2
  p.note = 3;
  p.offset = 1;
  p.velocity = MAX_VELOCITY;
  p.holdTime = 48;
  SequencerPatternB.push_back(p);
  p.note = 2;
  p.offset = 25;
  p.velocity = MAX_VELOCITY;
  p.holdTime = 24;
  SequencerPatternB.push_back(p);
  p.note = 1;
  p.offset = 37;
  p.velocity = MAX_VELOCITY;
  p.holdTime = 36;
  SequencerPatternB.push_back(p);

  //pattern 3
  p.note = 2;
  p.offset = 1;
  p.velocity = MAX_VELOCITY;
  p.holdTime = 48;
  SequencerPatternC.push_back(p);
  p.note = 3;
  p.offset = 25;
  p.velocity = MAX_VELOCITY;
  p.holdTime = 24;
  SequencerPatternC.push_back(p);
  p.note = 1;
  p.offset = 37;
  p.velocity = MAX_VELOCITY;
  p.holdTime = 36;
  SequencerPatternC.push_back(p);
}

enum OmniChordModeType {
  OmniChordOffType = 0,    //no omnichord
  OmniChordOffGuitarType,  //no omnichord but uses guitar chords instead
  OmniChordStandardType,   //basic omnichord mode where paddles adjust octave and it uses piano chords
  OmniChordGuitarType,     //requires paddle
};

std::vector<std::vector<neckAssignment>> neckAssignments;  //[preset][neck assignment 0-27]

//current presets:
// 1 standard
// 2 standard but different 3rd column sus4
// 3 standard but different 3rd column major 6th
// 4 simple mode
// 5 omnichord mode
// 6 omnichord mode but hybrid mode
void prepareNeck() {
  Note myN;
  neckAssignments.clear();
  std::vector<neckAssignment> tempNeck;
  //preset 1
  tempNeck.clear();
  for (int i = 0; i < NECK_ROWS; i++) {

    switch (i) {
      case 0:
        myN = C_NOTE;  // C
        break;
      case 1:
        myN = D_NOTE;
        break;
      case 2:
        myN = E_NOTE;
        break;
      case 3:
        myN = F_NOTE;
        break;
      case 4:
        myN = G_NOTE;
        break;
      case 5:
        myN = A_NOTE;
        break;
      case 6:
        myN = B_NOTE;
        break;
      default:
        myN = C_NOTE;
    }
    for (uint8_t j = 0; j < NECK_COLUMNS; j++) {
      neckAssignment n;
      switch (j) {
        case 0:
          n.chordType = majorChordType;
          break;
        case 1:
          n.chordType = minorChordType;
          break;
        default:
          n.chordType = dominant7thChordType;
      }
      n.key = myN;
      tempNeck.push_back(n);
    }
  }
  // for (uint8_t i = 0; i < tempNeck.size(); i++)
  // {
  //     Serial.printf("Num %d, note %d, chord %d\n", i, (int) tempNeck[i].key, (int) tempNeck[i].chordType);

  // }
  neckAssignments.push_back(tempNeck);
  //preset 2
  tempNeck.clear();
  for (int i = 0; i < NECK_ROWS; i++) {
    neckAssignment n;
    switch (i) {
      case 0:
        n.key = C_NOTE;  // C
        break;
      case 1:
        n.key = D_NOTE;
        break;
      case 2:
        n.key = E_NOTE;
        break;
      case 3:
        n.key = F_NOTE;
        break;
      case 4:
        n.key = G_NOTE;
        break;
      case 5:
        n.key = A_NOTE;
        break;
      case 6:
        n.key = B_NOTE;
        break;
      default:
        n.key = C_NOTE;
    }
    for (uint8_t j = 0; j < NECK_COLUMNS; j++) {
      switch (j) {
        case 0:
          n.chordType = majorChordType;
          break;
        case 1:
          n.chordType = minorChordType;
          break;
        default:
          n.chordType = suspended4ChordType;
      }
      tempNeck.push_back(n);
    }
  }
  neckAssignments.push_back(tempNeck);
  //preset 3
  tempNeck.clear();
  for (int i = 0; i < NECK_ROWS; i++) {
    neckAssignment n;
    switch (i) {
      case 0:
        n.key = C_NOTE;  // C
        break;
      case 1:
        n.key = D_NOTE;
        break;
      case 2:
        n.key = E_NOTE;
        break;
      case 3:
        n.key = F_NOTE;
        break;
      case 4:
        n.key = G_NOTE;
        break;
      case 5:
        n.key = A_NOTE;
        break;
      case 6:
        n.key = B_NOTE;
        break;
      default:
        n.key = C_NOTE;
    }
    for (uint8_t j = 0; j < NECK_COLUMNS; j++) {
      switch (j) {
        case 0:
          n.chordType = majorChordType;
          break;
        case 1:
          n.chordType = minorChordType;
          break;
        default:
          n.chordType = major6thChordType;
      }
      tempNeck.push_back(n);
    }
  }
  neckAssignments.push_back(tempNeck);
  //preset 4
  tempNeck.clear();
  for (int i = 0; i < NECK_ROWS; i++) {
    neckAssignment n;
    n.chordType = majorChordType;
    switch (i) {
      case 0:
        n.key = C_NOTE;  // C

        break;
      case 1:
        n.key = D_NOTE;
        n.chordType = minorChordType;
        break;
      case 2:
        n.key = E_NOTE;
        n.chordType = minorChordType;
        break;
      case 3:
        n.key = F_NOTE;
        break;
      case 4:
        n.key = G_NOTE;
        break;
      case 5:
        n.key = A_NOTE;
        n.chordType = minorChordType;
        break;
      case 6:
        n.key = B_NOTE;
        n.chordType = diminishedChordType;
        break;
      case 8:
        n.key = D_NOTE;
        n.chordType = minorChordType;
        break;
      default:
        n.key = C_NOTE;
    }
    for (uint8_t j = 0; j < NECK_COLUMNS; j++) {
      tempNeck.push_back(n);
    }
  }
  neckAssignments.push_back(tempNeck);
  //preset 5
  tempNeck.clear();
  for (int i = 0; i < NECK_ROWS; i++) {

    switch (i) {
      case 0:
        myN = C_NOTE;  // C
        break;
      case 1:
        myN = D_NOTE;
        break;
      case 2:
        myN = E_NOTE;
        break;
      case 3:
        myN = F_NOTE;
        break;
      case 4:
        myN = G_NOTE;
        break;
      case 5:
        myN = A_NOTE;
        break;
      case 6:
        myN = B_NOTE;
        break;
      default:
        myN = C_NOTE;
    }
    for (uint8_t j = 0; j < NECK_COLUMNS; j++) {
      neckAssignment n;
      switch (j) {
        case 0:
          n.chordType = majorChordType;
          break;
        case 1:
          n.chordType = minorChordType;
          break;
        default:
          n.chordType = dominant7thChordType;
      }
      n.key = myN;
      tempNeck.push_back(n);
    }
  }
  neckAssignments.push_back(tempNeck);

  //preset 6
  tempNeck.clear();
  for (int i = 0; i < NECK_ROWS; i++) {

    switch (i) {
      case 0:
        myN = C_NOTE;  // C
        break;
      case 1:
        myN = D_NOTE;
        break;
      case 2:
        myN = E_NOTE;
        break;
      case 3:
        myN = F_NOTE;
        break;
      case 4:
        myN = G_NOTE;
        break;
      case 5:
        myN = A_NOTE;
        break;
      case 6:
        myN = B_NOTE;
        break;
      default:
        myN = C_NOTE;
    }
    for (uint8_t j = 0; j < NECK_COLUMNS; j++) {
      neckAssignment n;
      switch (j) {
        case 0:
          n.chordType = majorChordType;
          break;
        case 1:
          n.chordType = minorChordType;
          break;
        default:
          n.chordType = dominant7thChordType;
      }
      n.key = myN;
      tempNeck.push_back(n);
    }
  }

  neckAssignments.push_back(tempNeck);

  //Serial.printf("neckAssignments size = %d\n", neckAssignments.size());
}

std::vector<uint8_t> getGuitarChordNotesFromNeck(uint8_t row, uint8_t column, uint8_t usePreset) {
  //given preset get neck Assignment:
  if (isSimpleChordMode[preset]) {
    column = 0;
  }

  neckAssignment n = neckAssignments[usePreset][row * NECK_COLUMNS + column];
  /*
  if (debug)
  {
    Serial.printf("getGuitarChordNotesFromNeck %d %d %d root %d chord %d\n", usePreset, row, column, (int) n.key, n.chordType);
  }
  */
  //Serial.printf("GT row %d col %d preset %d\n",row, column, usePreset);
  //Serial.printf("ChordType GT to chord = %d\n",(int)n.chordType);
  //Serial.printf("ChordType GT to key = %d\n",(int)n.key);
  //now based on neck assignment, we will need to determine enum value then return the value
  //return allChordsGuitar[(uint8_t)n.chordType][(uint8_t)n.key];
  return GetAllChordsGuitar((uint8_t)n.chordType, (uint8_t)n.key);
}

Chord getKeyboardChordNotesFromNeck(uint8_t row, uint8_t column, uint8_t usePreset) {
  //given preset get neck Assignment:
  //Serial.printf("KB row %d col %d preset %d index %d\n",row, column, usePreset, row * NECK_COLUMNS + column);
  if (isSimpleChordMode[preset]) {
    column = 0;
  }
  neckAssignment n = neckAssignments[usePreset][row * NECK_COLUMNS + column];
  /*
  if (debug)
  {
    //Serial.printf("getKeyboardChordNotesFromNeck %d %d %d root %d chord %d\n", usePreset, row, column, (int) n.key, n.chordType);
  }
  */
  //Serial.printf("ChordType KB to row = %d preset = %d\n",(int)n.chordType, usePreset);
  //now based on neck assignment, we will need to determine enum value then return the value
  //return chords[(uint8_t)n.chordType];
  return GetPianoChords((uint8_t)n.chordType);
}

bool isStaggeredNotes() {
  return simpleChordSetting[preset] == ManualStrum;
}

void prepareConfig() {
  presetBPM.clear();
  omniChordModeGuitar.clear();
  enableAllNotesOnChords.clear();
  isSimpleChordMode.clear();
  enableButtonMidi.clear();
  strumSeparation.clear();

  muteWhenLetGo.clear();
  ignoreSameChord.clear();
  chordHold.clear();
  guitarTranspose.clear();
  omniKBTransposeOffset.clear();
  alternateDirection.clear();
  simpleChordSetting.clear();
  useToggleSustain.clear();
  drumsEnabled.clear();
  bassEnabled.clear();
  accompanimentEnabled.clear();
  properOmniChord5ths.clear();
  //bassNoteOffsets.clear();
  //accompanimentNoteOffsets.clear();
  //guitarNoteOffsets.clear();

  //Serial.printf("prepareConfig! Enter\n");
  uint8_t temp8;
  uint16_t temp16;
  for (uint8_t i = 0; i < MAX_PRESET; i++) {
    //set default values first:
    temp16 = 128 + i * 12;
    presetBPM.push_back(temp16);
    if (i == 4) {
      omniChordModeGuitar.push_back(OmniChordStandardType);
    } else if (i == 5) {
      omniChordModeGuitar.push_back(OmniChordGuitarType);
    } else {
      omniChordModeGuitar.push_back(0);
    }
    temp8 = 1;
    strumSeparation.push_back(temp8);
    muteSeparation.push_back(3);

    muteWhenLetGo.push_back(false);
    ignoreSameChord.push_back(true);
    ignoreSameChord[4] = false;
    chordHold.push_back(true);
    guitarTranspose.push_back(0);
    omniKBTransposeOffset.push_back(0);
    alternateDirection.push_back(false);
    useToggleSustain.push_back(false);
    simpleChordSetting.push_back(0);
    enableButtonMidi.push_back(false);
    enableAllNotesOnChords.push_back(false);
    isSimpleChordMode.push_back(false);
    drumsEnabled.push_back(true);
    bassEnabled.push_back(true);
    accompanimentEnabled.push_back(true);
    properOmniChord5ths.push_back(true);
    AccompanimentPatternID = 0;
    for (int i = 0; i < DRUM_END_ID + 1; i++) {
      DrumPatternID[i] = 0;
    }
    BassPatternID = 0;
    GuitarPatternID0 = 0;
    GuitarPatternID1 = 0;
    GuitarPatternID2 = 0;
  }
}

void prepareChords() {
  //todo erase samples completely as needed
  std::vector<AssignedPattern> assignedFretPatterns;
  assignedFretPatternsByPreset.clear();
  uint8_t rootNotes[] = { 48, 50, 52, 53, 55, 57, 59 };

  // Populate the assignedFretPatterns with the chords for each root note
  for (int x = 0; x < MAX_PRESET; x++) {
    assignedFretPatterns.clear();
    for (int i = 0; i < NECK_ROWS; i++) {
      for (int j = 0; j < NECK_COLUMNS; j++) {
        assignedFretPatterns.push_back(AssignedPattern(simpleChordSetting[x], getKeyboardChordNotesFromNeck(i, j, x), getGuitarChordNotesFromNeck(i, j, x), rootNotes[i], 0));
      }
    }
    lastPressedChord = assignedFretPatterns[0].assignedChord;
    //lastPressedGuitarChord = allChordsGuitar[0][0];  // c major default strum
    lastPressedGuitarChord = GetAllChordsGuitar(0, 0);
    //Serial.printf("assignedFretPatterns size is %d\n", assignedFretPatterns.size());
    assignedFretPatternsByPreset.push_back(assignedFretPatterns);
  }
  //Serial.printf("assignedFretPatternsByPreset size is %d\n", assignedFretPatternsByPreset.size());
}

void printChords() {
  for (int i = 0; i <= clusterChordType; i++) {
    GetPianoChords(i).printChordInfo();
  }
}

// Interrupt Service Routine: set flag to send clock
void clockISR() {
  sendClockTick = true;
}

void tickISR() {
  sendClockTick = true;
}

void setNewBPM(int newBPM) {
  //deviceBPM = newBPM;
  unsigned int newInterval = computeTickInterval(newBPM);
  tickTimer.end();
  tickTimer.begin(tickISR, newInterval);  // restart timer with new interval
}
void sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity = DEFAULT_VELOCITY) {
  if (channel <= GUITAR_CHANNEL) {
    lastNotePressTime = millis();
    velocity += volumeOffset;
    if (velocity > 127) {
      velocity = 127;
    }
  }
  if (!isKeyboard) {
    if (channel == GUITAR_CHANNEL) {
      sendProgram(channel, PROGRAM_ACOUSTIC_GUITAR);
    }
  }
  usbMIDI.sendNoteOn(note, velocity, channel);
  if (BLEEnabled) {
    char data[] = { (char)(NOTE_ON | channel), note, velocity };
    Serial7.write(data, 3);
  }
  //todo add support for playing sound
  if (debug && DEBUGNOTEPRINT) {
    Serial.printf("Note ON: ch=%d note=%d vel=%d\n", channel, note, velocity);
  }
}

void sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity = 0) {
  usbMIDI.sendNoteOff(note, velocity, channel);
  //todo add support for playing sound
  if (BLEEnabled) {
    char data[] = { (char)(NOTE_OFF | channel), note, velocity };
    Serial7.write(data, 3);
  }
  if (debug && DEBUGNOTEPRINT) {
    Serial.printf("Note OFF: ch=%d note=%d vel=%d\n", channel, note, velocity);
  }
}

void sendNoteRealOn(uint8_t channel, uint8_t note, uint8_t velocity = DEFAULT_VELOCITY) {
  char data[] = { (char)(0x90 | channel), note, velocity };
  Serial2.write(data, sizeof(data));
}

void sendNoteRealOff(uint8_t channel, uint8_t note, uint8_t velocity = 0) {
  char data[] = { (char)(0x80 | channel), note, velocity };
  Serial2.write(data, sizeof(data));
}

void noteAllOff(int channel = -1) {
  for (uint8_t n = 0; n < MAX_NOTE; n++) {
    switch (channel) {
      case GUITAR_CHANNEL:
        sendNoteOff(GUITAR_CHANNEL, n);
        break;
      case KEYBOARD_CHANNEL:
        sendNoteOff(KEYBOARD_CHANNEL, n);
        break;
      case GUITAR_BUTTON_CHANNEL:
        sendNoteOff(GUITAR_BUTTON_CHANNEL, n);
        break;
      case DRUM_CHANNEL:
        sendNoteOff(DRUM_CHANNEL, n);
        break;
      case BASS_CHANNEL:
        sendNoteOff(BASS_CHANNEL, n);
        break;
      case ACCOMPANIMENT_CHANNEL:
        sendNoteOff(ACCOMPANIMENT_CHANNEL, n);
        break;
      default:
        sendNoteOff(GUITAR_CHANNEL, n);
        sendNoteOff(KEYBOARD_CHANNEL, n);
        sendNoteOff(GUITAR_BUTTON_CHANNEL, n);
        sendNoteOff(DRUM_CHANNEL, n);
        sendNoteOff(BASS_CHANNEL, n);
        sendNoteOff(ACCOMPANIMENT_CHANNEL, n);
        //omniChordNewNotes.clear();
        //lastOmniNotes.clear();
        drumState = DrumStopped;
        bassStart = false;
        accompanimentStart = false;
        DrumSequencerNotes.clear();
        BassSequencerNotes.clear();
        AccompanimentSequencerNotes.clear();
        //StaggeredSequencerNotes.clear();
        SequencerNotes.clear();
    }
  }
}
void notifyDevice() {
}
//generic preset Change handler when vectors aren't enough
void presetChanged() {
  isStrumUp = false;
  isSustain = false;
  isSustainPressed = false;
  //if (assignedFretPatternsByPreset[preset][msg.note].getPatternAssigned() == ManualStrum)
  setNewBPM(presetBPM[preset]);
  omniChordNewNotes.clear();
  detector.transposeReset();
  lastOmniNotes.clear();
  StaggeredSequencerNotes.clear();
  //if all sounds must be stopped
  if (stopSoundsOnPresetChange) {
    noteAllOff();
  }

  neckButtonPressed = -1;  // set last button is null
  lastValidNeckButtonPressed = -1;
  lastValidNeckButtonPressed2 = -1;
  notifyDevice();
}

bool loadPatternFiles(bool loadFromBackup = false) {
  if (!loadPatternRelatedConfig()) {
    return false;
  };
  if (backingState != 0) {
    if (debug) {
      Serial.printf("Setting Backing State and files to %d\n", backingState);
    }
    handleOmnichordBackingChange(backingState);
  }
  if (!savePatternRelatedConfig()) {
    return false;
  };
  String myFile = "pattern.ini";
  if (loadFromBackup) {
    myFile = "patternBackup.ini";
  }
  File f = SD.open(myFile.c_str(), FILE_READ);
  if (!f) {
    if (debug)
      Serial.println("Error loading pattern config!");
    return false;
  }
  if (!loadFromBackup) {
    SequencerPatternA.clear();
    SequencerPatternB.clear();
    SequencerPatternC.clear();
  }
  BassSequencerPattern.clear();
  AccompanimentSequencerPattern.clear();

  DrumLoopSequencer.clear();
  DrumLoopHalfBarSequencer.clear();
  DrumFillSequencer.clear();
  DrumIntroSequencer.clear();
  DrumEndSequencer.clear();

  std::vector<SequencerNote>* currentPattern = nullptr;
  uint8_t currentChannel = 0;
  char line[64];

  while (f.available()) {
    memset(line, 0, sizeof(line));
    f.readBytesUntil('\n', line, sizeof(line));

    // Trim newline and whitespace
    String trimmed = String(line);
    trimmed.trim();

    if (trimmed == "DRUM_INTRO_START") {
      currentPattern = &DrumIntroSequencer;
      currentChannel = DRUM_CHANNEL;
    } else if (trimmed == "DRUM_LOOP_START") {
      currentPattern = &DrumLoopSequencer;
      currentChannel = DRUM_CHANNEL;
    } else if (trimmed == "DRUM_HALFLOOP_START") {
      currentPattern = &DrumLoopHalfBarSequencer;
      currentChannel = DRUM_CHANNEL;
    } else if (trimmed == "DRUM_FILL_START") {
      currentPattern = &DrumFillSequencer;
      currentChannel = DRUM_CHANNEL;
    } else if (trimmed == "DRUM_END_START") {
      currentPattern = &DrumEndSequencer;
      currentChannel = DRUM_CHANNEL;

    } else if (trimmed == "GUITAR_A_START") {
      currentPattern = &SequencerPatternA;
      currentChannel = GUITAR_CHANNEL;
    } else if (trimmed == "GUITAR_B_START") {
      currentPattern = &SequencerPatternB;
      currentChannel = GUITAR_CHANNEL;
    } else if (trimmed == "GUITAR_C_START") {
      currentPattern = &SequencerPatternC;
      currentChannel = GUITAR_CHANNEL;
    } else if (trimmed == "BASS_START") {
      currentPattern = &BassSequencerPattern;
      currentChannel = BASS_CHANNEL;
    } else if (trimmed.endsWith("_END")) {
      currentPattern = nullptr;
    } else if (trimmed == "ACCOMPANIMENT_START") {
      currentPattern = &AccompanimentSequencerPattern;
      currentChannel = ACCOMPANIMENT_CHANNEL;
    } else if (trimmed.endsWith("_END")) {
      currentPattern = nullptr;
    } else if (currentPattern) {
      // Parse note line: note,holdTime,offset,velocity
      SequencerNote note;
      int n, h, o, v, r;
      if (sscanf(trimmed.c_str(), "%d,%d,%d,%d,%d", &n, &h, &o, &v, &r) == 5) {
        note.note = n;
        note.holdTime = h;
        note.offset = o;
        note.velocity = v;
        note.channel = currentChannel;
        note.relativeOctave = r;
        currentPattern->push_back(note);
      }
    }
  }

  f.close();
  return true;
}

void setup() {
  //strum = 0;
  pinMode(VOLUME_LSB, INPUT_PULLUP);                                       // CLK
  pinMode(VOLUME_MSB, INPUT_PULLUP);                                       // DT
  attachInterrupt(digitalPinToInterrupt(VOLUME_LSB), onEncoderA, RISING);  // Only track rising edge
  pinMode(NOTE_OFF_PIN, INPUT_PULLUP);
  pinMode(START_TRIGGER_PIN, INPUT_PULLUP);
  pinMode(BUTTON_1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_2_PIN, INPUT_PULLUP);
  pinMode(BUTTON_3_PIN, INPUT_PULLUP);
  //pinMode(BT_ON_PIN, OUTPUT);     // Set pin 19 as output
  //pinMode(BT_STATUS_PIN, INPUT);  // Set pin 18 as output

  //digitalWrite(BT_ON_PIN, HIGH);  // Set pin 19 to high

  Serial1.begin(250000);  // Guitar
  Serial2.begin(250000);  // Keyboard
  Serial3.begin(250000);  // Cyber G to Guitar
  Serial4.begin(250000);  // Cyber G to Piano
  //Serial5.begin(9600);    // BT Serial Port for commands
  //Serial6.begin(9600);    // BLE Serial Port for commands
  //Serial7.begin(250000);  // BLE Midi
  Serial.begin(115200);  // USB debug monitor

  usbMIDI.begin();  //external USB device, typically either a USB midi instrument or the midi expression pedals
  isKeyboard = false;
  while (!Serial && millis() < 3000)
    ;  // Wait for Serial Monitor
       //if (debug) {
  Serial.println("Teensy MIDI Debug Start");
  //}
  //Serial3.println("AT\r\n");  // Send AT command

  if (!SD.begin(BUILTIN_SDCARD)) {
    if (debug)
      Serial.println("Error! SD card failed to initialize!");
    return;
  }

  prepareConfig();
  //printChords();
  prepareNeck();

  preparePatterns();
  prepareChords();
  if (!loadSettings()) {
    if (debug) {
      Serial.println("Error! Failed to open file for reading.");
    }
  }
  if (!loadPatternFiles(false)) {
    if (debug) {
      Serial.println("Error! Failed to open pattern file for reading.");
    }
  }
  //updateChords();
  tickTimer.begin(clockISR, computeTickInterval(presetBPM[preset]));  // in microseconds
  //enable general midi
  byte gm_sysex[] = { 0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7 };
  usbMIDI.sendSysEx(sizeof(gm_sysex), gm_sysex, true);
  if (BLEEnabled) {
    Serial7.write(gm_sysex, sizeof(gm_sysex));
  }
  sendProgram(BASS_CHANNEL, PROGRAM_ACOUSTIC_BASS);
  sendProgram(ACCOMPANIMENT_CHANNEL, PROGRAM_HARPSICORD);
  //sendProgram(DRUM_CHANNEL, PROGRAM_DRUMS);

  myusb.begin();
  midi1.setHandleNoteOn(handleExtUSBNoteOn);
  midi1.setHandleNoteOff(handleExtUSBNoteOff);
}

void readExtUSBUnhandledMIDI() {
  while (midi1.read()) {
    byte type = midi1.getType();
    byte channel = midi1.getChannel();
    byte d1 = midi1.getData1();
    byte d2 = midi1.getData2();
    int mult = 1;
    if (channel == 1 && type == midi::ControlChange) {
      volumeOffset = floor(d2 / 4.70);
    } else if (channel == 2) {
      mult = -1;
    }

    if (channel != 1 && type == midi::PitchBend) {
      int value = ((d2 << 7) | d1) - 8192;
      value = constrain(value + 512, -8192, 8191);
      usbMIDI.sendPitchBend(value * mult, GUITAR_CHANNEL);
      usbMIDI.sendPitchBend(value * mult, KEYBOARD_CHANNEL);
      if (BLEEnabled) {
        char data[] = { (char)(PITCH_BEND | GUITAR_CHANNEL), (char)(d1 & 0x7f), (char)(d2 & 0x7f) };
        Serial7.write(data, 3);
        char data2[] = { (char)(PITCH_BEND | KEYBOARD_CHANNEL), (char)(d1 & 0x7f), (char)(d2 & 0x7f) };
        Serial7.write(data2, 3);
      }
    }
  }
}

// Called when a note on is received from the USB MIDI device
void handleExtUSBNoteOn(byte channel, byte note, byte velocity) {
  int c = channel;
  if (externalUseFixedChannel) {
    c = EXTERNAL_CHANNEL;
  }
  // MODIFY note or velocity here if needed
  note += 1;  // Example modification

  // Send modified message out through Teensy's USB MIDI
  usbMIDI.sendNoteOn(note, velocity, c);
}

// Called when a note off is received from the USB MIDI device
void handleExtUSBNoteOff(byte channel, byte note, byte velocity) {
  int c = channel;
  if (externalUseFixedChannel) {
    c = EXTERNAL_CHANNEL;
  }
  // MODIFY note or velocity here if needed
  note += 1;  // Example modification

  // Send modified message out through Teensy's USB MIDI
  usbMIDI.sendNoteOff(note, velocity, c);
}

void sendSustain(uint8_t channel, bool isOn) {
  if (isOn) {
    usbMIDI.sendControlChange(64, 127, channel);
  } else {
    usbMIDI.sendControlChange(64, 0, channel);
  }
  if (debug) {
    Serial.printf("Sending Sustain %d on channel %d\n", isOn ? 1 : 0, channel);
  }
}

void sendStart() {
  usbMIDI.sendRealTime(START_MIDI);
  if (BLEEnabled) {
    char data[] = { START_MIDI };
    Serial7.write(data, 1);
  }
  if (debug) {
    Serial.printf("Start!\n");
  }
}

void sendStop() {
  usbMIDI.sendRealTime(STOP_MIDI);
  if (BLEEnabled) {
    char data[] = { STOP_MIDI };
    Serial7.write(data, 1);
  }
  if (debug) {
    Serial.printf("Stop\n");
  }
}

void sendContinue() {
  usbMIDI.sendRealTime(0xFB);
  if (BLEEnabled) {
    char data[] = { CONTINUE_MIDI };
    Serial7.write(data, 1);
  }
  if (debug) {
    Serial.printf("Continue\n");
  }
  playMode = 1;
}

void sendProgram(uint8_t channel, uint8_t program) {
  if (program == 2)  //right
  {
    curProgram = (curProgram + 1) % 128;
  } else if (program == 1) {  //left
    curProgram = curProgram - 1;
    if (curProgram < 0) {
      curProgram = 127;
    }
  } else {
    //do nothing;
  }
  if (program == 1 || program == 2) {
    usbMIDI.sendProgramChange(curProgram, channel);
    if (BLEEnabled) {
      char data[] = { (char)(PROGRAM_CHANGE | channel), (char)curProgram };
      Serial7.write(data, 2);
    }
    if (debug) {
      Serial.printf("Program: ch=%d program=%d\n", channel, curProgram);
    }
  } else {
    usbMIDI.sendProgramChange(program, channel);
    if (BLEEnabled) {
      char data[] = { (char)(PROGRAM_CHANGE | channel), (char)program };
      Serial7.write(data, 2);
    }
    //if (debug) {
    //Serial.printf("Program: ch=%d set program=%d\n", channel, program);
    //}
  }
}

void sendCC(uint8_t channel, uint8_t cc, uint8_t value) {
  //unused for now, may be used for adjusting key
  /*
  usbMIDI.sendControlChange(cc, value, channel);
  if (BLEEnabled)
  {
    char data[] = {(char)(cc | channel), (char) (value)};
    Serial7.write(data,2);
  }
  */
  //Serial.printf("CC: ch=%d cc=%d val=%d\n", channel, cc, value);
  if (cc == 9)  // transpose up
  {
    if (transpose < SEMITONESPEROCTAVE) {
      transpose++;
    }
  } else if (cc == 3)  //transpose down
  {
    if (transpose > -1 * SEMITONESPEROCTAVE) {
      transpose--;
    }
  }
  //Serial.printf("Transpose: ch=%d transpose=%d\n", channel, transpose);
}

void trimBuffer(String& buffer) {
  if (buffer.length() > MAX_BUFFER_SIZE) {
    buffer.remove(0, buffer.length() - MAX_BUFFER_SIZE);
  }
}

unsigned long firstPressTime = 0;
unsigned long lastPressTime = 0;
uint8_t pressCount = 0;

bool areChordsEqual(std::vector<uint8_t> chordNotesA, std::vector<uint8_t> chordNotesB) {
  bool curFound = false;
  for (uint8_t i = 0; i < chordNotesA.size(); i++) {
    curFound = false;
    for (uint8_t j = 0; j < chordNotesB.size(); j++) {
      //check if all notes in A have a match
      if (chordNotesA[i] == chordNotesB[j]) {
        curFound = true;
        break;
      }
    }
    if (curFound == false) {
      return false;
    }
  }
  for (uint8_t i = 0; i < chordNotesB.size(); i++) {
    curFound = false;
    for (uint8_t j = 0; j < chordNotesA.size(); j++) {
      //check if all notes in A have a match
      if (chordNotesB[i] == chordNotesA[j]) {
        curFound = true;
        break;
      }
    }
    if (curFound == false) {
      return false;
    }
  }
  return true;
}

void BPMTap() {
  unsigned long currentTime = millis();

  // Reset if more than 3 seconds since last press
  if (currentTime - lastPressTime > 2000) {
    pressCount = 0;
    firstPressTime = currentTime;
  }

  lastPressTime = currentTime;
  pressCount++;

  if (pressCount == 1) {
    firstPressTime = currentTime;  // Record first tap
  }

  if (pressCount >= 2) {
    unsigned long timeInterval = lastPressTime - firstPressTime;
    float bpm = (pressCount - 1) * 60000.0 / timeInterval;
    if (debug) {
      Serial.print("New BPM: ");
      Serial.println((int)bpm);
    }
    presetBPM[preset] = (int)bpm;
    setNewBPM(presetBPM[preset]);
  }
}

void updateNotes(std::vector<uint8_t>& chordNotesA, std::vector<uint8_t>& chordNotesB, std::vector<SequencerNote>& SN) {

  for (size_t j = 0; j < SN.size(); ++j) {
    if (SN[j].offset == 0 && SN[j].holdTime != INDEFINITE_HOLD_TIME) {
      continue;
    }
    uint8_t originalNote = SN[j].note;
    bool updated = false;
    if (originalNote == REST_NOTE) {
      continue;
    }
    for (size_t i = 0; i < chordNotesA.size(); ++i) {
      // Compare by pitch class (mod 12)
      if ((originalNote % 12) == (chordNotesA[i] % 12)) {
        if (i < chordNotesB.size()) {
          int octaveDiff = (int)originalNote - (int)chordNotesA[i];
          int newNote = (int)chordNotesB[i] + octaveDiff;

          // Clamp to MIDI range
          //Serial.printf("Pre-clamp %d\n", newNote);
          newNote = std::max(0, std::min(DEFAULT_VELOCITY, newNote));
          //Serial.printf("%d to %d\n", SN[j].note, newNote);
          SN[j].note = static_cast<uint8_t>(newNote);

          //Serial.printf("Mapped %d → %d (from %d → %d with octave diff %d)\n",
          //originalNote, SN[j].note,
          //chordNotesA[i], chordNotesB[i],
          //octaveDiff);

          updated = true;
          break;
        } else {
          //Serial.printf("Error! Unexpected diff size? %d vs %d\n", i, chordNotesB.size());
        }
      }
    }

    if (!updated && debug) {
      Serial.printf("Note %d did not match any in chord A\n", originalNote);
    }
  }
}

/*
void updateNotes(std::vector<uint8_t> &chordNotesA, std::vector<uint8_t> &chordNotesB, std::vector<SequencerNote> & SN)
{
  //uint8_t modded = 0;
  //uint8_t ignored = 0;
  std::vector<bool> noteModified(SN.size(), false);  // Track modifications
  for (int i = 0; i < chordNotesA.size(); i++)
  {
    Serial.printf("Chord A[%d] = %d\n", i, chordNotesA[i]);
  }
  for (int i = 0; i < chordNotesB.size(); i++)
  {
    Serial.printf("Chord B[%d] = %d\n", i, chordNotesB[i]);
  }
  for(uint8_t i = 0; i < chordNotesA.size(); i++)
  {
    for (uint8_t j = 0; j < SN.size(); j++)
    {
      //Serial.printf("SN[j] offset %d holdTime %d\n", SN[j].offset, SN[j].holdTime);
      if (noteModified[j])
      {
        continue;
      }
      //if (SN[j].offset == 0 && SN[j].holdTime != INDEFINITE_HOLD_TIME)
      //{
//        ignored++;
      //}
      if (SN[j].note == chordNotesA[i] && (SN[j].offset > 0 || (SN[j].offset == 0 && SN[j].holdTime == INDEFINITE_HOLD_TIME))) // we only care for unplayed notes that can also be long held
      {
        //if there is an equivalent on the new one, swap it out
        if (chordNotesB.size() > i)
        {
          //modded++;
          //Serial.printf("Note swapped!\n");
          noteModified[j] = true;
          Serial.printf("Changing A %d/%d to %d\n", chordNotesA[i],SN[j].note, chordNotesB[i]);
          SN[j].note = chordNotesB[i];
        }
        else //revert to placeholder
        {
          Serial.printf("Changing B SN %d to %d\n", SN[j].note, i);
          SN[j].note = i; 
          
          noteModified[j] = true;
          //modded++;
        }        
      }
      else if (SN[j].note <= 12)
      {
        if(SN[j].note < chordNotesB.size())
        {
          //modded++;
          noteModified[j] = true;
          SN[j].note = chordNotesB[SN[j].note];
          Serial.printf("Changing C %d to %d\n", chordNotesA[i], chordNotesB[i]);
        }
      }
    }
  }
  //Serial.printf("Updated notes is %d, ignored %d\n", modded, ignored);
}
*/
void generateOmnichordNoteMap(uint8_t note) {
  //need to create a note list
  omniChordNewNotes.clear();
  //int offset = -1 * SEMITONESPEROCTAVE;  //move down by 1 octave since original uses middle C
  int offset = 0;  //move down by 1 octave since original uses middle C

  //std::vector<uint8_t> chordNotes = assignedFretPatternsByPreset[preset][msg.note].getChords().getCompleteChordNotes();  //get notes
  //std::vector<uint8_t> chordNotes = getActualAssignedChord(msg.note).getChords().getCompleteChordNotes();  //get notes
  std::vector<uint8_t> chordNotes = getActualAssignedChord(note).getChords().getCompleteChordNotes3();  //get notes
  if (properOmniChord5ths[preset]) {
    for (uint8_t i = 0; i < chordNotes.size(); i++) {
      //if (chordNotes[i] == chordNotes[0] + 7 )
      if (i == 2)  // assumed to be always done for 3rd note/5ths
      {
        chordNotes[i] -= SEMITONESPEROCTAVE;
      }
      //chordNotes[i] += 12; //correct too low
    }
  }

  while (omniChordNewNotes.size() < omniChordOrigNotes.size()) {
    if (omniChordNewNotes.size() != 0) {
      offset += SEMITONESPEROCTAVE;
    }
    for (uint8_t i = 0; i < chordNotes.size() && omniChordNewNotes.size() < omniChordOrigNotes.size(); i++) {
      omniChordNewNotes.push_back(chordNotes[i] + offset + SEMITONESPEROCTAVE * omniKBTransposeOffset[preset]);
    }
  }
}

void checkSerialG2Guitar(HardwareSerial& serialPort, char* buffer, uint8_t& bufferLen) {
  //size_t last_len = 0;
  //uint8_t rootNote = 0;
  // --- Step 1: Read bytes into buffer ---
  
  while (serialPort.available() && bufferLen < MAX_BUFFER_SIZE - 1) {
    buffer[bufferLen++] = serialPort.read();
  }
  /*
 if (bufferLen > 0) {
    for (uint8_t i = 0; i < bufferLen; ++i) {
      bufferList.push_back(buffer[i]);
    }
  }

 if (bufferList.size() >= 16) {
    Serial.print("Dump: ");
    for (size_t i = 0; i < bufferList.size(); ++i) {
      Serial.printf("%02X ", static_cast<uint8_t>(bufferList[i]));
    }
    Serial.println();
    bufferList.clear(); // Reset for next batch
  }
*/
  
  serialPort.write(buffer, bufferLen);
  /**/

  bufferLen = 0;
}
/*
void checkSerialG2KB(HardwareSerial& serialPort, char* buffer, uint8_t& bufferLen) {
  //size_t last_len = 0;
  //uint8_t rootNote = 0;
  // --- Step 1: Read bytes into buffer ---
  
  while (serialPort.available() && bufferLen < MAX_BUFFER_SIZE - 1) {
    buffer[bufferLen++] = serialPort.read();
  }
  
 if (bufferLen > 0) {
    for (uint8_t i = 0; i < bufferLen; ++i) {
      bufferList.push_back(buffer[i]);
    }
  }

 if (bufferList.size() >= 32) {
    Serial.print("Dump: ");
    for (size_t i = 0; i < bufferList.size(); ++i) {
      Serial.printf("%02X ", static_cast<uint8_t>(bufferList[i]));
    }
    Serial.println();
    bufferList.clear(); // Reset for next batch
  }

  
  serialPort.write(buffer, bufferLen);

  bufferLen = 0;
}
*/


void checkSerialG2KB(HardwareSerial& serialPort, char* buffer, uint8_t& bufferLen) {
  // Step 1: Read serial data into buffer
  while (serialPort.available() && bufferLen < MAX_BUFFER_SIZE - 1) {
    buffer[bufferLen++] = serialPort.read();
  }

  // Step 2: Push new bytes to bufferList
  if (bufferLen > 0) {
    for (uint8_t i = 0; i < bufferLen; ++i) {
      bufferList.push_back(buffer[i]);
    }
  }
  while (bufferList.size() >= 9) 
  {
    bool matchFound = false;
    for (size_t i = 0; i <= bufferList.size() - 9; ++i) 
    {
      if ((uint8_t)bufferList[i]     == 0xF5 &&
          (uint8_t)bufferList[i + 1] == 0x55 &&
          (uint8_t)bufferList[i + 2] == 0x00 &&
          (uint8_t)bufferList[i + 3] == 0x06 &&
          (uint8_t)bufferList[i + 4] == 0x00 &&
          (uint8_t)bufferList[i + 5] == 0x13 )
          //&& (uint8_t)bufferList[i + 8] == 0x00 &&
          //(uint8_t)bufferList[i + 9] == 0x07) 
          {
        
        // Extract and print xx and yy
        uint8_t xx = bufferList[i + 6];
        uint8_t yy = bufferList[i + 7];
        uint8_t zz = bufferList[i + 8];
        cyberGCapo = getCyberGCapo(bufferList[i + 6]);
        Serial.print("Virtual Capo Signal -> XX: ");
        Serial.printf("%02X, YY: %02X, ZZ: %02X\n", xx, yy, zz);
        Serial.printf("Capo is %d\n", cyberGCapo);

        // Remove matched 10-byte message
        bufferList.erase(bufferList.begin() + i, bufferList.begin() + i + 9);
        matchFound = true;
        break; // Restart search from beginning
      }
      else if (
        (uint8_t)bufferList[i] == 0xF5 &&
        (uint8_t)bufferList[i + 1] == 0x55 &&
        (uint8_t)bufferList[i + 2] == 0x00 &&
        (uint8_t)bufferList[i + 3] == 0x03 &&
        (uint8_t)bufferList[i + 4] == 0x00 &&
        (uint8_t)bufferList[i + 5] == 0x12 )
        {
          cyberGOctave = bufferList[i + 6];
          
          Serial.printf("Octave Signal -> cyberGOctave: %02x\n", cyberGOctave);
          matchFound = true;
          bufferList.erase(bufferList.begin() + i, bufferList.begin() + i + 7);
        }
    }
    if (!matchFound) break; // Exit if no further matches found
  }

  // Step 4: Echo received data back to serial port
  serialPort.write(buffer, bufferLen);

  // Step 5: Clear temp buffer
  bufferLen = 0;
}

bool wasLastOctave = false;
void checkSerialGuitar(HardwareSerial& serialPort, char* buffer, uint8_t& bufferLen, uint8_t channel) {
  size_t last_len = 0;
  bool forcePassThrough = false;
  //uint8_t rootNote = 0;
  // --- Step 1: Read bytes into buffer ---
  while (serialPort.available() && bufferLen < MAX_BUFFER_SIZE - 1) {
    buffer[bufferLen++] = serialPort.read();
  }
  /*
  if (bufferLen >= 6) {
    Serial.print("Raw buffer: ");
    for (uint8_t i = 0; i < bufferLen; i++) {
      Serial.printf("%02x", buffer[i]);
    }
    Serial.println();
  }
*/
  while (bufferLen >= 2) {

    if (buffer[0] == 0xaa && buffer[1] == 0x55) {
      break;
    }
    if (passThroughSerial) {
      serialPort.write(buffer, 2);
    }
    memmove(buffer, buffer + 2, bufferLen - 2);
    bufferLen -= 2;
  }

  bool ignore;
  // --- Step 5: Main parser loop ---
  while (true) {
    ignore = false;
    uint8_t myTranspose = guitarTranspose[preset];
    bool processed = false;
    bool matched = false;
    if (bufferLen > 6) {
      HexToProgram msg;
      //for (const HexToProgram& msg : hexToProgram) {
      for (uint8_t i = 0; i < 2; i++) {
        msg = hexToProgram(i);
        size_t len = strlen(msg.hex);
        size_t expectedBytes = len / 2;
        //if (bufferLen >= len && strncmp(buffer, msg.hex, len) == 0) {
        if (bufferLen >= expectedBytes && isHexStringEqualToBytes(msg.hex, len, buffer, expectedBytes)) {
          if (debug) {
            Serial.printf("Program was pressed %d\n", msg.program);
          }
          sendCC(channel, msg.program, MAX_VELOCITY);

          if (fullPassThroughSerial) {
            serialPort.write(buffer, len / 2);
          }

          memmove(buffer, buffer + len / 2, bufferLen - len / 2);
          bufferLen -= len / 2;
          matched = true;
          processed = true;
          break;
        }
      }

      if (matched) continue;
    }
    MidiMessage msg;
    forcePassThrough = false;
    if (bufferLen >= 11 and !matched) {
      //for (const MidiMessage& msg : hexToNote) {
      for (uint8_t a = 0; a <= ACTUAL_NECKBUTTONS; a++) {
        msg = hexToNote(a);
        size_t len = strlen(msg.hex);
        last_len = len;
        size_t expectedBytes = len / 2;

        //if (bufferLen >= len && strncmp(buffer, msg.hex, len) == 0)
        if (bufferLen >= expectedBytes && isHexStringEqualToBytes(msg.hex, len, buffer, expectedBytes)) {
          isSustainPressed = false;
          //handling for last 2 rows of guitar neck
          if (msg.note >= MIN_IGNORED_GUITAR && msg.note <= MAX_IGNORED_GUITAR) {

            wasLastOctave = false;
            if (msg.note == BPM_COMPUTE_BUTTON) {
              BPMTap();
            } else if (msg.note == SUSTAIN_BUTTON) {
              if (useToggleSustain[preset]) {
                isSustain = !isSustain;
              } else {
                isSustain = true;
              }
              sendSustain(KEYBOARD_CHANNEL, isSustain);
            } else if (msg.note == DRUM_STARTSTOP_BUTTON) {
              //if (drumsEnabled[preset]) //if drums on, need them to start first
              {
                if (drumState == DrumStopped) {
                  if (debug) {
                    Serial.printf("Starting drum sequencer\n");
                  }
                  if (drumsEnabled[preset]) {
                    if (DrumIntroSequencer.size() > 0) {
                      drumState = DrumIntro;
                      drumNextState = DrumNone;
                      prepareDrumSequencer();
                    } else {
                      drumState = DrumLoop;
                      drumNextState = DrumNone;
                      prepareDrumSequencer();
                    }
                  }
                  //if (bassEnabled[preset])
                  {
                    bassStart = true;
                  }
                  //if (accompanimentEnabled[preset] && omniChordModeGuitar[preset] > OmniChordOffType)
                  {
                    accompanimentStart = true;
                  }
                } else {
                  if (debug)
                    Serial.printf("Stopping drum sequencer\n");
                  bassStart = false;
                  accompanimentStart = false;
                  if (backingState != 0) {
                    drumState = DrumStopped;
                    drumNextState = DrumStopped;
                    DrumSequencerNotes.clear();
                  } else {
                    drumNextState = DrumEnding;
                    prepareDrumSequencer();
                  }
                }
              }
            } else if (msg.note == DRUM_FILL_BUTTON) {
              if (drumState == DrumStopped || drumState == DrumNone || drumNextState == DrumEnding || drumState == DrumEnding) {
                //ignore
              } else {
                if (DrumFillSequencer.size() > 0 && debug) {
                  Serial.printf("Adding drum fill\n");
                  drumNextState = DrumLoopFill;
                } else if (debug) {
                  Serial.printf("Drum fill is empty\n");
                }
              }
            } else if (msg.note == OCTAVE_DOWN_BUTTON || msg.note == OCTAVE_UP_BUTTON) {
              //Serial.printf("Pass through button!\n");
              forcePassThrough = true;
            }
          } else  //handling for first 7 rows
          {

            isSustainPressed = false;
            lastNeckButtonPressed = neckButtonPressed;
            if (lastNeckButtonPressed != -1) {

              lastValidNeckButtonPressed2 = lastValidNeckButtonPressed;
              lastValidNeckButtonPressed = neckButtonPressed;
              //Serial.printf("Guitar: last1=%d, last2= %d changed %d\n", lastValidNeckButtonPressed, lastValidNeckButtonPressed2, buttonPressedChanged?1:0);
            } else if (isSustain && !useToggleSustain[preset]) {
              isSustain = false;
              sendSustain(KEYBOARD_CHANNEL, false);
            }
            if (msg.note != 255)  //button press
            {
              //Serial.printf("not Pass through button!\n");
              wasLastOctave = false;
              neckButtonPressed = msg.note;

              if (!isKeyboard) {
                myTranspose = 0;
              } else {
                //capo
                myTranspose = guitarTranspose[preset];
              }
              //if (debug) {
              //Serial.printf("Get strum pattern %d button = %d\n", assignedFretPatternsByPreset[preset][msg.note].getPatternAssigned(), msg.note);
              //}
              //guitar plays chord on button press (always if keyboard)
              //omnichord mode for guitar and piano
              if (isKeyboard || omniChordModeGuitar[preset] > OmniChordOffGuitarType) {
                ignore = true;
                //if omnichord mode and is using paddle or keyboard is in a legit omnichord mode, prepare the notes
                if (omniChordModeGuitar[preset] > OmniChordOffGuitarType && (!isKeyboard || (isKeyboard && omniChordModeGuitar[preset] > OmniChordOffGuitarType))) {
                  //check if button pressed is the same
                  if (lastValidNeckButtonPressed != neckButtonPressed || omniChordNewNotes.size() == 0) {
                    generateOmnichordNoteMap(msg.note);
                  }
                }
                //plays guitar chords when neck button is pressed for non omnichord guitar mode
                //if (assignedFretPatternsByPreset[preset][msg.note].getPatternStyle() == SimpleStrum && omniChordModeGuitar[preset] != OmniChordGuitarType)
                //if (simpleChordSetting[preset] == SimpleStrum && omniChordModeGuitar[preset] != OmniChordGuitarType)
                if (simpleChordSetting[preset] == SimpleStrum && ((omniChordModeGuitar[preset] != OmniChordGuitarType && !isKeyboard) || (isKeyboard && (omniChordModeGuitar[preset] == OmniChordGuitarType || omniChordModeGuitar[preset] <= OmniChordOffGuitarType)))) {
                  lastSimple = true;
                  if (lastGuitarNotes.size() > 0) {
                    for (uint8_t i = 0; i < lastGuitarNotes[i]; i++) {
                      sendNoteOff(channel, lastGuitarNotes[i]);
                    }
                    lastGuitarNotes.clear();
                  }
                  //rootNote = assignedFretPatternsByPreset[preset][msg.note].getChords().getRootNote();
                  //rootNote = getActualAssignedChord(msg.note).getChords().getRootNote();

                  //if (debug) {
                  //Serial.printf("Root note is %d\n", rootNote);
                  //}
                  //plays root to last notes
                  std::vector<uint8_t> notesToPlay;
                  if (omniChordModeGuitar[preset] == OmniChordOffGuitarType) {
                    notesToPlay = getActualAssignedChord(msg.note).getGuitarChords().getCompleteChordNotes();
                  } else {
                    //sendNoteOn(channel, rootNote + myTranspose);
                    //lastGuitarNotes.push_back(rootNote + myTranspose);
                    notesToPlay = getActualAssignedChord(msg.note).getChords().getCompleteChordNotes();
                  }

                  //getActualAssignedChord(
                  //for (uint8_t note : getActualAssignedChord(msg.note).getChords().getChordNotes()) {
                  for (uint8_t i = 0; i < notesToPlay.size(); i++) {
                    //sendNoteOn(channel, rootNote + note + myTranspose);
                    sendNoteOn(channel, notesToPlay[i] + myTranspose);
                    lastGuitarNotes.push_back(notesToPlay[i] + myTranspose);
                  }
                } else if (simpleChordSetting[preset] != SimpleStrum && isKeyboard && omniChordModeGuitar[preset] == OmniChordGuitarType) {
                  lastSimple = false;
                  //auto or manual type
                  //if (debug) {
                  //Serial.printf("Not simple! Type is %d\n", (int) simpleChordSetting[preset]);
                  //}
                  bool isReverse = false;
                  if (alternateDirection[preset]) {
                    isStrumUp = !isStrumUp;
                    isReverse = isStrumUp;
                  }
                  int curbut = getCurrentButtonPressed();
                  int lastbut = getPreviousButtonPressed();
                  //if (lastValidNeckButtonPressed != neckButtonPressed && lastValidNeckButtonPressed != -1)
                  if (curbut != lastbut && curbut != -1) {
                    for (uint8_t i = 0; i < SequencerNotes.size(); i++) {
                      if (SequencerNotes[i].channel == GUITAR_CHANNEL) {
                        sendNoteOff(channel, SequencerNotes[i].note);
                      }
                    }
                    SequencerNotes.clear();
                  }
                  if (curbut != -1 && lastbut != -1 && curbut != lastbut)
                  //if (lastValidNeckButtonPressed2 != lastValidNeckButtonPressed && lastValidNeckButtonPressed2 != -1 && lastValidNeckButtonPressed != -1)// && StaggeredSequencerNotes.size() > 0)
                  {
                    //Serial.printf("You need to change the notes! %d vs %d\n", lastbut, curbut);
                    std::vector<uint8_t> chordNotesA;
                    std::vector<uint8_t> chordNotesB;
                    if (enableAllNotesOnChords[preset]) {
                      //chordNotesA = assignedFretPatternsByPreset[preset][lastValidNeckButtonPressed2].getChords().getCompleteChordNotes();  //get notes
                      //chordNotesB = assignedFretPatternsByPreset[preset][lastValidNeckButtonPressed].getChords().getCompleteChordNotes();  //get notes
                      //chordNotesA = getActualAssignedChord(lastValidNeckButtonPressed2).getChords().getCompleteChordNotes(true);  //get notes
                      //chordNotesB = getActualAssignedChord(lastValidNeckButtonPressed).getChords().getCompleteChordNotes(true);  //get notes
                      chordNotesA = getActualAssignedChord(lastbut).getChords().getCompleteChordNotes(true);  //get notes
                      chordNotesB = getActualAssignedChord(curbut).getChords().getCompleteChordNotes(true);   //get notes
                    } else {
                      //
                      //chordNotesA = assignedFretPatternsByPreset[preset][lastValidNeckButtonPressed2].getChords().getCompleteChordNotes3();  //get notes
                      //chordNotesB = assignedFretPatternsByPreset[preset][lastValidNeckButtonPressed].getChords().getCompleteChordNotes3();  //get notes
                      //chordNotesA = getActualAssignedChord(lastValidNeckButtonPressed2).getChords().getCompleteChordNotes3(true);  //get notes
                      //chordNotesB = getActualAssignedChord(lastValidNeckButtonPressed).getChords().getCompleteChordNotes3(true);  //get notes
                      chordNotesA = getActualAssignedChord(lastbut).getChords().getCompleteChordNotes3(true);  //get notes
                      chordNotesB = getActualAssignedChord(curbut).getChords().getCompleteChordNotes3(true);   //get notes
                    }
                    if (StaggeredSequencerNotes.size() > 0) {
                      updateNotes(chordNotesA, chordNotesB, StaggeredSequencerNotes);
                    } else {
                    }
                  }
                  /*
                  else 
                  {
                    //todo check if still needed
                    if (neckButtonPressed != lastValidNeckButtonPressed && neckButtonPressed != -1) //if size is 0
                    {
                      lastValidNeckButtonPressed2 = lastValidNeckButtonPressed;
                      lastValidNeckButtonPressed = neckButtonPressed; //force change to reflect in buildAutoManualNotes
                    }
                  }
                  */
                  //gets and plays next manual strum notes
                  //buildAutoManualNotes(isReverse, msg.note); //always in order for
                  buildAutoManualNotes(isReverse, curbut);  //always in order for
                  //add whole sequence to to play queue
                  // depending on the mode, it will start playing if piano
                  //paddle mode does not play the notes automatically
                }
                //need to create one for kb + auto/manual strum
                //plays non simple patterns, manual press or automatic, tentatively piano chord?
                else if (omniChordModeGuitar[preset] != OmniChordGuitarType)  // other strum type, omnichord mode = off
                {
                  lastSimple = false;
                  //auto or manual type
                  //if (debug) {
                  //Serial.printf("Not simple! Type is %d\n", (int) simpleChordSetting[preset]);
                  //}
                  bool isReverse = false;
                  if (alternateDirection[preset]) {
                    isStrumUp = !isStrumUp;
                    isReverse = isStrumUp;
                  }
                  int curbut = getCurrentButtonPressed();
                  int lastbut = getPreviousButtonPressed();
                  //if (lastValidNeckButtonPressed != neckButtonPressed && lastValidNeckButtonPressed != -1)
                  if (curbut != lastbut && curbut != -1) {
                    for (uint8_t i = 0; i < SequencerNotes.size(); i++) {
                      if (SequencerNotes[i].channel == GUITAR_CHANNEL) {
                        sendNoteOff(channel, SequencerNotes[i].note);
                      }
                    }
                    SequencerNotes.clear();
                  }
                  if (curbut != -1 && lastbut != -1 && curbut != lastbut)
                  //if (lastValidNeckButtonPressed2 != lastValidNeckButtonPressed && lastValidNeckButtonPressed2 != -1 && lastValidNeckButtonPressed != -1)// && StaggeredSequencerNotes.size() > 0)
                  {
                    //Serial.printf("You need to change the notes! %d vs %d\n", lastbut, curbut);
                    std::vector<uint8_t> chordNotesA;
                    std::vector<uint8_t> chordNotesB;
                    if (enableAllNotesOnChords[preset]) {
                      //chordNotesA = assignedFretPatternsByPreset[preset][lastValidNeckButtonPressed2].getChords().getCompleteChordNotes();  //get notes
                      //chordNotesB = assignedFretPatternsByPreset[preset][lastValidNeckButtonPressed].getChords().getCompleteChordNotes();  //get notes
                      //chordNotesA = getActualAssignedChord(lastValidNeckButtonPressed2).getChords().getCompleteChordNotes(true);  //get notes
                      //chordNotesB = getActualAssignedChord(lastValidNeckButtonPressed).getChords().getCompleteChordNotes(true);  //get notes
                      chordNotesA = getActualAssignedChord(lastbut).getChords().getCompleteChordNotes(true);  //get notes
                      chordNotesB = getActualAssignedChord(curbut).getChords().getCompleteChordNotes(true);   //get notes
                    } else {
                      //
                      //chordNotesA = assignedFretPatternsByPreset[preset][lastValidNeckButtonPressed2].getChords().getCompleteChordNotes3();  //get notes
                      //chordNotesB = assignedFretPatternsByPreset[preset][lastValidNeckButtonPressed].getChords().getCompleteChordNotes3();  //get notes
                      //chordNotesA = getActualAssignedChord(lastValidNeckButtonPressed2).getChords().getCompleteChordNotes3(true);  //get notes
                      //chordNotesB = getActualAssignedChord(lastValidNeckButtonPressed).getChords().getCompleteChordNotes3(true);  //get notes
                      chordNotesA = getActualAssignedChord(lastbut).getChords().getCompleteChordNotes3(true);  //get notes
                      chordNotesB = getActualAssignedChord(curbut).getChords().getCompleteChordNotes3(true);   //get notes
                    }
                    if (StaggeredSequencerNotes.size() > 0) {
                      updateNotes(chordNotesA, chordNotesB, StaggeredSequencerNotes);
                    } else {
                    }
                  }
                  /*
                  else 
                  {
                    //todo check if still needed
                    if (neckButtonPressed != lastValidNeckButtonPressed && neckButtonPressed != -1) //if size is 0
                    {
                      lastValidNeckButtonPressed2 = lastValidNeckButtonPressed;
                      lastValidNeckButtonPressed = neckButtonPressed; //force change to reflect in buildAutoManualNotes
                    }
                  }
                  */
                  //gets and plays next manual strum notes
                  //buildAutoManualNotes(isReverse, msg.note); //always in order for
                  buildAutoManualNotes(isReverse, curbut);  //always in order for
                  //add whole sequence to to play queue
                  // depending on the mode, it will start playing if piano
                  //paddle mode does not play the notes automatically

                }
                //omnichord guitar mode
                else {
                  ignore = false;  //force to still use paddle
                  //do nothing
                }
                //midi button handling if enabled
                if (enableButtonMidi[preset]) {

                  sendNoteOn(GUITAR_BUTTON_CHANNEL, msg.note + MIDI_BUTTON_CHANNEL_NOTE_OFFSET, 1);  //play the specific note on another channel at minimum non 0 volume
                  if (lastValidNeckButtonPressed != lastNeckButtonPressed && lastValidNeckButtonPressed >= 0) {
                    sendNoteOff(GUITAR_BUTTON_CHANNEL, lastValidNeckButtonPressed + MIDI_BUTTON_CHANNEL_NOTE_OFFSET);  //play the specific note on another channel at minimum non 0 volume
                  }
                  lastGuitarNotesButtons.push_back(msg.note + MIDI_BUTTON_CHANNEL_NOTE_OFFSET);
                }
              }
              //else //using paddle adapter - omnichord mode = guitar
              if (!ignore) {
                //mutes previous if another button is pressed
                //Serial.printf("MuteWhenLetGo %d, ignoreSameChord[preset] %d, LastValidpressed = %d neckPressed = %d\n", muteWhenLetGo[preset]?1:0, ignoreSameChord[preset]?1:0, lastValidNeckButtonPressed, neckButtonPressed);
                if (getPreviousButtonPressed() >= 0 && getCurrentButtonPressed() >= 0 && !muteWhenLetGo[preset]) {
                  //std::vector<uint8_t> chordNotes = assignedFretPatternsByPreset[preset][msg.note].getChords().getCompleteChordNotes(); //get notes
                  //check if same notes are to be played
                  bool ignorePress = false;
                  if (ignoreSameChord[preset]) {
                    //Serial.printf("lastGuitarNotes size %d SequencerNotes size %d\n", lastGuitarNotes.size(), SequencerNotes.size());
                    bool isEqual = true;
                    //
                    //std::vector<uint8_t> chordNotesA = assignedFretPatternsByPreset[preset][getPreviousButtonPressed()].getChords().getCompleteChordNotes();  //get notes
                    //std::vector<uint8_t> chordNotesB = assignedFretPatternsByPreset[preset][getCurrentButtonPressed()].getChords().getCompleteChordNotes();           //get notes
                    std::vector<uint8_t> chordNotesA = getActualAssignedChord(getPreviousButtonPressed()).getChords().getCompleteChordNotes();  //get notes
                    std::vector<uint8_t> chordNotesB = getActualAssignedChord(getCurrentButtonPressed()).getChords().getCompleteChordNotes();   //get notes
                    /*
                    for (uint8_t i = 0; i < chordNotesA.size() && i < chordNotesB.size(); i++) {
                      if (chordNotesA[i] != chordNotesB[i]) {
                        if (debug) {
                          Serial.printf("Chords are not the same! %d vs %d\n", chordNotesA[i], chordNotesB[i]);
                        }
                        isEqual = false;
                        break;
                      }
                    }
                    */
                    isEqual = areChordsEqual(chordNotesA, chordNotesB);
                    if (!isEqual) {
                      if (debug) {
                        Serial.printf("Chords are not the same!\n");
                      }
                    }
                    if (isEqual) {
                      ignorePress = true;
                    }
                  }
                  if (debug) {
                    Serial.printf("ignorePress = %d\n", ignorePress ? 1 : 0);
                  }
                  //cancelGuitarChordNotes(assignedFretPatternsByPreset[preset][lastNeckButtonPressed].assignedGuitarChord);
                  //for paddle we actually stop playing when user lets go of "strings"
                  //
                  if (!ignorePress) {
                    while (lastGuitarNotes.size() > 0 && muteSeparation[preset] == 0) {
                      sendNoteOff(channel, lastGuitarNotes.back());
                      lastGuitarNotes.pop_back();
                    }
                    if (enableButtonMidi[preset]) {
                      while (lastGuitarNotesButtons.size() > 0) {
                        sendNoteOff(channel, lastGuitarNotesButtons.back());
                        lastGuitarNotesButtons.pop_back();
                      }
                    }
                    //handle string let go case and turn off all notes
                    if (muteSeparation[preset] == 0) {
                      for (uint8_t i = 0; i < SequencerNotes.size(); i++) {
                        if (SequencerNotes[i].channel == GUITAR_CHANNEL) {
                          SequencerNotes[i].offset = 0;
                          SequencerNotes[i].holdTime = 0;
                        }
                      }
                    } else {
                      gradualCancelNotes(lastGuitarNotes, GUITAR_CHANNEL);
                      lastGuitarNotes.clear();
                    }
                  }
                }
                //do nothing as we only care about neck tracking
              }
              //fix chord change issue on bass/accompaniment here
              //force update here
              //lastValidNeckButtonPressed = lastNeckButtonPressed;
              //lastNeckButtonPressed = neckButtonPressed;
            } else  //no button pressed on neck
            {
              lastNeckButtonPressed = neckButtonPressed;
              neckButtonPressed = -1;
              if (muteWhenLetGo[preset]) {
                //for paddle we actually stop playing when user lets go of "strings"
                //todo add check if it really is for paddle only
                while (lastGuitarNotes.size() > 0) {
                  sendNoteOff(channel, lastGuitarNotes.back());
                  lastGuitarNotes.pop_back();
                }
                if (enableButtonMidi[preset]) {
                  while (lastGuitarNotesButtons.size() > 0) {
                    sendNoteOff(channel, lastGuitarNotesButtons.back());
                    lastGuitarNotesButtons.pop_back();
                  }
                }
                if (lastNeckButtonPressed != -1 && !isKeyboard) {
                  //handle string let go case and turn off all notes
                  for (uint8_t i = 0; i < SequencerNotes.size(); i++) {
                    SequencerNotes[i].offset = 0;
                    SequencerNotes[i].holdTime = 0;
                  }
                }
              }
            }
          }
          if (forcePassThrough)  // need to send let go for octave
          {

            wasLastOctave = true;
          }
          if (wasLastOctave) {
            forcePassThrough = true;
          }
          if (fullPassThroughSerial || forcePassThrough) {
            serialPort.write(buffer, len / 2);
          }

          memmove(buffer, buffer + len / 2, bufferLen - len / 2);
          bufferLen -= len / 2;
          matched = true;
          processed = true;  //did something
          break;
        }
      }
      if (matched) continue;
      else {
        if (passThroughSerial) {
          serialPort.write(buffer, last_len / 2);
        }
        memmove(buffer, buffer + last_len / 2, bufferLen - last_len / 2);
        bufferLen -= last_len;
      }
      break;
    }

    if (!processed) break;
  }

  if (bufferLen > MAX_BUFFER_SIZE - 10) {
    if (passThroughSerial) {
      serialPort.write(buffer, bufferLen - 60 / 2);
    }
    memmove(buffer, buffer + bufferLen - 60 / 2, 60 / 2);
    bufferLen = 60 / 2;
  }
}

std::vector<String> splitAsciiCommand(char* buffer, uint8_t bufferLen) {
  std::vector<String> result;
  String token = "";

  for (uint8_t i = 0; i < bufferLen; ++i) {
    char c = buffer[i];
    if (c == ',') {
      result.push_back(token);
      token = "";
    } else {
      token += c;
    }
  }

  if (token.length() > 0) {
    result.push_back(token);  // last token
  }

  return result;
}

void checkSerialBTClassic(HardwareSerial& serialPort, char* buffer, uint8_t& bufferLen) {
  String lineout = "";
  bool error = false;
  while (serialPort.available()) {
    char c = serialPort.read();

    if (c == '\r') {
      continue;
    } else if (c == '\n') {
      // Line complete
      buffer[bufferLen] = '\0';  // Null-terminate the string
      break;
    } else {
      if (bufferLen < MAX_BUFFER_SIZE - 1) {
        buffer[bufferLen++] = c;
      } else {
        // Buffer overflow protection
        error = true;
        bufferLen = 0;
        break;
      }
    }
  }
  if (error) {
    bufferLen = 0;
    return;
  }
  //assume we got data already.
  std::vector<String> data = splitAsciiCommand(buffer, bufferLen);
  if (data.size() == 0) {
    bufferLen = 0;
    return;
  }
  if (data[0] == "OK00") {
    if (lastCommandBTClassicSent.size() == 0)  //error
    {
      bufferLen = 0;
      return;
    }
    switch (lastCommandBTClassicSent.front()) {
      case BTMW:  //BTMW //Make discoverable
      case BTRA:  //BTRA disconnect all
      case BTOW:  // BTOW  BT On/Off
        break;
      case BTMR:  //BTMR
        if (data.size() >= 2) {
          BTClassicDiscoveryEnabled = atoi(data[1].c_str()) == 1;
        }
        break;
      case BTOR:  //BTOR
        if (data.size() >= 2) {
          BTClassicEnabled = atoi(data[1].c_str()) == 1;
        }
        break;
    }
  } else {
    if (debug) {
      Serial.printf("On BT Classic, I received wrong %s\n", data[0].c_str());
    }
    if (lastCommandBTClassicSent.size() == 0)  //error
    {
      bufferLen = 0;
      return;
    }
  }
  lastCommandBTClassicSent.pop_front();
  bufferLen = 0;
}

void checkSerialBTBLEMidi(HardwareSerial& serialPort, char* buffer, uint8_t& bufferLen) {
  String lineout = "";
  bool error = false;
  while (serialPort.available()) {
    char c = serialPort.read();

    if (c == '\r') {
      continue;
    } else if (c == '\n') {
      // Line complete
      buffer[bufferLen] = '\0';  // Null-terminate the string
      break;
    } else {
      if (bufferLen < MAX_BUFFER_SIZE - 1) {
        buffer[bufferLen++] = c;
      } else {
        // Buffer overflow protection
        error = true;
        bufferLen = 0;
        break;
      }
    }
  }
  if (error) {
    bufferLen = 0;
    return;
  }
  //assume we got data already.
  std::vector<String> data = splitAsciiCommand(buffer, bufferLen);
  if (data.size() == 0) {
    bufferLen = 0;
    return;
  }
  if (data[0] == "OK00") {
    if (lastCommandBLEMidiSent.size() == 0)  //error
    {
      bufferLen = 0;
      return;
    }
    switch (lastCommandBLEMidiSent.front()) {
      case BTMW:  //BTMW //Make discoverable
      case BTRA:  //BTRA disconnect all
      case BTOW:  // BTOW  BT On/Off
        break;
      case BTMR:  //BTMR
        if (data.size() >= 2) {
          BLEDiscoveryEnabled = atoi(data[1].c_str()) == 1;
        }
        break;
      case BTOR:  //BTOR
        if (data.size() >= 2) {
          BLEEnabled = atoi(data[1].c_str()) == 1;
        }
        break;
    }
  } else {
    if (debug) {
      Serial.printf("On BT BLE, I received wrong %s\n", data[0].c_str());
    }
    if (lastCommandBLEMidiSent.size() == 0)  //error
    {
      bufferLen = 0;
      return;
    }
  }
  lastCommandBLEMidiSent.pop_front();
  bufferLen = 0;
}

void lightAKey(int key) {
  if (isKeyboard && key >= 0 && key <=0x13) {
    char pData[] = { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x16, (char)key };  //F3
    Serial4.write(pData, sizeof(pData));                              //only last is lit
  }
}
bool savePatternFiles(bool saveToBackup = false) {
  if (!savePatternRelatedConfig()) {
    return false;
  };
  String myFile = "pattern.ini";
  if (saveToBackup) {
    myFile = "patternBackup.ini";
  }
  File f = SD.open(myFile.c_str(), FILE_WRITE);
  if (f) {
    f.seek(0);
    f.truncate();
    //file.println("your new config data");
    //snprintf(buffer, sizeof(buffer), "Name: %s, Int: %d, Float: %.2f\n", name, someValue, anotherValue);
    //file.print(buffer);
    char buffer[64];
    //std::vector<SequencerNote> DrumLoopSequencer; // drum loop pattern/drum pattern
    //std::vector<SequencerNote> DrumLoopHalfBarSequencer; // drum loop pattern bar sequencer
    //std::vector<SequencerNote> DrumFillSequencer; // drum fill pattern
    //std::vector<SequencerNote> DrumIntroSequencer; // drum Intro pattern
    //std::vector<SequencerNote> DrumEndSequencer; // drum Intro pattern
    if (DrumIntroSequencer.size() > 0) {
      snprintf(buffer, sizeof(buffer), "DRUM_INTRO_START\n");
      f.print(buffer);
      for (auto& seqNote : DrumIntroSequencer) {
        snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d,%d\n", seqNote.note, seqNote.holdTime, seqNote.offset, seqNote.velocity, seqNote.relativeOctave);
        f.print(buffer);
      }
      snprintf(buffer, sizeof(buffer), "DRUM_INTRO_END\n");
      f.print(buffer);
    }
    if (DrumLoopHalfBarSequencer.size() > 0) {
      snprintf(buffer, sizeof(buffer), "DRUM_HALFLOOP_START\n");
      f.print(buffer);
      for (auto& seqNote : DrumLoopHalfBarSequencer) {
        snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d,%d\n", seqNote.note, seqNote.holdTime, seqNote.offset, seqNote.velocity, seqNote.relativeOctave);
        f.print(buffer);
      }
      snprintf(buffer, sizeof(buffer), "DRUM_HALFLOOP_END\n");
      f.print(buffer);
    }
    if (DrumLoopSequencer.size() > 0) {
      snprintf(buffer, sizeof(buffer), "DRUM_LOOP_START\n");
      f.print(buffer);
      for (auto& seqNote : DrumLoopSequencer) {
        snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d,%d\n", seqNote.note, seqNote.holdTime, seqNote.offset, seqNote.velocity, seqNote.relativeOctave);
        f.print(buffer);
      }
      snprintf(buffer, sizeof(buffer), "DRUM_LOOP_END\n");
      f.print(buffer);
    }

    if (DrumFillSequencer.size() > 0) {
      snprintf(buffer, sizeof(buffer), "DRUM_FILL_START\n");
      f.print(buffer);
      for (auto& seqNote : DrumFillSequencer) {
        snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d,%d\n", seqNote.note, seqNote.holdTime, seqNote.offset, seqNote.velocity, seqNote.relativeOctave);
        f.print(buffer);
      }
      snprintf(buffer, sizeof(buffer), "DRUM_FILL_END\n");
      f.print(buffer);
    }

    if (DrumEndSequencer.size() > 0) {
      snprintf(buffer, sizeof(buffer), "DRUM_END_START\n");
      f.print(buffer);
      for (auto& seqNote : DrumEndSequencer) {
        snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d,%d\n", seqNote.note, seqNote.holdTime, seqNote.offset, seqNote.velocity, seqNote.relativeOctave);
        f.print(buffer);
      }
      snprintf(buffer, sizeof(buffer), "DRUM_END_END\n");
      f.print(buffer);
    }

    //save guitar patterns
    if (!saveToBackup) {
      if (SequencerPatternA.size() > 0) {
        snprintf(buffer, sizeof(buffer), "GUITAR_A_START\n");
        f.print(buffer);
        for (auto& seqNote : SequencerPatternA) {
          snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d,%d\n", seqNote.note, seqNote.holdTime, seqNote.offset, seqNote.velocity, seqNote.relativeOctave);
          f.print(buffer);
        }
        snprintf(buffer, sizeof(buffer), "GUITAR_A_END\n");
        f.print(buffer);
      }
      if (SequencerPatternB.size() > 0) {
        snprintf(buffer, sizeof(buffer), "GUITAR_B_START\n");
        f.print(buffer);
        for (auto& seqNote : SequencerPatternB) {
          snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d,%d\n", seqNote.note, seqNote.holdTime, seqNote.offset, seqNote.velocity, seqNote.relativeOctave);
          f.print(buffer);
        }
        snprintf(buffer, sizeof(buffer), "GUITAR_B_END\n");
        f.print(buffer);
      }
      if (SequencerPatternC.size() > 0) {
        snprintf(buffer, sizeof(buffer), "GUITAR_C_START\n");
        f.print(buffer);
        for (auto& seqNote : SequencerPatternC) {
          snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d,%d\n", seqNote.note, seqNote.holdTime, seqNote.offset, seqNote.velocity, seqNote.relativeOctave);
          f.print(buffer);
        }
        snprintf(buffer, sizeof(buffer), "GUITAR_C_END\n");
        f.print(buffer);
      }
    }
    //load bass pattern
    if (BassSequencerPattern.size() > 0) {
      snprintf(buffer, sizeof(buffer), "BASS_START\n");
      f.print(buffer);
      for (auto& seqNote : BassSequencerPattern) {
        snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d,%d\n", seqNote.note, seqNote.holdTime, seqNote.offset, seqNote.velocity, seqNote.relativeOctave);
        f.print(buffer);
      }
      snprintf(buffer, sizeof(buffer), "BASS_END\n");
      f.print(buffer);
    }

    if (AccompanimentSequencerPattern.size() > 0) {
      snprintf(buffer, sizeof(buffer), "ACCOMPANIMENT_START\n");
      f.print(buffer);
      for (auto& seqNote : AccompanimentSequencerPattern) {
        snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d,%d\n", seqNote.note, seqNote.holdTime, seqNote.offset, seqNote.velocity, seqNote.relativeOctave);
        f.print(buffer);
      }
      snprintf(buffer, sizeof(buffer), "BASS_END\n");
      f.print(buffer);
    }
    f.print(buffer);
  } else {
    if (debug)
      Serial.println("Error saving pattern config!");
    return false;
  }
  f.close();
  return true;
}


template<typename SerialType>
bool decodeCmd(SerialType& serialPort, String cmd, std::vector<String>* params) {
  char buffer[64];
  bool bTemp = false;
  if (debug) {
    Serial.printf("Serial command received is %s\n", cmd.c_str());
  }
  if (cmd == "MEML") {
    printMemoryUsage();
    serialPort.write("OK00\r\n");
    // Match found
  } else if (cmd == "ISKB") {
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", isKeyboard ? 1 : 0);
    serialPort.write(buffer);
  } else if (cmd == "DEVI") {

    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", DEVICE_TYPE);
    serialPort.write(buffer);
    // Match found
  } else if (cmd == "SAVE") {
    if (saveSettings()) {
      serialPort.write("OK00\r\n");
    } else {
      serialPort.write("ER00\r\n");
    }
    // Match found
  } else if (cmd == "SAVP") {
    if (savePatternFiles(backingState != 0)) {
      serialPort.write("OK00\r\n");
    } else {
      serialPort.write("ER00\r\n");
    }
    // Match found
  } else if (cmd == "STOP") {
    noteAllOff();
    serialPort.write("OK00\r\n");
  } else if (cmd == "RSTP")  // RESET Please
  {

    noteAllOff();
    prepareConfig();

    prepareNeck();

    preparePatterns();
    if (!loadSettings()) {
      if (debug) {
        Serial.println("Error! Failed to open file for reading.");
      }
    }
    if (!loadPatternFiles(false)) {
      if (debug) {
        Serial.println("Error! Failed to open pattern file for reading.");
      }
    }

    setNewBPM(presetBPM[preset]);
    serialPort.write("OK00\r\n");
  } else if (cmd == "RCFG")  //read conffig
  {
    if (params->size() == 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) == 0) {
      printFileContents("config.ini", true);
    } else if (atoi(params->at(0).c_str()) == 1) {
      printFileContents("config2.ini", true);
    } else {
      serialPort.write("ER01\r\n");
    }
    serialPort.write("OK00\r\n");
  } else if (cmd == "RINI")  //read config but through serial debug
  {
    if (params->size() == 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) == 0) {
      bTemp = printFileContents("config.ini", false);
    } else if (atoi(params->at(0).c_str()) == 1) {
      bTemp = printFileContents("config2.ini", false);
    } else {
      serialPort.write("ER01\r\n");
      return true;
    }
    if (!bTemp) {
      serialPort.write("ER02\r\n");
      return true;
    }
    serialPort.write("OK00\r\n");

  } else if (cmd == "LOAD")  //reload config files
  {
    if (loadSettings()) {
      serialPort.write("OK00\r\n");
    } else {
      serialPort.write("ER00\r\n");
    }
  }
  if (cmd == "LODP") {
    if (loadPatternFiles(false)) {
      serialPort.write("OK00\r\n");
    } else {
      serialPort.write("ER00\r\n");
    }
    // Match found
  }
  //pattern handling start
  else if (cmd == "GCLR")  //Guitar data clear
  {
    if (params->size() != 1)  //missing parameter
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) >= MAX_CUSTOM_PATTERNS || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");  //invalid parameter
      return true;
    }
    getGuitarPattern(atoi(params->at(0).c_str()))->clear();
    serialPort.write("OK00\r\n");
  } else if (cmd == "DCLR")  //Drum data clear
  {
    if (params->size() != 1)  //missing parameter
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > DRUM_END_ID || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");  //invalid parameter
      return true;
    }
    getDrumPattern(atoi(params->at(0).c_str()))->clear();
    serialPort.write("OK00\r\n");
  } else if (cmd == "BCLR")  //Bass data clear
  {
    if (params->size() != 1)  //missing parameter
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_BASS_PATTERNS || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");  //invalid parameter
      return true;
    }
    getBassPattern(atoi(params->at(0).c_str()))->clear();
    serialPort.write("OK00\r\n");
  } else if (cmd == "ACLR")  //Accompaniment data clear
  {
    if (params->size() != 1)  //missing parameter
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_ACCOMPANIMENT_PATTERNS || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");  //invalid parameter
      return true;
    }
    getAccompanimentPattern(atoi(params->at(0).c_str()))->clear();
    serialPort.write("OK00\r\n");
  } else if (cmd == "DADD")  //Drum data add
  {
    if (params->size() != 6)  //missing parameter
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > DRUM_END_ID || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");  //invalid parameter
      return true;
    }
    //note
    if (atoi(params->at(1).c_str()) > REST_NOTE || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER02\r\n");  //invalid parameter
      return true;
    }
    //offset
    if (atoi(params->at(2).c_str()) > MAX_OFFSET || atoi(params->at(2).c_str()) < 0) {
      serialPort.write("ER03\r\n");  //invalid parameter
      return true;
    }
    //holdtime
    if (atoi(params->at(3).c_str()) > MAX_HOLDTIME || atoi(params->at(3).c_str()) < 0) {
      serialPort.write("ER04\r\n");  //invalid parameter
      return true;
    }
    //velocity
    if (atoi(params->at(4).c_str()) > MAX_VELOCITY || atoi(params->at(4).c_str()) < 0) {
      serialPort.write("ER05\r\n");  //invalid parameter
      return true;
    }
    if (atoi(params->at(5).c_str()) > MAX_RELATIVE_OFFSET || atoi(params->at(5).c_str()) < MIN_RELATIVE_OFFSET) {
      serialPort.write("ER06\r\n");  //invalid parameter
      return true;
    }

    addEntryToPattern(getDrumPattern(atoi(params->at(0).c_str())), atoi(params->at(1).c_str()), atoi(params->at(2).c_str()), atoi(params->at(3).c_str()), atoi(params->at(4).c_str()), atoi(params->at(5).c_str()), DRUM_CHANNEL);
    serialPort.write("OK00\r\n");
  } else if (cmd == "GADD")  //Guitar data add
  {
    if (params->size() != 6)  //missing parameter
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_CUSTOM_PATTERNS || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");  //invalid parameter
      return true;
    }
    //note
    if (atoi(params->at(1).c_str()) > REST_NOTE || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER02\r\n");  //invalid parameter
      return true;
    }
    //offset
    if (atoi(params->at(2).c_str()) > MAX_OFFSET || atoi(params->at(2).c_str()) < 0) {
      serialPort.write("ER03\r\n");  //invalid parameter
      return true;
    }
    //holdtime
    if (atoi(params->at(3).c_str()) > MAX_HOLDTIME || atoi(params->at(3).c_str()) < 0) {
      serialPort.write("ER04\r\n");  //invalid parameter
      return true;
    }
    //velocity
    if (atoi(params->at(4).c_str()) > MAX_VELOCITY || atoi(params->at(4).c_str()) < 0) {
      serialPort.write("ER05\r\n");  //invalid parameter
      return true;
    }
    if (atoi(params->at(5).c_str()) > MAX_RELATIVE_OFFSET || atoi(params->at(5).c_str()) < MIN_RELATIVE_OFFSET) {
      serialPort.write("ER06\r\n");  //invalid parameter
      return true;
    }
    addEntryToPattern(getGuitarPattern(atoi(params->at(0).c_str())), atoi(params->at(1).c_str()), atoi(params->at(2).c_str()), atoi(params->at(3).c_str()), atoi(params->at(4).c_str()), atoi(params->at(5).c_str()), GUITAR_CHANNEL);
    serialPort.write("OK00\r\n");
  } else if (cmd == "BADD")  //Bass data add
  {
    if (params->size() != 6)  //missing parameter
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > 0 || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");  //invalid parameter
      return true;
    }
    //note
    if (atoi(params->at(1).c_str()) > REST_NOTE || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER02\r\n");  //invalid parameter
      return true;
    }
    //offset
    if (atoi(params->at(2).c_str()) > MAX_OFFSET || atoi(params->at(2).c_str()) < 0) {
      serialPort.write("ER03\r\n");  //invalid parameter
      return true;
    }
    //holdtime
    if (atoi(params->at(3).c_str()) > MAX_HOLDTIME || atoi(params->at(3).c_str()) < 0) {
      serialPort.write("ER04\r\n");  //invalid parameter
      return true;
    }
    //velocity
    if (atoi(params->at(4).c_str()) > MAX_VELOCITY || atoi(params->at(4).c_str()) < 0) {
      serialPort.write("ER05\r\n");  //invalid parameter
      return true;
    }
    if (atoi(params->at(5).c_str()) > MAX_RELATIVE_OFFSET || atoi(params->at(5).c_str()) < MIN_RELATIVE_OFFSET) {
      serialPort.write("ER06\r\n");  //invalid parameter
      return true;
    }
    addEntryToPattern(getBassPattern(atoi(params->at(0).c_str())), atoi(params->at(1).c_str()), atoi(params->at(2).c_str()), atoi(params->at(3).c_str()), atoi(params->at(4).c_str()), atoi(params->at(5).c_str()), BASS_CHANNEL);
    serialPort.write("OK00\r\n");
  } else if (cmd == "AADD")  //Accompaniment data add
  {
    if (params->size() != 6)  //missing parameter
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > 0 || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");  //invalid parameter
      return true;
    }
    //note
    if (atoi(params->at(1).c_str()) > REST_NOTE || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER02\r\n");  //invalid parameter
      return true;
    }
    //offset
    if (atoi(params->at(2).c_str()) > MAX_OFFSET || atoi(params->at(2).c_str()) < 0) {
      serialPort.write("ER03\r\n");  //invalid parameter
      return true;
    }
    //holdtime
    if (atoi(params->at(3).c_str()) > MAX_HOLDTIME || atoi(params->at(3).c_str()) < 0) {
      serialPort.write("ER04\r\n");  //invalid parameter
      return true;
    }
    //velocity
    if (atoi(params->at(4).c_str()) > MAX_VELOCITY || atoi(params->at(4).c_str()) < 0) {
      serialPort.write("ER05\r\n");  //invalid parameter
      return true;
    }
    if (atoi(params->at(5).c_str()) > MAX_RELATIVE_OFFSET || atoi(params->at(5).c_str()) < MIN_RELATIVE_OFFSET) {
      serialPort.write("ER06\r\n");  //invalid parameter
      return true;
    }
    addEntryToPattern(getAccompanimentPattern(atoi(params->at(0).c_str())), atoi(params->at(1).c_str()), atoi(params->at(2).c_str()), atoi(params->at(3).c_str()), atoi(params->at(4).c_str()), atoi(params->at(5).c_str()), ACCOMPANIMENT_CHANNEL);
    serialPort.write("OK00\r\n");
  } else if (cmd == "GPRN")  //Guitar data Print
  {
    if (params->size() != 1)  //missing parameter
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_CUSTOM_PATTERNS || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");  //invalid parameter
      return true;
    }
    sendPatternData(getGuitarPattern(atoi(params->at(0).c_str())), true);
    //serialPort.write("OK00\r\n");
  } else if (cmd == "BPRN")  //Bass data Print
  {
    if (params->size() != 1)  //missing parameter
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_BASS_PATTERNS || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");  //invalid parameter
      return true;
    }
    sendPatternData(getBassPattern(atoi(params->at(0).c_str())), true);
    //serialPort.write("OK00\r\n");
  } else if (cmd == "DPRN")  //Drum data Print
  {
    if (params->size() != 1)  //missing parameter
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > DRUM_END_ID || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");  //invalid parameter
      return true;
    }
    sendPatternData(getDrumPattern(atoi(params->at(0).c_str())), true);
    //serialPort.write("OK00\r\n");
  } else if (cmd == "APRN")  //Accompaniment data Print
  {
    if (params->size() != 1)  //missing parameter
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) >= MAX_ACCOMPANIMENT_PATTERNS || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");  //invalid parameter
      return true;
    }
    sendPatternData(getAccompanimentPattern(atoi(params->at(0).c_str())), true);
    //serialPort.write("OK00\r\n");
  }
  //pattern handling end
  else if (cmd == "CONN") {
    if (params->size() != 1)  //missing parameter
    {
      if (debug) {
        Serial.printf("Error in received CONN\n");
      }
      return true;
    }
    if (atoi(params->at(0).c_str()) == 0) {
      BLEConnected = true;
      if (debug) {
        Serial.printf("BTClassicConnected = %d\n", BLEConnected ? 1 : 0);
      }
      return true;
    } else {
      BTClassicConnected = true;
      if (debug) {
        Serial.printf("BTClassicConnected = %d\n", BTClassicConnected ? 1 : 0);
      }
      return true;
    }
  } else if (cmd == "DCDC") {
    if (params->size() != 1)  //missing parameter
    {
      if (debug) {
        Serial.printf("Error in received DCDC \n");
      }
      return true;
    }
    if (atoi(params->at(0).c_str()) == 0) {
      BLEConnected = false;
      if (debug) {
        Serial.printf("BLEConnected = %d\n", BLEConnected ? 1 : 0);
      }
      return true;
    } else {
      BTClassicConnected = false;
      if (debug) {
        Serial.printf("BTClassicConnected = %d\n", BTClassicConnected ? 1 : 0);
      }
      return true;
    }
  } else if (cmd == "PACO") {
    
    char pData6[] = { 0xF5,0x55,0x00,0x06,0x00,0x13,0x14,0x6B,0x00,0x07


  };

    Serial4.write(pData6, sizeof(pData6));  //only last is lit
    serialPort.write("OK00\r\n");
  } else if (cmd == "OCTV") {
    if (params->size() == 0)  //missing parameter
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) >= 6 || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");  //invalid parameter
      return true;
    }
    size_t len = 0;
    const char* oData = hexToOctave(atoi(params->at(0).c_str()), len);
    Serial4.write(oData, len);

    serialPort.write("OK00\r\n");
  } else if (cmd == "PRSW")  //Set Current Preset
  {
    if (params->size() == 0)  //missing parameter
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) >= MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");  //invalid parameter
      return true;
    }
    uint8_t newPreset = atoi(params->at(0).c_str());
    if (newPreset != preset) {
      preset = newPreset;
      presetChanged();
    }
    serialPort.write("OK00\r\n");
  } else if (cmd == "PRSR")  //Read Current Preset
  {
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", preset);
    serialPort.write(buffer);
  } else if (cmd == "DBGW")  //Set Debug setting
  {
    if (params->size() == 0)  //missing parameter
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > 1 || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");  //invalid parameter
      return true;
    }
    debug = atoi(params->at(0).c_str()) > 0;
    serialPort.write("OK00\r\n");
  } else if (cmd == "DBGR")  //Debug setting read
  {
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", debug ? 1 : 0);
    serialPort.write(buffer);
  }


  else if (cmd == "PASW")  //Pass Through Mode
  {
    if (params->size() == 0)  //missing parameter
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > 1 || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");  //invalid parameter
      return true;
    }
    passThroughSerial = atoi(params->at(0).c_str()) > 0;
    serialPort.write("OK00\r\n");
  } else if (cmd == "PASR")  //Debug setting read
  {
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", passThroughSerial ? 1 : 0);
    serialPort.write(buffer);
  }

  else if (cmd == "SPCW")  //stopSoundsOnPresetChange
  {
    if (params->size() == 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > 1 || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");  //invalid parameter
      return true;
    }
    stopSoundsOnPresetChange = atoi(params->at(0).c_str()) > 0;
    serialPort.write("OK00\r\n");
  } else if (cmd == "SPCR")  //stopSoundsOnPresetChange read
  {
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", stopSoundsOnPresetChange ? 1 : 0);
    serialPort.write(buffer);
  }

  else if (cmd == "HBCW")  //handleOmnichordBackingChange
  {
    if (params->size() == 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_BACKING_TYPE || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");  //invalid parameter
      return true;
    }
    bool restartMusic = false;
    if (drumState != DrumStopped) {
      restartMusic = true;
      noteAllOff();
    }
    handleOmnichordBackingChange(atoi(params->at(0).c_str()));  //change backing from standard to omnichord
    if (restartMusic) {
      if (DrumIntroSequencer.size() > 0) {
        drumState = DrumIntro;
        drumNextState = DrumNone;
        prepareDrumSequencer();
      } else {
        drumState = DrumLoop;
        drumNextState = DrumNone;
        prepareDrumSequencer();
      }
      bassStart = true;
      accompanimentStart = true;
    }
    serialPort.write("OK00\r\n");
  } else if (cmd == "HBCR")  //backingState
  {
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", backingState);
    serialPort.write(buffer);
  } else if (cmd == "CLKW")  //midiClockEnable
  {
    if (params->size() == 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > 1 || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");  //invalid parameter
      return true;
    }
    midiClockEnable = atoi(params->at(0).c_str()) > 0;
    serialPort.write("OK00\r\n");
  } else if (cmd == "CLKR")  //midiClockEnable
  {
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", midiClockEnable ? 1 : 0);
    serialPort.write(buffer);
  } else if (cmd == "BPMW")  //presetBPM write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > MAX_BPM || atoi(params->at(1).c_str()) < MIN_BPM) {
      serialPort.write("ER00\r\n");
      return true;
    }
    presetBPM[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str());
    if (preset == atoi(params->at(0).c_str())) {
      setNewBPM(presetBPM[atoi(params->at(0).c_str())]);
    }
    serialPort.write("OK00\r\n");
  } else if (cmd == "BPMR")  //presetBPM Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", presetBPM[atoi(params->at(0).c_str())]);
    serialPort.write(buffer);
  }

  else if (cmd == "OMMW")  //omniChordModeGuitar write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > (int)OmniChordGuitarType || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    int oldMode = omniChordModeGuitar[atoi(params->at(0).c_str())];
    omniChordModeGuitar[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str());
    if (oldMode != atoi(params->at(1).c_str())) {
      detector.transposeReset();
    }
    serialPort.write("OK00\r\n");
  } else if (cmd == "OMMR")  //omniChordModeGuitar Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", omniChordModeGuitar[atoi(params->at(0).c_str())]);
    serialPort.write(buffer);
  } else if (cmd == "MWLW")  //muteWhenLetGo write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > 1 || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    muteWhenLetGo[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str()) > 0;
    serialPort.write("OK00\r\n");
  }

  else if (cmd == "MWLR")  //muteWhenLetGo Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", muteWhenLetGo[atoi(params->at(0).c_str())] ? 1 : 0);
    serialPort.write(buffer);
  } else if (cmd == "ISCW")  //ignoreSameChord write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > 1 || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    ignoreSameChord[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str()) > 0;
    serialPort.write("OK00\r\n");
  } else if (cmd == "ISCR")  //ignoreSameChord Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", ignoreSameChord[atoi(params->at(0).c_str())] ? 1 : 0);
    serialPort.write(buffer);
  } else if (cmd == "CH_W")  //chordHold write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > 1 || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    chordHold[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str()) > 0;
    serialPort.write("OK00\r\n");
  } else if (cmd == "CH_R")  //chordHold Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", chordHold[atoi(params->at(0).c_str())] ? 1 : 0);
    serialPort.write(buffer);
  }

  else if (cmd == "UTSW")  //useToggleSustain write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > 1 || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    useToggleSustain[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str()) > 0;
    serialPort.write("OK00\r\n");
  } else if (cmd == "UTSR")  //useToggleSustain Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", useToggleSustain[atoi(params->at(0).c_str())] ? 1 : 0);
    serialPort.write(buffer);
  }

  else if (cmd == "STSW")  //strumSeparation write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > MAX_STRUM_SEPARATION || atoi(params->at(1).c_str()) < MIN_STRUM_SEPARATION) {
      serialPort.write("ER00\r\n");
      return true;
    }
    strumSeparation[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str());
    serialPort.write("OK00\r\n");
  } else if (cmd == "STSR")  //strumSeparation Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", strumSeparation[atoi(params->at(0).c_str())]);
    serialPort.write(buffer);
  } else if (cmd == "SCSW")  //simpleChordSetting write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > MAX_STRUM_STYLE || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER02\r\n");
      return true;
    }
    simpleChordSetting[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str());

    serialPort.write("OK00\r\n");
  } else if (cmd == "SCSR")  //simpleChordSetting Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", simpleChordSetting[atoi(params->at(0).c_str())]);
    serialPort.write(buffer);
  }

  else if (cmd == "OTOW")  //omniKBTransposeOffset write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > OMNICHORD_TRANSPOSE_MAX || atoi(params->at(1).c_str()) < OMNICHORD_TRANSPOSE_MIN) {
      serialPort.write("ER00\r\n");
      return true;
    }
    omniKBTransposeOffset[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str());
    bool hadNotes = omniChordNewNotes.size() > 0;
    omniChordNewNotes.clear();
    if (hadNotes) {
      int lastB = -1;
      if (neckButtonPressed != -1) {
        lastB = neckButtonPressed;
      } else if (lastValidNeckButtonPressed != -1) {
        lastB = lastValidNeckButtonPressed;
      } else {
      }
      if (lastB != -1) {
        generateOmnichordNoteMap(lastB);
      }
    }
    serialPort.write("OK00\r\n");
  } else if (cmd == "OTOR")  //omniKBTransposeOffset Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", omniKBTransposeOffset[atoi(params->at(0).c_str())]);
    serialPort.write(buffer);
  } else if (cmd == "GTRW")  //guitarTranspose write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > GUITAR_TRANSPOSE_MAX || atoi(params->at(1).c_str()) < GUITAR_TRANSPOSE_MIN) {
      serialPort.write("ER00\r\n");
      return true;
    }
    noteAllOff();
    guitarTranspose[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str());
    serialPort.write("OK00\r\n");
  } else if (cmd == "GTRR")  //guitarTranspose Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", guitarTranspose[atoi(params->at(0).c_str())]);
    serialPort.write(buffer);
  }

  else if (cmd == "GMSW")  //muteSeparation write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > GUITAR_PALMING_DELAY_MAX || atoi(params->at(1).c_str()) < GUITAR_PALMING_DELAY_MIN) {
      serialPort.write("ER00\r\n");
      return true;
    }
    muteSeparation[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str());
    serialPort.write("OK00\r\n");
  } else if (cmd == "GMSR")  //muteSeparation Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", muteSeparation[atoi(params->at(0).c_str())]);
    serialPort.write(buffer);
  } else if (cmd == "EANW")  //enableAllNotesOnChords write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > 1 || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    enableAllNotesOnChords[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str()) == 1;
    serialPort.write("OK00\r\n");
  } else if (cmd == "EANR")  //enableButtonMidi Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", enableAllNotesOnChords[atoi(params->at(0).c_str())] ? 1 : 0);
    serialPort.write(buffer);
  }

  else if (cmd == "SCMW")  //isSimpleChordMode write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > 1 || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    isSimpleChordMode[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str()) == 1;
    serialPort.write("OK00\r\n");
    if (preset == atoi(params->at(0).c_str())) {
      noteAllOff();
    }
  } else if (cmd == "SCMR")  //isSimpleChordMode Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", isSimpleChordMode[atoi(params->at(0).c_str())] ? 1 : 0);
    serialPort.write(buffer);
  }

  //
  else if (cmd == "OM5W")  //properOmniChord5ths write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > 1 || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    properOmniChord5ths[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str()) == 1;
    serialPort.write("OK00\r\n");
    omniChordNewNotes.clear();
    int note = lastValidNeckButtonPressed;
    if (note == -1) {
      note = lastValidNeckButtonPressed;
    }
    if (note != -1) {
      MidiMessage msg = hexToNote(note);
      int offset = 0;

      std::vector<uint8_t> chordNotes = getActualAssignedChord(msg.note).getChords().getCompleteChordNotes();  //get notes

      if (properOmniChord5ths[preset]) {
        for (uint8_t i = 0; i < chordNotes.size(); i++) {
          //if (chordNotes[i] == chordNotes[0] + 7 )
          if (i == 2)  // assumed to be always done for 3rd note/5ths
          {
            chordNotes[i] -= SEMITONESPEROCTAVE;
          }
          //chordNotes[i] += 12; //correct too low
        }
      }
      while (omniChordNewNotes.size() < omniChordOrigNotes.size()) {
        if (omniChordNewNotes.size() != 0) {
          offset += SEMITONESPEROCTAVE;
        }
        for (uint8_t i = 0; i < chordNotes.size() && omniChordNewNotes.size() < omniChordOrigNotes.size(); i++) {
          omniChordNewNotes.push_back(chordNotes[i] + offset + SEMITONESPEROCTAVE * omniKBTransposeOffset[preset]);
        }
      }
    }
  }


  else if (cmd == "OM5R")  //properOmniChord5ths Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", properOmniChord5ths[atoi(params->at(0).c_str())] ? 1 : 0);
    serialPort.write(buffer);
  } else if (cmd == "EBMW")  //enableButtonMidi write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > 1 || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    enableButtonMidi[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str()) == 1;
    serialPort.write("OK00\r\n");
  } else if (cmd == "EBMR")  //enableButtonMidi Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", enableButtonMidi[atoi(params->at(0).c_str())] ? 1 : 0);
    serialPort.write(buffer);
  }

  else if (cmd == "BENW")  //BassEnabled write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > 1 || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    bassEnabled[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str()) == 1;
    serialPort.write("OK00\r\n");
  } else if (cmd == "BENR")  //BassEnabled Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", bassEnabled[atoi(params->at(0).c_str())] ? 1 : 0);
    serialPort.write(buffer);
  }

  else if (cmd == "AENW")  //accompanimentEnabled write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > 1 || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    accompanimentEnabled[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str()) == 1;
    serialPort.write("OK00\r\n");
  } else if (cmd == "AENR")  //accompanimentEnabled Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", accompanimentEnabled[atoi(params->at(0).c_str())] ? 1 : 0);
    serialPort.write(buffer);
  }

  else if (cmd == "ASDW")  //alternateDirection
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > 1 || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    alternateDirection[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str()) == 1;
    serialPort.write("OK00\r\n");
  } else if (cmd == "ASDR")  //alternateDirection Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", alternateDirection[atoi(params->at(0).c_str())] ? 1 : 0);
    serialPort.write(buffer);
  }

  else if (cmd == "DENW")  //drumsEnabled write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > 1 || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    drumsEnabled[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str()) == 1;
    serialPort.write("OK00\r\n");
  } else if (cmd == "DENR")  //drumsEnabled Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", drumsEnabled[atoi(params->at(0).c_str())] ? 1 : 0);
    serialPort.write(buffer);
  }

  //simple
  //std::vector<uint16_t> QUARTERNOTETICKS;

  //complex
  //std::vector<std::vector<AssignedPattern>> assignedFretPatternsByPreset;

  else if (cmd == "NASW")  //neckAssignments write
  {
    if (params->size() < 5) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");
      return true;
    }
    //row
    if (atoi(params->at(1).c_str()) > NECK_ROWS || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER02\r\n");
      return true;
    }
    //column
    if (atoi(params->at(2).c_str()) > NECK_COLUMNS || atoi(params->at(2).c_str()) < 0) {
      serialPort.write("ER03\r\n");
      return true;
    }
    if (atoi(params->at(3).c_str()) > (int)B_NOTE || atoi(params->at(3).c_str()) < (int)C_NOTE) {
      serialPort.write("ER04\r\n");
      return true;
    }
    if (atoi(params->at(4).c_str()) > (int)clusterChordType || atoi(params->at(4).c_str()) < (int)majorChordType) {
      serialPort.write("ER05\r\n");
      return true;
    }
    neckAssignments[atoi(params->at(0).c_str())][atoi(params->at(1).c_str()) * NECK_COLUMNS + atoi(params->at(2).c_str())].key = (Note)atoi(params->at(3).c_str());
    neckAssignments[atoi(params->at(0).c_str())][atoi(params->at(1).c_str()) * NECK_COLUMNS + atoi(params->at(2).c_str())].chordType = (ChordType)atoi(params->at(4).c_str());

    //update pattern's root note
    //todo check
    assignedFretPatternsByPreset[atoi(params->at(0).c_str())][atoi(params->at(1).c_str()) * NECK_COLUMNS + atoi(params->at(2).c_str())].assignedChord = getKeyboardChordNotesFromNeck(atoi(params->at(1).c_str()), atoi(params->at(2).c_str()), atoi(params->at(0).c_str()));
    assignedFretPatternsByPreset[atoi(params->at(0).c_str())][atoi(params->at(1).c_str()) * NECK_COLUMNS + atoi(params->at(2).c_str())].assignedGuitarChord = getGuitarChordNotesFromNeck(atoi(params->at(1).c_str()), atoi(params->at(2).c_str()), atoi(params->at(0).c_str()));
    assignedFretPatternsByPreset[atoi(params->at(0).c_str())][atoi(params->at(1).c_str()) * NECK_COLUMNS + atoi(params->at(2).c_str())].assignedChord.setRootNote((int)neckAssignments[atoi(params->at(0).c_str())][atoi(params->at(1).c_str()) * NECK_COLUMNS + atoi(params->at(2).c_str())].key + OMNICHORD_ROOT_START);

    serialPort.write("OK00\r\n");
    omniChordNewNotes.clear();
  } else if (cmd == "NASR")  //NeckAssignment Read
  {
    if (params->size() < 3) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    //row
    if (atoi(params->at(1).c_str()) > NECK_ROWS || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    //column
    if (atoi(params->at(2).c_str()) > NECK_COLUMNS || atoi(params->at(2).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d,%d\r\n", (int)neckAssignments[atoi(params->at(0).c_str())][atoi(params->at(1).c_str()) * NECK_COLUMNS + atoi(params->at(2).c_str())].key, (int)neckAssignments[atoi(params->at(0).c_str())][atoi(params->at(1).c_str()) * NECK_COLUMNS + atoi(params->at(2).c_str())].chordType);
    serialPort.write(buffer);
  }

  else if (cmd == "NACW")  //neckAssignments set to 1 chord
  {
    if (params->size() < 3) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");
      return true;
    }

    //column
    if (atoi(params->at(1).c_str()) > NECK_COLUMNS || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(2).c_str()) > (((int)quartalChordType) + 1) || atoi(params->at(2).c_str()) < 0) {
      serialPort.write("ER02\r\n");
      return true;
    }
    ChordType c = (ChordType)atoi(params->at(2).c_str());
    int j = atoi(params->at(1).c_str());
    for (int i = 0; i < NECK_ROWS; i++) {
      //for (int j = 0; j < NECK_COLUMNS; j++)
      {
        neckAssignments[atoi(params->at(0).c_str())][i * NECK_COLUMNS + j].chordType = (ChordType)atoi(params->at(2).c_str());
        assignedFretPatternsByPreset[atoi(params->at(0).c_str())][i * NECK_COLUMNS + j].assignedChord = getKeyboardChordNotesFromNeck(i, j, atoi(params->at(0).c_str()));
        assignedFretPatternsByPreset[atoi(params->at(0).c_str())][i * NECK_COLUMNS + j].assignedGuitarChord = getGuitarChordNotesFromNeck(i, j, atoi(params->at(0).c_str()));
        assignedFretPatternsByPreset[atoi(params->at(0).c_str())][i * NECK_COLUMNS + j].assignedChord.setRootNote((int)neckAssignments[atoi(params->at(0).c_str())][i * NECK_COLUMNS + j].key + OMNICHORD_ROOT_START);
      }
    }
    if (debug) {
      Serial.printf("Preset %d Chord changed to %s\n", atoi(params->at(0).c_str()), chordTypeToString(c));
    }
    omniChordNewNotes.clear();
    noteAllOff();
    serialPort.write("OK00\r\n");

  }

  else if (cmd == "NAPW")  //neckAssignments.customPattern write
  {
    if (params->size() < 4) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");
      return true;
    }
    //row
    if (atoi(params->at(1).c_str()) > NECK_ROWS || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER02\r\n");
      return true;
    }
    //column
    if (atoi(params->at(2).c_str()) > NECK_COLUMNS || atoi(params->at(2).c_str()) < 0) {
      serialPort.write("ER03\r\n");
      return true;
    }
    if (atoi(params->at(3).c_str()) >= MAX_CUSTOM_PATTERNS || atoi(params->at(3).c_str()) < 0) {
      serialPort.write("ER04\r\n");
      return true;
    }
    assignedFretPatternsByPreset[atoi(params->at(0).c_str())][atoi(params->at(1).c_str()) * NECK_COLUMNS + atoi(params->at(2).c_str())].customPattern = atoi(params->at(3).c_str());
    serialPort.write("OK00\r\n");
  } else if (cmd == "NAPR")  //NeckAssignmentcustom.Pattern Read
  {
    if (params->size() < 3) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    //row
    if (atoi(params->at(1).c_str()) > NECK_ROWS || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    //column
    if (atoi(params->at(2).c_str()) > NECK_COLUMNS || atoi(params->at(2).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", (int)assignedFretPatternsByPreset[atoi(params->at(0).c_str())][atoi(params->at(1).c_str()) * NECK_COLUMNS + atoi(params->at(2).c_str())].customPattern);
    serialPort.write(buffer);
  }

  else if (cmd == "NPAW")  //neckAssignments.customPattern write all
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");
      return true;
    }
    //patterns
    if (atoi(params->at(1).c_str()) >= MAX_CUSTOM_PATTERNS || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER02\r\n");
      return true;
    }
    for (int i = 0; i < NECK_ROWS; i++) {
      for (int j = 0; j < NECK_COLUMNS; j++) {
        if (debug)
          Serial.printf("At %d: %d, %d = %d\n", atoi(params->at(0).c_str()), i, j, i * NECK_COLUMNS + j);
        assignedFretPatternsByPreset[atoi(params->at(0).c_str())][i * NECK_COLUMNS + j].customPattern = atoi(params->at(1).c_str());
      }
    }
    serialPort.write("OK00\r\n");
  }

  else if (cmd == "DIDW")  //DrumPatternID write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > DRUM_END_ID || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > 65535 || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    DrumPatternID[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str());
    serialPort.write("OK00\r\n");
  } else if (cmd == "DIDR")  //DrumPatternID Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > DRUM_END_ID || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", DrumPatternID[atoi(params->at(0).c_str())]);
    serialPort.write(buffer);
  } else if (cmd == "BIDW")  //BassPatternID write
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > 65535 || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    BassPatternID = atoi(params->at(0).c_str());
    serialPort.write("OK00\r\n");
  } else if (cmd == "BIDR")  //BassPatternID Read
  {
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", BassPatternID);
    serialPort.write(buffer);
  }

  else if (cmd == "AIDW")  //AccompanimentPatternID write
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > 65535 || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    AccompanimentPatternID = atoi(params->at(0).c_str());
    serialPort.write("OK00\r\n");
  } else if (cmd == "AIDR")  //AccompanimentPatternID Read
  {
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", AccompanimentPatternID);
    serialPort.write(buffer);
  }

  else if (cmd == "GIDW")  //BassPatternID write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_CUSTOM_PATTERNS || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > 65535 || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER02\r\n");
      return true;
    }
    switch (atoi(params->at(0).c_str())) {
      case 0:
        GuitarPatternID0 = atoi(params->at(1).c_str());
        break;
      case 1:
        GuitarPatternID1 = atoi(params->at(1).c_str());
        break;
      default:
        GuitarPatternID2 = atoi(params->at(1).c_str());
    }

    serialPort.write("OK00\r\n");
  } else if (cmd == "GIDR")  //GuitarPatternID[0-2] Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) >= MAX_CUSTOM_PATTERNS || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER01\r\n");
      return true;
    }
    uint16_t temp = 0;
    switch (atoi(params->at(0).c_str())) {
      case 0:
        temp = GuitarPatternID0;
        break;
      case 1:
        temp = GuitarPatternID1;
        break;
      default:
        temp = GuitarPatternID2;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", temp);
    serialPort.write(buffer);
  }
  return true;
}


template<typename SerialType>
void checkSerialCmd(SerialType& serialPort, char* buffer, uint8_t& bufferLen) {
  std::vector<String> params;
  while (serialPort.available()) {
    char c = serialPort.read();

    // Prevent buffer overflow
    if (bufferLen < 255) {
      buffer[bufferLen++] = c;
    }

    // Look for command terminator
    if (c == '\n' && bufferLen >= 2 && buffer[bufferLen - 2] == '\r') {
      buffer[bufferLen] = '\0';  // Null-terminate the string

      // Optionally: remove trailing \r\n for cleaner parsing
      buffer[bufferLen - 2] = '\0';

      // --- Parse Command ---
      char* cmd = strtok(buffer, ",");
      if (cmd && strlen(cmd) == 4) {
        if (debug) {
          serialPort.printf("Received CMD: %s", cmd);
        }

        //int paramIndex = 1;
        char* token;
        while ((token = strtok(NULL, ",")) != NULL) {
          //serialPort.printf("Param %d: %s\n", paramIndex++, token);
          if (debug) {
            serialPort.printf(",%s", token);
          }
          params.push_back(String(token));
        }
        if (debug)
          serialPort.printf("\n");
        // Respond with OK

      } else {
        serialPort.write("ER98\r\n");
      }
      if (!decodeCmd(serialPort, String(cmd), &params)) {
        serialPort.write("ER99\r\n");
      }
      // Reset buffer for next command
      bufferLen = 0;
    }
  }
}

void gradualCancelNotes(const std::vector<uint8_t>& chordNotes, uint8_t channel) {
  uint8_t stopDelay = 0;
  //Serial.printf("gradualCancel\n");
  for (uint8_t chordNote : chordNotes) {
    for (auto& seqNote : SequencerNotes) {

      if (seqNote.note == chordNote && seqNote.channel == channel) {
        // Match found — cancel the note
        seqNote.offset = 0;  //note will not be played, hold time is set to low value to delay/force note off
        //assumes simple chord hold at this point?
        if (muteSeparation[preset] == 0) {
          seqNote.holdTime = 0;
        } else {
          seqNote.holdTime = stopDelay;  //order is always the same
          stopDelay += muteSeparation[preset];
        }
      }
    }
  }
}
void cancelGuitarChordNotes(const std::vector<uint8_t>& chordNotes) {
  // If the input chord is empty, there's nothing to cancel
  if (chordNotes.empty()) {
    return;
  }

  uint8_t stopDelay = 0;
  // For each note in the chord, look for matching notes in SequencerNotes
  for (uint8_t chordNote : chordNotes) {
    for (auto& seqNote : SequencerNotes) {

      if (seqNote.note == chordNote && seqNote.channel == GUITAR_CHANNEL) {
        // Match found — cancel the note
        seqNote.offset = 0;  //note will not be played, hold time is set to low value to delay/force note off
        //assumes simple chord hold at this point?
        if (muteSeparation[preset] == 0) {
          seqNote.holdTime = 0;
        } else {
          if (seqNote.holdTime != INDEFINITE_HOLD_TIME) {
            seqNote.holdTime = stopDelay;  //order is always the same
            stopDelay += muteSeparation[preset];
          }
        }
      }
    }
  }
}

std::vector<SequencerNote> getCurrentPattern(uint8_t button) {
  if (button < 0 || button >= NECK_COLUMNS * NECK_ROWS) {
    if (debug)
      Serial.printf("Error! Invalid button value %d\n", button);
    button = 0;
  }
  switch (getActualAssignedChord(button).customPattern) {
    case 0:
      return SequencerPatternA;
    case 1:
      return SequencerPatternB;
    default:
      return SequencerPatternC;
  }
}

//replace note place holders with actual notes from chord
//std::vector<SequencerNote> getPatternNotesFromChord(bool isGuitar, int buttonPressed)
std::vector<SequencerNote> getPatternNotesFromChord(uint8_t channel, int buttonPressed) {
  //todo check effect with other transpose
  uint8_t myOffset = guitarTranspose[preset];
  //int buttonPressed = lastValidNeckButtonPressed;
  // /Serial.printf("getPatternNotesFromChord is %d\n", buttonPressed);
  std::vector<SequencerNote> result;
  if (channel == BASS_CHANNEL) {
    result = BassSequencerPattern;
  } else if (channel == GUITAR_CHANNEL) {
    result = getCurrentPattern(buttonPressed);
  } else {
    result = AccompanimentSequencerPattern;
  }
  std::vector<uint8_t> chordNotesA;
  if (channel != BASS_CHANNEL) {
    chordNotesA = getActualAssignedChord(buttonPressed).getChords().getCompleteChordNotes3();  //get notes
  } else {
    chordNotesA = getActualAssignedChord(buttonPressed).getChords().getCompleteChordNotes();  //get notes
  }
  for (uint8_t i = 0; i < chordNotesA.size(); i++) {
    for (uint8_t j = 0; j < result.size(); j++) {
      if (result[j].note == i) {
        result[j].note = chordNotesA[i] + result[j].relativeOctave * SEMITONESPEROCTAVE + myOffset;
      }
    }
  }
  return result;
}

//for manual strum, it will prepare the staggered notes and update the strum sequence
//for auto strum, it will assign the notes and add to the strum sequence
void buildAutoManualNotes(bool reverse, int buttonPressed) {
  bool isManual = false;
  if (buttonPressed < 0 || buttonPressed >= NECK_COLUMNS * NECK_ROWS) {
    if (debug)
      Serial.printf("Error! Invalid button value %d\n", buttonPressed);
    return;  // error!
  }
  //if staggered notes is empty
  //if manual
  uint16_t currentOrder = 0;
  if (simpleChordSetting[preset] == ManualStrum) {
    isManual = true;
    //if empty, load next
    if (StaggeredSequencerNotes.size() == 0) {
      //clean up pattern if empty and remove rests
      StaggeredSequencerNotes = getPatternNotesFromChord(GUITAR_CHANNEL, buttonPressed);
      auto it = StaggeredSequencerNotes.begin();
      while (it != StaggeredSequencerNotes.end()) {
        if (it->note == REST_NOTE) {
          // Remove the note and update the iterator
          it = StaggeredSequencerNotes.erase(it);
        } else {
          ++it;
        }
      }
      //start with current first offset
    }
    if (StaggeredSequencerNotes.size() == 0) {
      if (debug)
        Serial.printf("Error! All the contests of staggered were rests\n");
      return;  // error!
    }
    //get whatever is first
    currentOrder = StaggeredSequencerNotes[0].offset;

    uint16_t lastOffset = 255;

    //mute all notes before clearing, todo see if really needed or be simplified
    //turn off currently playing notes
    for (uint8_t i = 0; i < SequencerNotes.size(); i++) {
      if (SequencerNotes[i].note > 12) {
        sendNoteOff(GUITAR_CHANNEL, SequencerNotes[i].note);
      }
    }
    //Clear Sequencer notes
    //
    SequencerNotes.clear();
    //get all notes of the current order. This will find the next item
    for (uint8_t i = 0; i < StaggeredSequencerNotes.size(); i++) {
      if (StaggeredSequencerNotes[i].offset != currentOrder) {
        lastOffset = i;  //know limit
        break;
      }
    }
    //if not found, meaning it is the last item, use size as stop value
    if (lastOffset == 255) {
      lastOffset = StaggeredSequencerNotes.size();
    }
    reverse = false;
    uint8_t separation = 0;
    //apply strum separation
    if (!reverse) {
      for (int i = 0; i < lastOffset; i++) {
        //if next part, quit
        if (StaggeredSequencerNotes[i].offset != currentOrder) {
          lastOffset = i;  //know limit
          break;
        }
        //apply separation per note
        if (StaggeredSequencerNotes[i].note > 12) {
          StaggeredSequencerNotes[i].offset += separation;
          separation += strumSeparation[preset];
          SequencerNotes.push_back(StaggeredSequencerNotes[i]);
        }
      }
    } else {
      for (int i = lastOffset - 1; i >= 0; i--) {
        if (StaggeredSequencerNotes[i].offset != currentOrder) {
          lastOffset = i;  //know limit
          break;
        }
        if (StaggeredSequencerNotes[i].note > 12) {
          //StaggeredSequencerNotes[i].offset += separation;
          //separation += strumSeparation[preset];
          SequencerNotes.push_back(StaggeredSequencerNotes[i]);
        }
      }
    }
    //erase last pattern
    StaggeredSequencerNotes.erase(StaggeredSequencerNotes.begin(), StaggeredSequencerNotes.begin() + lastOffset);
  } else {
    SequencerNotes = getCurrentPattern(buttonPressed);
  }

  //basically adjust offset and holdtime for manual strum and get the notes typically just gets 3 notes at
  //todo check if ths is better than 3 notes only
  //std::vector<uint8_t> chordNotes = assignedFretPatternsByPreset[preset][buttonPressed].assignedChord.getCompleteChordNotes3();
  std::vector<uint8_t> chordNotes;
  if (enableAllNotesOnChords[preset]) {
    chordNotes = getActualAssignedChord(buttonPressed).assignedChord.getCompleteChordNotes(true);
  } else {
    chordNotes = getActualAssignedChord(buttonPressed).assignedChord.getCompleteChordNotes3(true);
  }
  for (uint8_t i = 0; i < SequencerNotes.size(); i++) {
    if (isManual)  // adds virtual strumming delay
    {
      SequencerNotes[i].offset = 1 + strumSeparation[preset] * i;
      SequencerNotes[i].holdTime = INDEFINITE_HOLD_TIME;  //chord hold like simple mode
    } else {
      //nothing in theory as it is setup properly
    }
    if (SequencerNotes[i].note < chordNotes.size()) {
      SequencerNotes[i].note = chordNotes[SequencerNotes[i].note];
    } else {
      //SequencerNotes[i].note = -1; //should note be played //do nothing
    }
  }
}

// This function builds a list of SequencerNotes from a given chord.
// If 'reverse' is true, the notes are added in reverse order.
void buildGuitarSequencerNotes(const std::vector<uint8_t>& chordNotes, bool reverse = false) {

  uint8_t currentOffset = 1;
  std::vector<SequencerNote> SequencerNoteTemp;
  if (reverse) {
    // Loop through the chord notes in reverse
    for (auto it = chordNotes.rbegin(); it != chordNotes.rend(); ++it) {
      SequencerNote note(*it, INDEFINITE_HOLD_TIME, currentOffset, GUITAR_CHANNEL);
      note.note += guitarTranspose[preset];
      lastGuitarNotes.push_back(note.note);
      SequencerNoteTemp.push_back(note);
      currentOffset += strumSeparation[preset];
    }
  } else {
    // Loop through the chord notes in normal order
    for (uint8_t midiNote : chordNotes) {
      SequencerNote note(midiNote, INDEFINITE_HOLD_TIME, currentOffset, GUITAR_CHANNEL);
      note.note += guitarTranspose[preset];
      lastGuitarNotes.push_back(note.note);
      SequencerNoteTemp.push_back(note);
      currentOffset += strumSeparation[preset];
    }
  }
  //check if there are invalid note offs where currently to be played is stopped by previous notes
  if (muteSeparation[preset] > 0) {
    for (auto& seqNoteNew : SequencerNoteTemp) {
      for (auto it = SequencerNotes.begin(); it != SequencerNotes.end();) {
        // new note is to be played and there is a current note with offset == 0 && holdtime that is greater than offset
        if (seqNoteNew.note == it->note &&    // same note
            it->channel == GUITAR_CHANNEL &&  //same channel
            seqNoteNew.offset < it->holdTime  //new note will play before the note is supposed to be off
        ) {
          it->holdTime = seqNoteNew.offset;  // force it to turn off before note is played again
        } else {
          ++it;
        }
      }
    }
  }
  //copy new notes to the sequencer
  for (auto& seqNoteNew : SequencerNoteTemp) {
    SequencerNotes.push_back(seqNoteNew);
  }
}

void cancelAllGuitarNotes() {
  //Serial.printf("Cancelling all guitar notes!\n");
  for (uint8_t i = 0; i < SequencerNotes.size(); i++) {
    if (SequencerNotes[i].channel == GUITAR_CHANNEL) {
      SequencerNotes[i].offset = 0;
      SequencerNotes[i].holdTime = 0;
      if (SequencerNotes[i].note > 12) {
        sendNoteOff(GUITAR_CHANNEL, SequencerNotes[i].note);
      }
    }
  }
}
int getCurrentButtonPressed() {
  if (neckButtonPressed < 0) {
    if (chordHold[preset]) {
      //Serial.printf("Paddle Current lastValid %d\n", lastValidNeckButtonPressed);
      return lastValidNeckButtonPressed;
    }
  }
  //Serial.printf("Paddle Current neckButtonPressed %d\n", neckButtonPressed);
  return neckButtonPressed;
}
int getPreviousButtonPressed() {
  if (neckButtonPressed < 0) {
    //Serial.printf("Paddle Previous lastValid2 %d\n", lastValidNeckButtonPressed2);
    return lastValidNeckButtonPressed2;
  }
  //Serial.printf("Paddle Previous lastValid1 %d\n", lastValidNeckButtonPressed);
  return lastValidNeckButtonPressed;
}

void updateButtonPressedStates() {
  if (neckButtonPressed < 0) {
    lastValidNeckButtonPressed2 = lastValidNeckButtonPressed;
  } else {
    lastValidNeckButtonPressed2 = lastValidNeckButtonPressed;
    lastValidNeckButtonPressed = neckButtonPressed;
  }
}
void checkSerialPassThrough(HardwareSerial& serialPort, char* buffer, uint8_t& bufferLen, uint8_t channel) {
  while (serialPort.available()) {
    char c = serialPort.read();
    serialPort.write(c);  // echo back to target device
  }
}
void checkSerialKB(HardwareSerial& serialPort, char* buffer, uint8_t& bufferLen, uint8_t channel) {
  // --- Step 1: Read bytes into buffer ---
  while (serialPort.available() && bufferLen < MAX_BUFFER_SIZE - 1) {
    buffer[bufferLen++] = serialPort.read();
  }
/*
   if (bufferLen > 0) {
    for (uint8_t i = 0; i < bufferLen; ++i) {
      bufferList2.push_back(buffer[i]);
    }
  }

 if (bufferList2.size() >= 8) {
    Serial.print("Dump KB1: ");
    for (size_t i = 0; i < bufferList2.size(); ++i) {
      Serial.printf("%02X ", static_cast<uint8_t>(bufferList2[i]));
    }
    Serial.println();
    bufferList2.clear(); // Reset for next batch
  }
*/
  /*
  if (bufferLen > 0) {
    Serial.print("Raw buffer: ");
    for (uint8_t i = 0; i < bufferLen; i++) {
      Serial.printf("%02x", buffer[i]);
    }
    Serial.println();
  }
*/
  if (!ignoringIdlePing) {
    //Serial.printf("Raw buffer (KB): %s\n", buffer);
  }

  // --- Step 2: If we're ignoring idle ping, skip until we see a valid start ---
  if (ignoringIdlePing) {
    while (bufferLen >= 1) {
      if (buffer[0] == 80 || buffer[0] == 0x90 || buffer[0] == 0xf5) {
        ignoringIdlePing = false;
        break;  // valid data found, resume normal parsing
      }

      // Remove 1 byte (2 hex chars) and keep scanning
      if (passThroughSerial) {
        serialPort.write(buffer, 1);
      }
      memmove(buffer, buffer + 1, bufferLen - 1);
      bufferLen -= 1;
    }

    // Still in idle ignore mode, no need to parse
    if (ignoringIdlePing) return;
  }

  // --- Step 3: Check for idle ping pattern and enter ignore mode ---
  //keyboard always sends a specific pattern when idle
  //if (bufferLen >= 12 && strncmp(buffer, "f55500282000", 12) == 0) {
  if (bufferLen >= 6 && buffer[0] == 0xf5 && buffer[1] == 0x55 && buffer[2] == 0x00 && buffer[3] == 0x28 && buffer[4] == 0x20 && buffer[5] == 0x00) {
    ignoringIdlePing = true;
    //todo if we do built-in sound, send to the device
    isKeyboard = true;
    if (passThroughSerial) {
      serialPort.write(buffer, 6);
    }
    memmove(buffer, buffer + 6, bufferLen - 6);
    bufferLen -= 6;

    return;
  }
  //handling for guitar strum attachment when strummed
  //if (bufferLen >= 16 && strncmp(buffer, "f55500042017", 12) == 0)
  if (bufferLen >= 8 && buffer[0] == 0xf5 && buffer[1] == 0x55 && buffer[2] == 0x00 && buffer[3] == 0x04 && buffer[4] == 0x20 && buffer[5] == 0x17) {
    isKeyboard = false;
    // Get the value of the last 2 bytes
    uint16_t value = 0;
    //sscanf(buffer + 6, "%04hx", &value);
    value = ((uint16_t)buffer[6] << 8) | buffer[7];

    int lastPressed = neckButtonPressed;

    if (chordHold[preset]) {
      if (lastPressed == -1 && lastValidNeckButtonPressed != -1) {
        lastPressed = lastValidNeckButtonPressed;
      }
    }
    if (value >= 0x200 && (lastPressed > -1)) {
      //strum = -1;  // up
      if (omniChordModeGuitar[preset] <= OmniChordOffGuitarType || omniChordModeGuitar[preset] == OmniChordGuitarType) {
        //if (assignedFretPatternsByPreset[preset][msg.note].getPatternStyle() == SimpleStrum)
        if (simpleChordSetting[preset] == SimpleStrum) {
          if (getPreviousButtonPressed() >= 0) {
            cancelGuitarChordNotes(getActualAssignedChord(getPreviousButtonPressed()).assignedGuitarChord);
          }
          buildGuitarSequencerNotes(getActualAssignedChord(getCurrentButtonPressed()).assignedGuitarChord, true);
        }
        //else if (assignedFretPatternsByPreset[preset][msg.note].getPatternStyle() == ManualStrum)
        else if (simpleChordSetting[preset] == ManualStrum) {
          //update guitar notes to match chord button
          int curbut = getCurrentButtonPressed();
          int lastbut = getPreviousButtonPressed();
          //Serial.printf("cur %d last %d\n", curbut, lastbut);
          if (lastbut != -1 && curbut != -1 && lastbut != curbut && curbut != lastKBPressed) {
            //todo check if no 5 is needed
            std::vector<uint8_t> chordNotesA;
            std::vector<uint8_t> chordNotesB;
            if (enableAllNotesOnChords[preset]) {
              chordNotesA = getActualAssignedChord(lastbut).getChords().getCompleteChordNotes(true);  //get notes
              chordNotesB = getActualAssignedChord(curbut).getChords().getCompleteChordNotes(true);   //get notes
            } else {
              chordNotesA = getActualAssignedChord(lastbut).getChords().getCompleteChordNotes3(true);  //get notes
              chordNotesB = getActualAssignedChord(curbut).getChords().getCompleteChordNotes3(true);   //get notes
            }
            //Serial.printf("I was supposed to change notes sample\n");
            if (SequencerNotes.size() > 0) {
              updateNotes(chordNotesA, chordNotesB, StaggeredSequencerNotes);
            }
          }

          buildAutoManualNotes(true, getCurrentButtonPressed());  //gets next manual note
          //if there is an existing set of notes, kill them
          //build get next sequence of notes from pattern
          //add to StaggeredSequencerNotes but reversed as needed
          lastKBPressed = curbut;
        } else  //autostrum
        {
          cancelAllGuitarNotes();
          buildAutoManualNotes(true, lastPressed);
        }
      } else  //omnichord mode just transposes the notes
      {
        detector.transposeUp();
      }
      //queue buttons to be played
    } else if (value >= 0x100 && lastPressed > -1) {
      //strum = 1;  //down
      if (omniChordModeGuitar[preset] <= OmniChordOffGuitarType || omniChordModeGuitar[preset] == OmniChordGuitarType) {
        //if (assignedFretPatternsByPreset[preset][msg.note].getPatternStyle() == SimpleStrum)
        if (simpleChordSetting[preset] == SimpleStrum) {
          if (getPreviousButtonPressed() >= 0) {
            cancelGuitarChordNotes(getActualAssignedChord(getPreviousButtonPressed()).assignedGuitarChord);
          }
          buildGuitarSequencerNotes(getActualAssignedChord(getCurrentButtonPressed()).assignedGuitarChord, false);
        }
        //else if (assignedFretPatternsByPreset[preset][msg.note].getPatternStyle() == ManualStrum)
        else if (simpleChordSetting[preset] == ManualStrum) {
          //update guitar notes to match chord button
          int curbut = getCurrentButtonPressed();
          int lastbut = getPreviousButtonPressed();
          if (lastbut != -1 && curbut != -1 && lastbut != curbut && curbut != lastKBPressed) {

            //todo check if no5 is better
            std::vector<uint8_t> chordNotesA;
            std::vector<uint8_t> chordNotesB;
            if (enableAllNotesOnChords[preset]) {
              chordNotesA = getActualAssignedChord(lastbut).getChords().getCompleteChordNotes(true);  //get notes
              chordNotesB = getActualAssignedChord(curbut).getChords().getCompleteChordNotes(true);   //get notes
            } else {
              chordNotesA = getActualAssignedChord(lastbut).getChords().getCompleteChordNotes3(true);  //get notes
              chordNotesB = getActualAssignedChord(curbut).getChords().getCompleteChordNotes3(true);   //get notes
            }
            if (SequencerNotes.size() > 0) {
              updateNotes(chordNotesA, chordNotesB, StaggeredSequencerNotes);
            }
          }
          lastKBPressed = curbut;
          buildAutoManualNotes(false, getCurrentButtonPressed());  //gets next manual note
        } else                                                     //autostrum
        {
          cancelAllGuitarNotes();
          buildAutoManualNotes(false, lastPressed);
        }
      } else {
        detector.transposeDown();
      }
      //queue buttons to be played
    } else if (value == 0x000) {
      //strum = 0;  //neutral
      //do nothing
    }

    if (fullPassThroughSerial) {
      serialPort.write(buffer, 8);
    }
    memmove(buffer, buffer + 8, bufferLen - 8);
    bufferLen -= 8;
    return;
  }

  // --- Step 4: Clean junk at start ---
  while (bufferLen >= 1) {
    if ((buffer[0] == 0x80 || buffer[0] == 0x90) || (bufferLen >= 1 && buffer[0] == 0xf5)) {
      break;
    }

    if (fullPassThroughSerial) {
      serialPort.write(buffer, 2);
    }

    memmove(buffer, buffer + 1, bufferLen - 1);
    bufferLen -= 1;
  }

  // --- Step 5: Main parser loop ---
  bool skipNote;
  uint8_t oldNote = 0;
  while (true) {
    bool processed = false;
    skipNote = false;
    if (bufferLen >= 3 && (buffer[0] == 0x80 || buffer[0] == 0x90)) {

      uint8_t status = buffer[0];
      uint8_t note = buffer[1];
      uint8_t vel = buffer[2];
      //if omnichord mode, use generated map
      if (omniChordModeGuitar[preset] > OmniChordOffGuitarType && (!isKeyboard || (isKeyboard && omniChordModeGuitar[preset] >= OmniChordStandardType))) {
        if (!chordHold[preset] && neckButtonPressed == -1 && omniChordNewNotes.size() > 0) {
          omniChordNewNotes.clear();
        }
        if (omniChordNewNotes.size() == 0)  //no guitar button pressed
        {
          skipNote = true;
        } else {
          //Serial.printf("Original Note is %d\n", note);
          detector.noteOn(note);
          oldNote = note;
          //lastTransposeValueDetected = detector.getBestTranspose();
          //Serial.printf("lastTransposeValueDetected = %d\n", lastTransposeValueDetected);
          note = detector.getBestNote(note);
        }
      }
      int omniTransposeOffset = 0;
      if (!isKeyboard || (isKeyboard && omniChordModeGuitar[preset] >= OmniChordStandardType)) {
        omniTransposeOffset = guitarTranspose[preset];
      }
      if (!skipNote) {
        if ((status & 0xF0) == 0x90) {
          if (isKeyboard) {
            sendNoteOn(channel, note + transpose + omniTransposeOffset, vel);
          } else  //guitar paddle has really low note velocity
          {
            sendNoteOn(channel, note + transpose + omniTransposeOffset);
          }
          noteShift ns;
          ns.assignedNote = note + transpose;
          ns.paddleNote = oldNote;
          lastOmniNotes.push_back(ns);
        } else {
          for (auto it = lastOmniNotes.begin(); it != lastOmniNotes.end(); ++it) {
            if (it->paddleNote == oldNote) {
              note = it->assignedNote;
              lastOmniNotes.erase(it);
              break;
            }
          }
          if (isKeyboard) {
            sendNoteOff(channel, note + transpose, vel);
          } else  //guitar paddle has really low note velocity
          {
            sendNoteOff(channel, note + transpose + omniTransposeOffset);
          }
        }
      }

      if (fullPassThroughSerial) {
        serialPort.write(buffer, 3);
      }

      memmove(buffer, buffer + 3, bufferLen - 3);
      bufferLen -= 3;
      processed = true;
      continue;
    } else if (bufferLen >= 2 && buffer[0] == 0xf5 && buffer[1] == 0x55) {
      bool matched = false;
      HexToControl msg;
      //for (const HexToControl& msg : hexToControl) {
      for (uint8_t i = 0; i < 6; i++) {
        msg = hexToControl(i);
        //size_t len = strlen(msg.hex);
        size_t len = strlen(msg.hex);
        size_t expectedBytes = len / 2;
        //Serial.printf("Raw buffer (KB): %s vs %s %d\n", buffer, msg.hex, len);
        //if (bufferLen >= len && strncmp(buffer, msg.hex, len) == 0) {
        if (bufferLen >= expectedBytes && isHexStringEqualToBytes(msg.hex, len, buffer, expectedBytes)) {
          if (msg.cc != 0) {
            if (msg.cc == 3 || msg.cc == 4) {
              if (playMode == 1)  //started
              {
                sendStop();
                playMode = 0;  //paused/stop
              } else {
                sendContinue();
                playMode = 1;
              }
            } else if (msg.cc == 5) {
              transpose = 0;
              if (debug) {
                Serial.printf("Reset transpose to 0\n");
              }
            } else {
              sendProgram(channel, msg.cc);
            }
          }

          if (fullPassThroughSerial) {
            serialPort.write(buffer, len);
          }

          memmove(buffer, buffer + (len / 2), bufferLen - (len / 2));
          bufferLen -= (len / 2);
          matched = true;
          break;
        }
      }
      if (matched) continue;
      break;
    } else  //clean up?
    {
      while (bufferLen >= 1) {
        if ((buffer[0] == 0x80 || buffer[0] == 0x90) || (bufferLen >= 1 && buffer[0] == 0xf5)) {
          break;
        }

        if (fullPassThroughSerial) {
          serialPort.write(buffer, 1);
        }

        memmove(buffer, buffer + 1, bufferLen - 1);
        bufferLen -= 1;
      }
    }
    if (!processed) break;
  }

  if (bufferLen > MAX_BUFFER_SIZE - 10) {
    if (passThroughSerial) {
      serialPort.write(buffer, bufferLen - 60 / 2);
    }
    memmove(buffer, buffer + bufferLen - 60 / 2, 60 / 2);
    bufferLen = 60 / 2;
  }
}

// void checkSerialG2Piano(HardwareSerial& serialPort, char* buffer, uint8_t& bufferLen) {
//   size_t last_len = 0;
//   uint8_t rootNote = 0;
//   // --- Step 1: Read bytes into buffer ---
//   while (serialPort.available() && bufferLen < MAX_BUFFER_SIZE - 1) {
//     char c = serialPort.read();
//     sprintf(&buffer[bufferLen], "%02x", (unsigned char)c);
//     bufferLen += 2;
//   }
//   buffer[bufferLen] = '\0';

//   // --- Step 2: Print and clear the buffer ---
//   if (bufferLen > 0) {
//     Serial.print("Buffer: ");
//     Serial.println(buffer);  // Print hex data as string
//     bufferLen = 0;           // Clear buffer by resetting length
//     buffer[0] = '\0';        // Optional: clear first byte explicitly
//   }

// }
void advanceCyberGStep() {
  for (auto it = CyberGSequencerNotes.begin(); it != CyberGSequencerNotes.end();) {
    SequencerNote& note = *it;  // Reference to current note

    if (note.offset == 1) {
      //do not play rest and placeholder notes
      if (note.note > 0 && note.note != REST_NOTE && note.note > 12) {
        sendNoteRealOn(note.channel, note.note, note.velocity);
      }
      note.offset = 0;
    } else if (note.offset > 1) {
      note.offset--;
    }
    // count down to playing the note
    if (note.offset == 0 && note.holdTime < INDEFINITE_HOLD_TIME && note.holdTime > 0) {
      note.holdTime--;
    }
    //note is done playing, delete the note
    if (note.offset == 0 && note.holdTime == 0) {
      if (note.note > 0 && note.note != REST_NOTE && note.note > 12) {
        sendNoteRealOff(note.channel, note.note);
      }
      // Remove the note from the vector
      it = CyberGSequencerNotes.erase(it);  // `erase` returns the next iterator
    } else {
      ++it;  // Only increment if we didn’t erase the note
    }
  }
}

void advanceSequencerStep() {
  for (auto it = SequencerNotes.begin(); it != SequencerNotes.end();) {
    SequencerNote& note = *it;  // Reference to current note

    if (note.offset == 1) {
      //do not play rest and placeholder notes
      if (note.note > 0 && note.note != REST_NOTE && note.note > 12) {
        sendNoteOn(note.channel, note.note, note.velocity);
      }
      note.offset = 0;
    } else if (note.offset > 1) {
      note.offset--;
    }
    // count down to playing the note
    if (note.offset == 0 && note.holdTime < INDEFINITE_HOLD_TIME && note.holdTime > 0) {
      note.holdTime--;
    }
    //note is done playing, delete the note
    if (note.offset == 0 && note.holdTime == 0) {
      if (note.note > 0 && note.note != REST_NOTE && note.note > 12) {
        sendNoteOff(note.channel, note.note);
      }
      // Remove the note from the vector
      it = SequencerNotes.erase(it);  // `erase` returns the next iterator
    } else {
      ++it;  // Only increment if we didn’t erase the note
    }
  }
}

void advanceBassSequencerStep() {
  //enabled and started and bass sequencer notes is 0
  if (!bassStart && BassSequencerNotes.size() == 0) {
    return;
  }
  if (drumState == DrumIntro)  //no playing during drum intro
  {
    return;
  }
  if (lastValidNeckButtonPressed == -1)  //only play if a button has been pressed
  {
    if (neckButtonPressed != -1 && neckButtonPressed < MIN_IGNORED_GUITAR)  //handle first button is actually pressed
    {
      //do nothing as valid button
    } else if (neckButtonPressed >= MIN_IGNORED_GUITAR) {
      return;
    }
  }
  //check if BassSequencerNotes is empty, populate
  //Serial.printf("BassSequencerNotes.size() = %d BassSequencerPattern.size() = %d\n", BassSequencerNotes.size(), BassSequencerPattern.size());

  //check current button pressed:
  bool forceChange = false;
  int8_t lastReference = lastValidNeckButtonPressed;
  if (neckButtonPressed != -1 && neckButtonPressed < MIN_IGNORED_GUITAR && lastBassNeckButtonPressed != neckButtonPressed && lastValidNeckButtonPressed == lastBassNeckButtonPressed) {
    lastReference = neckButtonPressed;
    forceChange = true;
  }
  if (lastReference == -1 && BassSequencerNotes.size() == 0) {
    BassSequencerNotes = getPatternNotesFromChord(BASS_CHANNEL, 0);  //assume 0
    if (backingState == 0)                                           //we do not support this case for standard backing
    {
      return;
    }
  } else if (BassSequencerNotes.size() == 0 && BassSequencerPattern.size() > 0) {
    lastBassNeckButtonPressed = lastReference;
    BassSequencerNotes = getPatternNotesFromChord(BASS_CHANNEL, lastReference);
    //given the ppattern, we replace the notes based on chord
  } else if (lastReference != -1 && (lastReference != lastBassNeckButtonPressed || forceChange)) {
    if (lastBassNeckButtonPressed == -1) {
      lastBassNeckButtonPressed = 0;  //to handle case where button without actual neck pressed case
    }
    //todo check if no5 is better
    std::vector<uint8_t> chordNotesA;
    std::vector<uint8_t> chordNotesB;
    //if(enableAllNotesOnChords[preset])
    {
      chordNotesA = getActualAssignedChord(lastBassNeckButtonPressed).getChords().getCompleteChordNotes();  //get notes
      chordNotesB = getActualAssignedChord(lastReference).getChords().getCompleteChordNotes();              //get notes
    }

    //else
    //{
    //      chordNotesA = getActualAssignedChord(lastBassNeckButtonPressed).getChords().getCompleteChordNotes3();  //get notes
    //chordNotesB = getActualAssignedChord(lastReference).getChords().getCompleteChordNotes3();  //get notes
    //}
    updateNotes(chordNotesA, chordNotesB, BassSequencerNotes);
    lastBassNeckButtonPressed = lastReference;
    //we need to switch the notes
    //get the notes of the previous chord
    //get the notes of the new chord
    //swap them out
  } else if (BassSequencerNotes.size() == 0) {
    BassSequencerNotes = getPatternNotesFromChord(BASS_CHANNEL, lastReference);
  }
  for (auto it = BassSequencerNotes.begin(); it != BassSequencerNotes.end();) {
    SequencerNote& note = *it;  // Reference to current note
    if ((drumState == DrumStopped || drumState == DrumEnding) || !bassStart) {
      //stop everything and send them to be deleted
      note.offset = 0;
      note.holdTime = 0;
      bassStart = false;
    } else {
      if (note.offset == 1) {
        if (note.note != REST_NOTE && note.note > 12) {
          if (bassEnabled[preset] && lastReference != -1) {
            sendNoteOn(BASS_CHANNEL, note.note, note.velocity);
          }
        }
        note.offset = 0;
      } else if (note.offset > 1) {
        note.offset--;
      }
      // count down to playing the note
      if (note.offset == 0 && note.holdTime < INDEFINITE_HOLD_TIME && note.holdTime > 0) {
        note.holdTime--;
      }
    }
    //note is done playing, delete the note
    if (note.offset == 0 && note.holdTime == 0) {
      if (note.note > 0 && note.note != REST_NOTE && note.note > 12) {
        sendNoteOff(BASS_CHANNEL, note.note);  //todo figure out  how to adjust bass output
      }
      // Remove the note from the vector
      it = BassSequencerNotes.erase(it);  // `erase` returns the next iterator
    } else {
      ++it;  // Only increment if we didn’t erase the note
    }
  }
}

void advanceAccompanimentSequencerStep() {
  //enabled and started and bass sequencer notes is 0
  if (!accompanimentStart && AccompanimentSequencerNotes.size() == 0) {
    return;
  }
  if (drumState == DrumIntro)  //no playing during drum intro
  {
    return;
  }

  if (lastValidNeckButtonPressed == -1)  //only play if a button has been pressed
  {
    if (neckButtonPressed != -1 && neckButtonPressed < MIN_IGNORED_GUITAR)  //handle first button is actually pressed
    {
      //do nothing as valid button
    } else if (neckButtonPressed >= MIN_IGNORED_GUITAR) {
      return;
    }
  }

  bool forceChange = false;
  int8_t lastReference = lastValidNeckButtonPressed;
  if (neckButtonPressed != -1 && neckButtonPressed < MIN_IGNORED_GUITAR && lastAccompanimentNeckButtonPressed != neckButtonPressed && lastValidNeckButtonPressed == lastAccompanimentNeckButtonPressed) {
    lastReference = neckButtonPressed;
    lastValidNeckButtonPressed = lastReference;  //force update since this is the last in the sequence
    //since it is last
    forceChange = true;
  }
  if (lastReference == -1 && AccompanimentSequencerNotes.size() == 0) {
    if (backingState == 0)  //we do not support this case for standard backing
    {
      return;
    }
    AccompanimentSequencerNotes = getPatternNotesFromChord(ACCOMPANIMENT_CHANNEL, 0);
  }
  //check if AccompanimentSequencerNotes is empty, populate
  else if (AccompanimentSequencerNotes.size() == 0 && AccompanimentSequencerPattern.size() > 0) {
    lastAccompanimentNeckButtonPressed = lastReference;
    AccompanimentSequencerNotes = getPatternNotesFromChord(ACCOMPANIMENT_CHANNEL, lastReference);
    //given the ppattern, we replace the notes based on chord
  } else if (lastReference != -1 && (lastValidNeckButtonPressed != lastAccompanimentNeckButtonPressed || forceChange)) {
    if (lastAccompanimentNeckButtonPressed == -1) {
      lastAccompanimentNeckButtonPressed = 0;  //to handle case where button without actual neck pressed case
    }

    //todo check if no5 is better
    std::vector<uint8_t> chordNotesA;
    std::vector<uint8_t> chordNotesB;
    if (enableAllNotesOnChords[preset]) {
      chordNotesA = getActualAssignedChord(lastAccompanimentNeckButtonPressed).getChords().getCompleteChordNotes();  //get notes
      chordNotesB = getActualAssignedChord(lastReference).getChords().getCompleteChordNotes();                       //get notes
    } else {
      chordNotesA = getActualAssignedChord(lastAccompanimentNeckButtonPressed).getChords().getCompleteChordNotes3();  //get notes
      chordNotesB = getActualAssignedChord(lastReference).getChords().getCompleteChordNotes3();                       //get notes
    }
    updateNotes(chordNotesA, chordNotesB, AccompanimentSequencerNotes);
    lastAccompanimentNeckButtonPressed = lastReference;
    //we need to switch the notes
    //get the notes of the previous chord
    //get the notes of the new chord
    //swap them out
  } else if (AccompanimentSequencerNotes.size() == 0) {
    AccompanimentSequencerNotes = getPatternNotesFromChord(ACCOMPANIMENT_CHANNEL, lastReference);
  }
  for (auto it = AccompanimentSequencerNotes.begin(); it != AccompanimentSequencerNotes.end();) {
    SequencerNote& note = *it;  // Reference to current note
    if ((drumState == DrumStopped || drumState == DrumEnding) || !accompanimentStart) {
      //stop everything and send them to be deleted
      note.offset = 0;
      note.holdTime = 0;
      accompanimentStart = false;
    } else {
      if (note.offset == 1) {
        if (note.note != REST_NOTE && note.note > 12) {
          if (accompanimentEnabled[preset] && lastReference != -1) {
            sendNoteOn(ACCOMPANIMENT_CHANNEL, note.note, note.velocity);
          }
        }
        note.offset = 0;
      } else if (note.offset > 1) {
        note.offset--;
      }
      // count down to playing the note
      if (note.offset == 0 && note.holdTime < INDEFINITE_HOLD_TIME && note.holdTime > 0) {
        note.holdTime--;
      }
    }
    //note is done playing, delete the note
    if (note.offset == 0 && note.holdTime == 0) {
      if (note.note > 0 && note.note != REST_NOTE && note.note > 12) {
        sendNoteOff(ACCOMPANIMENT_CHANNEL, note.note);
      }
      // Remove the note from the vector
      it = AccompanimentSequencerNotes.erase(it);  // `erase` returns the next iterator
    } else {
      ++it;  // Only increment if we didn’t erase the note
    }
  }
}

void prepareDrumSequencer() {
  bool nextIsNotLoop = false;
  if (DrumSequencerNotes.size() == 0) {
    switch (drumState) {
      case DrumLoop:
        if (drumNextState == DrumNone)  // continue loop
        {
          if (DrumLoopHalfBarSequencer.size() > 0)  //play half bar end of loop
          {
            DrumSequencerNotes = DrumLoopHalfBarSequencer;
            drumState = DrumLoopHalfBar;
            if (debug)
              Serial.printf("DrumLoop->DrumLoopHalfBar\n");
          } else {
            DrumSequencerNotes = DrumLoopSequencer;
            if (debug)
              Serial.printf("DrumLoop->DrumLoop\n");
          }
        } else {
          if (drumNextState == DrumLoopFill && DrumLoopHalfBarSequencer.size() > 0) {
            if (debug)
              Serial.printf("DrumLoop->DrumLoopFill\n");
            drumState = drumNextState;
            drumNextState = DrumNone;
            nextIsNotLoop = true;
          } else if (DrumLoopHalfBarSequencer.size() > 0 && drumNextState != DrumEnding)  //not a fill and there is a half bar, half should be played first
          {
            if (debug)
              Serial.printf("DrumLoop->DrumLoopHalfbar\n");
            //play half file
            drumState = DrumLoopHalfBar;
            nextIsNotLoop = true;
          } else  // no half bar
          {
            if (debug)
              Serial.printf("Drum Loop %d to Next is %d? End=5\n", drumState, drumNextState);
            //if state change;
            //if half exists, you need to play half unless fill
            drumState = drumNextState;
            drumNextState = DrumNone;
            nextIsNotLoop = true;
          }
        }
        break;
      case DrumLoopHalfBar:
        if (drumNextState == DrumNone)  // continue loop
        {
          if (debug)
            Serial.printf("DrumLoopHalf->DrumLoop\n");
          DrumSequencerNotes = DrumLoopSequencer;
          drumState = DrumLoop;
        } else if (drumNextState == DrumLoopFill)  //fill has to play during half bar part, so it will loop again then gets played at half bar
        {
          if (debug)
            Serial.printf("DrumLoopHalf->DrumFill\n");
          drumState = DrumLoop;
          DrumSequencerNotes = DrumLoopSequencer;
        } else {
          if (debug)
            Serial.printf("DrumLoopHalf->Next %d\n", drumNextState);
          //if state change;
          drumState = drumNextState;
          drumNextState = DrumNone;
          nextIsNotLoop = true;
        }
        break;
      case DrumIntro:
        if (drumNextState == DrumNone)  // start loop
        {
          if (debug)
            Serial.printf("DrumLoopIntro->DrumLoop\n");
          DrumSequencerNotes = DrumIntroSequencer;
          //drumState = DrumLoop;
          //drumState = DrumIntro;
          drumNextState = DrumLoop;
        } else {
          if (debug)
            Serial.printf("DrumLoopIntro->? %d\n", drumNextState);
          //if state change;
          drumState = drumNextState;
          drumNextState = DrumNone;
          DrumSequencerNotes = DrumLoopSequencer;
          if (drumState == DrumEnding) {
            nextIsNotLoop = true;
          } else {
            nextIsNotLoop = false;
          }
        }
        break;
      case DrumLoopFill:
        if (drumNextState == DrumNone)  // continue loop
        {
          if (debug)
            Serial.printf("DrumLoopFill->DrumLoop\n");
          DrumSequencerNotes = DrumLoopSequencer;
          drumState = DrumLoop;
        } else {
          if (debug)
            Serial.printf("DrumLoopFill->?\n");
          //if state change;
          drumState = drumNextState;
          drumNextState = DrumNone;
          nextIsNotLoop = true;
        }
        break;
      case DrumEnding:
        if (drumNextState == DrumIntro)  // continue loop
        {
          if (debug)
            Serial.printf("DrumEnd->DrumIntro\n");
          DrumSequencerNotes = DrumIntroSequencer;
          drumState = DrumIntro;
        } else {
          if (debug)
            Serial.printf("DrumEnd->Stop\n");
          drumState = DrumStopped;  // should not loop or do anything else
        }
        break;
      default:
        //drumStopped; //do nothing
        break;
    }
    //actually load the sequencer for the interrupt kinds
    if (nextIsNotLoop) {
      switch (drumState) {
        case DrumIntro:
          DrumSequencerNotes = DrumIntroSequencer;
          break;
        case DrumEnding:
          DrumSequencerNotes = DrumEndSequencer;
          if (DrumEndSequencer.size() == 0) {
            drumState = DrumStopped;  // should not loop or do anything else
          }
          break;
        case DrumLoopFill:
          DrumSequencerNotes = DrumFillSequencer;
          break;
        default:
          break;
      }
    }
  }
}
void advanceDrumSequencerStep() {
  for (auto it = DrumSequencerNotes.begin(); it != DrumSequencerNotes.end();) {
    SequencerNote& note = *it;  // Reference to current note

    if (note.offset == 1) {
      if (note.note > 0 && note.note != REST_NOTE) {
        if (drumsEnabled[preset]) {
          sendNoteOn(DRUM_CHANNEL, note.note, note.velocity);
        }
      }
      note.offset = 0;
    } else if (note.offset > 1) {
      note.offset--;
    }
    // count down to playing the note
    if (note.offset == 0 && note.holdTime < INDEFINITE_HOLD_TIME && note.holdTime > 0) {
      note.holdTime--;
    }
    //note is done playing, delete the note
    if (note.offset == 0 && note.holdTime == 0) {
      if (note.note > 0 && note.note != REST_NOTE) {
        sendNoteOff(DRUM_CHANNEL, note.note);  //will still note off as needed just in case
      }
      // Remove the note from the vector
      it = DrumSequencerNotes.erase(it);  // `erase` returns the next iterator
    } else {
      ++it;  // Only increment if we didn’t erase the note
    }
  }
  //if last note was played in drum sequence, play the next if applicable
  if (DrumSequencerNotes.size() == 0 && drumState != DrumStopped) {
    prepareDrumSequencer();
  }
}

void onTick64() {
  tickCount++;

  // Send MIDI Clock every 1/24 note (i.e., every other 1/48 tick)
  if (midiClockEnable && tickCount % 2 == 0) {
    usbMIDI.sendRealTime(usbMIDI.Clock);
    /* disable clock on BLE as it will have issues
    if (BLEEnabled)
    {
      Serial7.write(CLOCK_MIDI);
    }
    */
  }
  //if note queue is not empty
  if (SequencerNotes.size() > 0) {
    advanceSequencerStep();  // or send a note, etc.
  }
  if (DrumSequencerNotes.size() > 0) {
    advanceDrumSequencerStep();  // or send a note, etc.
  }
  advanceBassSequencerStep();  // or send a note, etc.
  advanceAccompanimentSequencerStep();
  if (CyberGSequencerNotes.size() > 0) {
    advanceCyberGStep();
  }
  // Optional: wrap counter
  if (tickCount >= 4800) tickCount = 0;
}

void noteChannelOff(uint8_t channel) {
  for (uint8_t n = 0; n < MAX_NOTE; n++) {
    sendNoteOff(channel, n);
  }
}

bool printFileContents(const char* filename, bool isSerialOut) {
  File f = SD.open(filename, FILE_READ);
  if (!f) {
    if (!isSerialOut && debug) {
      Serial.printf("Failed to open %s for reading\n", filename);
    }
    return false;
  }

  if (!isSerialOut && debug) {
    Serial.printf("Contents of %s:\n", filename);
  }

  while (f.available()) {
    char c = f.read();  // Read one character
    if (isSerialOut) {
      Serial.write(c);  // Print using printf
    } else {
      if (debug)
        Serial.printf("%c", c);  // Print using printf
    }
  }

  f.close();
  if (isSerialOut && debug) {
    Serial.write("\n");  // Print using printf
  } else if (debug) {
    Serial.println();  // Optional: newline after file contents
  }
  return true;
}

bool savePatternRelatedConfig() {
  File f = SD.open("configP.ini", FILE_WRITE);
  if (f) {
    f.seek(0);
    f.truncate();
    //file.println("your new config data");
    //snprintf(buffer, sizeof(buffer), "Name: %s, Int: %d, Float: %.2f\n", name, someValue, anotherValue);
    //file.print(buffer);
    char buffer[64];
    //MAX_PRESET value

    snprintf(buffer, sizeof(buffer), "backingState,%d\n", backingState ? 1 : 0);
    f.print(buffer);
    snprintf(buffer, sizeof(buffer), "AccompanimentPatternID,%d\n", AccompanimentPatternID);
    f.print(buffer);
    snprintf(buffer, sizeof(buffer), "BassPatternID,%d\n", BassPatternID);
    f.print(buffer);
    snprintf(buffer, sizeof(buffer), "GuitarPatternID0,%d\n", GuitarPatternID0);
    f.print(buffer);
    snprintf(buffer, sizeof(buffer), "GuitarPatternID1,%d\n", GuitarPatternID1);
    f.print(buffer);
    snprintf(buffer, sizeof(buffer), "GuitarPatternID2,%d\n", GuitarPatternID2);
    f.print(buffer);
    snprintf(buffer, sizeof(buffer), "DrumPatternID0,%d\n", DrumPatternID[0]);
    f.print(buffer);
    snprintf(buffer, sizeof(buffer), "DrumPatternID1,%d\n", DrumPatternID[1]);
    f.print(buffer);
    snprintf(buffer, sizeof(buffer), "DrumPatternID2,%d\n", DrumPatternID[2]);
    f.print(buffer);
    snprintf(buffer, sizeof(buffer), "DrumPatternID3,%d\n", DrumPatternID[3]);
    f.print(buffer);
    snprintf(buffer, sizeof(buffer), "DrumPatternID4,%d\n", DrumPatternID[4]);
    f.print(buffer);

  } else {
    if (debug)
      Serial.println("Error saving pattern related config!");
    return false;
  }
  f.close();
  return true;
}

bool saveSimpleConfig() {
  File f = SD.open("config.ini", FILE_WRITE);
  if (f) {
    f.seek(0);
    f.truncate();
    //file.println("your new config data");
    //snprintf(buffer, sizeof(buffer), "Name: %s, Int: %d, Float: %.2f\n", name, someValue, anotherValue);
    //file.print(buffer);
    char buffer[64];
    //MAX_PRESET value
    snprintf(buffer, sizeof(buffer), "MAX_PRESET,%d\n", MAX_PRESET);
    f.print(buffer);
    snprintf(buffer, sizeof(buffer), "CUR_PRESET,%d\n", preset);
    f.print(buffer);
    snprintf(buffer, sizeof(buffer), "passThroughSerial,%d\n", passThroughSerial);
    f.print(buffer);
    //bool stopSoundsOnPresetChange = true;
    snprintf(buffer, sizeof(buffer), "stopSoundsOnPresetChange,%d\n", stopSoundsOnPresetChange ? 1 : 0);
    f.print(buffer);
    //bool midiClockEnable = true;

    snprintf(buffer, sizeof(buffer), "midiClockEnable,%d\n", midiClockEnable ? 1 : 0);
    f.print(buffer);

    for (uint8_t i = 0; i < MAX_PRESET; i++) {
      //std::vector<uint8_t> omniChordModeGuitar;
      snprintf(buffer, sizeof(buffer), "omniChordModeGuitar,%d,%d\n", i, omniChordModeGuitar[i]);
      f.print(buffer);

      //std::vector<bool> muteWhenLetGo;
      snprintf(buffer, sizeof(buffer), "muteWhenLetGo,%d,%d\n", i, muteWhenLetGo[i] ? 1 : 0);
      f.print(buffer);

      //std::vector<bool> ignoreSameChord;
      snprintf(buffer, sizeof(buffer), "ignoreSameChord,%d,%d\n", i, ignoreSameChord[i] ? 1 : 0);
      f.print(buffer);
      //std::vector<uint16_t> presetBPM;
      snprintf(buffer, sizeof(buffer), "presetBPM,%d,%d\n", i, presetBPM[i]);
      f.print(buffer);
      //std::vector<bool> chordHold;
      snprintf(buffer, sizeof(buffer), "chordHold,%d,%d\n", i, chordHold[i] ? 1 : 0);
      f.print(buffer);
      //std::vector<uint8_t> simpleChordSetting;
      snprintf(buffer, sizeof(buffer), "simpleChordSetting,%d,%d\n", i, simpleChordSetting[i]);
      f.print(buffer);
      //std::vector<uint8_t> strumSeparation; //time unit separation between notes //default 1
      snprintf(buffer, sizeof(buffer), "strumSeparation,%d,%d\n", i, strumSeparation[i]);
      f.print(buffer);
      snprintf(buffer, sizeof(buffer), "alternateDirection,%d,%d\n", i, (int)alternateDirection[i]);
      f.print(buffer);
      snprintf(buffer, sizeof(buffer), "useToggleSustain,%d,%d\n", i, (int)alternateDirection[i]);
      f.print(buffer);
      snprintf(buffer, sizeof(buffer), "omniKBTransposeOffset,%d,%d\n", i, omniKBTransposeOffset[i]);
      f.print(buffer);
      snprintf(buffer, sizeof(buffer), "guitarTranspose,%d,%d\n", i, (int)guitarTranspose[i]);
      f.print(buffer);
      //snprintf(buffer, sizeof(buffer), "useGradualMute,%d,%d\n", i, (int) useGradualMute[i]);
      snprintf(buffer, sizeof(buffer), "enableButtonMidi,%d,%d\n", i, (int)enableButtonMidi[i]);
      f.print(buffer);

      snprintf(buffer, sizeof(buffer), "properOmniChord5ths,%d,%d\n", i, (int)properOmniChord5ths[i]);
      f.print(buffer);
      snprintf(buffer, sizeof(buffer), "enableAllNotesOnChords,%d,%d\n", i, (int)enableAllNotesOnChords[i]);
      f.print(buffer);
      snprintf(buffer, sizeof(buffer), "isSimpleChordMode,%d,%d\n", i, (int)isSimpleChordMode[i]);
      f.print(buffer);

      snprintf(buffer, sizeof(buffer), "drumsEnabled,%d,%d\n", i, (int)drumsEnabled[i]);
      f.print(buffer);
      snprintf(buffer, sizeof(buffer), "bassEnabled,%d,%d\n", i, (int)bassEnabled[i]);
      f.print(buffer);
    }
  } else {
    if (debug)
      Serial.println("Error saving simple config!");
    return false;
  }
  f.close();
  return true;
}

bool saveComplexConfig() {
  File f = SD.open("config2.ini", FILE_WRITE);
  if (f) {
    f.seek(0);
    f.truncate();
    char buffer[50];

    for (uint8_t k = 0; k < MAX_PRESET; k++) {
      for (uint8_t i = 0; i < NECK_ROWS; i++) {
        for (uint8_t j = 0; j < NECK_COLUMNS; j++) {
          snprintf(buffer, sizeof(buffer), "neckAssignmentsKey,%d,%d,%d,%d,%d\n", k, i, j, neckAssignments[k][i * NECK_COLUMNS + j].key, neckAssignments[k][i * NECK_COLUMNS + j].chordType);
          f.print(buffer);
          //seems this is derived from neckAssignments + simpleChordSetting + row, column, preset
          //simpleChord setting - not needed
          //AssignedPattern(simpleChordSetting[x], getKeyboardChordNotesFromNeck(i,j,x), getGuitarChordNotesFromNeck(i,j,x), rootNotes[i], false);
          //prepareChords() could be used for loading but must clear the data first
        }
      }
    }

    for (uint8_t k = 0; k < MAX_PRESET; k++) {
      for (uint8_t i = 0; i < NECK_ROWS; i++) {
        for (uint8_t j = 0; j < NECK_COLUMNS; j++) {
          snprintf(buffer, sizeof(buffer), "assignedFretPatternsByPreset,%d,%d,%d,%d\n", k, i, j, assignedFretPatternsByPreset[k][i * NECK_COLUMNS + j].customPattern);
          f.print(buffer);
        }
      }
    }
  } else {
    if (debug)
      Serial.println("Error saving complex config!");
    return false;
  }
  f.close();
  return true;
}

void handleOmnichordBackingChange(uint8_t newBackingType) {
  if (newBackingType < MIN_BACKING_TYPE || newBackingType > MAX_BACKING_TYPE) {
    return;
  }
  if (backingState == 0) {
    if (newBackingType == 0) {
      return;  //do nothing as nothing changed
    }
    savePatternFiles(true);  //save to backup and change later
  } else {
    if (newBackingType != 0) {
      //no need to backup
    } else {
      loadPatternFiles(true);  //saves to new
      backingState = 0;        //revert to 0
      return;
    }
  }
  backingState = newBackingType;
  //clear data they overwrite
  DrumLoopSequencer.clear();
  DrumLoopHalfBarSequencer.clear();
  DrumFillSequencer.clear();
  DrumIntroSequencer.clear();
  DrumEndSequencer.clear();
  DrumSequencerNotes.clear();
  BassSequencerPattern.clear();
  AccompanimentSequencerPattern.clear();

  switch (newBackingType) {
    case 1:
      generateRock1();
      break;
    case 2:
      generateRock2();
      break;
    case 3:
      generateBossanova();
      break;
    case 4:
      generateFunk();
      break;
    case 5:
      generateCountry();
      break;
    case 6:
      generateDisco();
      break;
    case 7:
      generateSlowRock();
      break;
    case 8:
      generateSwing();
      break;
    case 9:
      generateWaltz();
      break;
    default:
      generateHiphop();
  }
}
bool saveSettings() {
  if (!saveSimpleConfig()) {
    if (debug)
      Serial.printf("Error loading Simple Config!\n");
    return false;
  }
  if (!saveComplexConfig()) {
    if (debug)
      Serial.printf("Error loading Complex Config!\n");
    return false;
  }
  return true;
}
bool loadPatternRelatedConfig() {
  File f = SD.open("configP.ini", FILE_READ);
  if (!f) {
    if (debug)
      Serial.println("Error opening configP.ini for reading!");
    return false;
  }

  char line[64];
  while (f.available()) {
    size_t len = f.readBytesUntil('\n', line, sizeof(line) - 1);
    line[len] = '\0';  // Null-terminate

    // Trim trailing \r if present
    if (len > 0 && line[len - 1] == '\r') {
      line[len - 1] = '\0';
    }

    // Parse the line
    char* key = strtok(line, ",");
    char* arg1 = strtok(NULL, ",");

    if (!key || !arg1) continue;
    if (strcmp(key, "backingState") == 0) {
      backingState = atoi(arg1);
    }
    /*
        uint16_t DrumPatternID[5];
        uint16_t BassPatternID;
        uint16_t GuitarPatternID0;
        uint16_t GuitarPatternID1;
        uint16_t GuitarPatternID2;
    */
    else if (strcmp(key, "BassPatternID") == 0) {
      BassPatternID = atoi(arg1);
    } else if (strcmp(key, "AccompanimentPatternID") == 0) {
      AccompanimentPatternID = atoi(arg1);
    } else if (strcmp(key, "GuitarPatternID0") == 0) {
      GuitarPatternID0 = atoi(arg1);
    } else if (strcmp(key, "GuitarPatternID1") == 0) {
      GuitarPatternID1 = atoi(arg1);
    } else if (strcmp(key, "GuitarPatternID2") == 0) {
      GuitarPatternID2 = atoi(arg1);
    } else if (strcmp(key, "DrumPatternID0") == 0) {
      DrumPatternID[0] = atoi(arg1);
    } else if (strcmp(key, "DrumPatternID1") == 0) {
      DrumPatternID[1] = atoi(arg1);
    } else if (strcmp(key, "DrumPatternID2") == 0) {
      DrumPatternID[2] = atoi(arg1);
    } else if (strcmp(key, "DrumPatternID3") == 0) {
      DrumPatternID[3] = atoi(arg1);
    } else if (strcmp(key, "DrumPatternID4") == 0) {
      DrumPatternID[4] = atoi(arg1);
    }
  }
  f.close();
  return true;
}
bool loadSimpleConfig() {
  File f = SD.open("config.ini", FILE_READ);
  if (!f) {
    if (debug)
      Serial.println("Error opening config.ini for reading!");
    return false;
  }
  char line[64];
  while (f.available()) {
    size_t len = f.readBytesUntil('\n', line, sizeof(line) - 1);
    line[len] = '\0';  // Null-terminate

    // Trim trailing \r if present
    if (len > 0 && line[len - 1] == '\r') {
      line[len - 1] = '\0';
    }

    // Parse the line
    char* key = strtok(line, ",");
    char* arg1 = strtok(NULL, ",");
    char* arg2 = strtok(NULL, ",");
    if (!key || !arg1) continue;
    int readPreset = MAX_PRESET;
    if (strcmp(key, "MAX_PRESET") == 0) {
      readPreset = atoi(arg1);
      if (readPreset > MAX_PRESET && debug) {
        Serial.printf("Error preset read is %d > MAX_PRESET\n", readPreset, MAX_PRESET);
        f.close();
        return false;
      }
    } else if (strcmp(key, "CUR_PRESET") == 0) {
      uint8_t newPreset = atoi(arg1);
      if (debug) {
        Serial.printf("new vs old preset is %d vs %d\n", newPreset, preset);
      }
      if (newPreset != preset) {
        preset = newPreset;
        presetChanged();
      }

    } else if (strcmp(key, "stopSoundsOnPresetChange") == 0) {
      stopSoundsOnPresetChange = atoi(arg1);
    } else if (strcmp(key, "passThroughSerial") == 0) {
      passThroughSerial = atoi(arg1);
    } else if (strcmp(key, "midiClockEnable") == 0) {
      midiClockEnable = atoi(arg1);
    } else if (arg2) {
      uint8_t i = atoi(arg1);
      int val = atoi(arg2);

      if (strcmp(key, "omniChordModeGuitar") == 0 && i < omniChordModeGuitar.size())
        omniChordModeGuitar[i] = val;

      else if (strcmp(key, "muteWhenLetGo") == 0 && i < muteWhenLetGo.size())
        muteWhenLetGo[i] = val;

      else if (strcmp(key, "ignoreSameChord") == 0 && i < ignoreSameChord.size())
        ignoreSameChord[i] = val;

      else if (strcmp(key, "presetBPM") == 0 && i < presetBPM.size())
        presetBPM[i] = val;

      else if (strcmp(key, "chordHold") == 0 && i < chordHold.size())
        chordHold[i] = val;

      else if (strcmp(key, "simpleChordSetting") == 0 && i < simpleChordSetting.size())
        simpleChordSetting[i] = val;

      else if (strcmp(key, "strumSeparation") == 0 && i < strumSeparation.size())
        strumSeparation[i] = val;
      else if (strcmp(key, "alternateDirection") == 0 && i < alternateDirection.size())
        alternateDirection[i] = val;
      else if (strcmp(key, "useToggleSustain") == 0 && i < useToggleSustain.size())
        useToggleSustain[i] = val;
      else if (strcmp(key, "omniKBTransposeOffset") == 0 && i < omniKBTransposeOffset.size())
        omniKBTransposeOffset[i] = val;
      else if (strcmp(key, "guitarTranspose") == 0 && i < guitarTranspose.size())
        guitarTranspose[i] = val;
      //else if (strcmp(key, "useGradualMute") == 0 && i < useGradualMute.size())
      //useGradualMute[i] = val;
      else if (strcmp(key, "enableButtonMidi") == 0 && i < enableButtonMidi.size())
        enableButtonMidi[i] = val == 1;
      else if (strcmp(key, "properOmniChord5ths") == 0 && i < properOmniChord5ths.size())
        properOmniChord5ths[i] = val == 1;

      else if (strcmp(key, "isSimpleChordMode") == 0 && i < isSimpleChordMode.size())
        isSimpleChordMode[i] = val == 1;
      else if (strcmp(key, "enableAllNotesOnChords") == 0 && i < enableAllNotesOnChords.size())
        enableAllNotesOnChords[i] = val == 1;
      else if (strcmp(key, "drumsEnabled") == 0 && i < drumsEnabled.size())
        drumsEnabled[i] = val == 1;
      else if (strcmp(key, "bassEnabled") == 0 && i < bassEnabled.size())
        bassEnabled[i] = val == 1;
    }
  }
  f.close();
  return true;
}

bool loadComplexConfig() {
  File f = SD.open("config2.ini", FILE_READ);
  if (!f) {
    if (debug)
      Serial.println("Error opening config2.ini for reading!");
    return false;
  }

  char line[80];
  while (f.available()) {
    size_t len = f.readBytesUntil('\n', line, sizeof(line) - 1);
    line[len] = '\0';  // null-terminate

    if (len > 0 && line[len - 1] == '\r') {
      line[len - 1] = '\0';
    }

    char* key = strtok(line, ",");
    char* arg0 = strtok(NULL, ",");
    char* arg1 = strtok(NULL, ",");
    char* arg2 = strtok(NULL, ",");
    char* arg3 = strtok(NULL, ",");
    char* arg4 = strtok(NULL, ",");

    if (!key || !arg0 || !arg1 || !arg2 || !arg3 || !arg4) {
      continue;  // invalid line
    }

    if (strcmp(key, "neckAssignmentsKey") == 0) {
      uint8_t presetT = atoi(arg0);
      uint8_t row = atoi(arg1);
      uint8_t col = atoi(arg2);
      uint8_t keyVal = atoi(arg3);
      uint8_t chordTypeVal = atoi(arg4);

      if (presetT < neckAssignments.size() && (row * NECK_COLUMNS + col) < (int)neckAssignments[presetT].size()) {
        neckAssignments[presetT][row * NECK_COLUMNS + col].key = (Note)keyVal;
        neckAssignments[presetT][row * NECK_COLUMNS + col].chordType = (ChordType)chordTypeVal;
      }
    }
    if (strcmp(key, "assignedFretPatternsByPreset") == 0) {
      uint8_t presetT = atoi(arg0);
      uint8_t row = atoi(arg1);
      uint8_t col = atoi(arg2);
      uint8_t patVal = atoi(arg3);

      if (presetT < neckAssignments.size() && (row * NECK_COLUMNS + col) < (int)assignedFretPatternsByPreset[presetT].size()) {
        assignedFretPatternsByPreset[presetT][row * NECK_COLUMNS + col].customPattern = patVal;
      }
    }
  }
  f.close();
  //update assignedFretPatternsByPreset's rootnotes
  //todo check
  for (int x = 0; x < MAX_PRESET; x++) {
    for (int i = 0; i < NECK_ROWS; i++) {
      for (int j = 0; j < NECK_COLUMNS; j++) {
        assignedFretPatternsByPreset[x][i * NECK_COLUMNS + j].assignedChord = getKeyboardChordNotesFromNeck(i, j, x);
        assignedFretPatternsByPreset[x][i * NECK_COLUMNS + j].assignedGuitarChord = getGuitarChordNotesFromNeck(i, j, x);
        assignedFretPatternsByPreset[x][i * NECK_COLUMNS + j].assignedChord.setRootNote((int)neckAssignments[x][i * NECK_COLUMNS + j].key + OMNICHORD_ROOT_START);
        //assignedFretPatterns.push_back(AssignedPattern(simpleChordSetting[x], getKeyboardChordNotesFromNeck(i, j, x), getGuitarChordNotesFromNeck(i, j, x), rootNotes[i], false, 0));
      }
    }
  }
  return true;
}

bool loadSettings() {
  File file = SD.open("config.ini", FILE_READ);
  if (file) {
    file.seek(0);
    file.truncate();
    //file.println("your new config data");
    //snprintf(buffer, sizeof(buffer), "Name: %s, Int: %d, Float: %.2f\n", name, someValue, anotherValue);
    //file.print(buffer);
    if (!loadSimpleConfig()) {
      file.close();
      if (debug)
        Serial.printf("Error loading Simple Config!\n");
      return false;
    }
    if (!loadComplexConfig()) {
      file.close();
      if (debug)
        Serial.printf("Error loading Complex Config!\n");
      return false;
    }
    file.close();
  } else {
    return false;
  }
  if (debug) {
    Serial.println("Loading config is OK!");
  }
  return true;
}

void fakeSerialKBPaddle(HardwareSerial& source, HardwareSerial& destination, char* buffer, uint8_t& bufferLen) {
  const uint8_t targetMsg[] = { 0xF5, 0x55};
  const size_t targetLen = sizeof(targetMsg);

  // Read serial input
  while (source.available() && bufferLen < 64) {
    buffer[bufferLen++] = source.read();
    Serial.printf("%02x", buffer[bufferLen-1]);
  } 
  
  // Check for match
  if (bufferLen >= targetLen) {
    for (uint8_t i = 0; i <= bufferLen - targetLen; ++i) {
      if (memcmp(&buffer[i], targetMsg, targetLen) == 0) {
        // Send fixed response

        static const uint8_t response[][10] = {
          { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x01, 0x01 },
          { 0xF5, 0x55, 0x00, 0x02, 0x10, 0x00 },
          { 0xF5, 0x55, 0x00, 0x06, 0x00, 0x13, 0x00, 0x7F, 0x00, 0x08 },
          { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x12, 0x00 },
          { 0xF5, 0x55, 0x00, 0x06, 0x00, 0x26, 0x01, 0x02, 0x00, 0x00 },
          { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x10, 0x01 },
          { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x29, 0x00 },
          { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x28, 0x01 },
          { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x29, 0x00 },
          { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x01, 0x01 },
          { 0xF5, 0x55, 0x00, 0x02, 0x10, 0x00 },
          //code from kb to fix octave being set wrong on guitar neck (not sure why it works that way)
          { 0xF5, 0x55, 0x00, 0x06, 0x00, 0x13, 0x35, 0x60, 0x03, 0x06 },
          { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x12, 0x04 },
          { 0xF5, 0x55, 0x00, 0x06, 0x00, 0x26, 0x01, 0x00, 0x00, 0x00 },
          { 0xF5, 0x55, 0x00, 0x02, 0x10, 0x03 },
        };
        static const uint8_t lengths[] = {
          7, 6, 10, 7, 10, 7, 7, 7, 7, 7, 6,
          10, 7,10,6
        };
        for (uint8_t j = 0; j < sizeof(lengths); ++j) {
          destination.write(response[j], lengths[j]);
        }


        Serial.printf("I faked the device\n");
        
        bufferLen = 0; // Clear after match
        deviceFaked = true;
        return;
      }
    }

    // Shift buffer to discard oldest byte if too large
    if (bufferLen >= 64) {
      memmove(buffer, buffer + 1, --bufferLen);
    }
  }
}

/*
void fakeSerialKBPaddle(HardwareSerial& source, HardwareSerial& destination, char* buffer, uint8_t& bufferLen) {
  const uint8_t targetMsg[] = { 0xF5, 0x55};
  const size_t targetLen = sizeof(targetMsg);

  // Read serial input
  while (source.available() && bufferLen < 64) {
    buffer[bufferLen++] = source.read();
    Serial.printf("%02x", buffer[bufferLen-1]);
  } 
  
  // Check for match
  if (bufferLen >= targetLen) {
    for (uint8_t i = 0; i <= bufferLen - targetLen; ++i) {
      if (memcmp(&buffer[i], targetMsg, targetLen) == 0) {
        // Send fixed response

        static const uint8_t response[][10] = {
            { 0xF5, 0x55, 0x00, 0x02, 0x10, 0x03 },
            { 0xF5, 0x55, 0x00, 0x02, 0x10, 0x00 },
            { 0xF5, 0x55, 0x00, 0x02, 0x10, 0x03 },
            { 0xF5, 0x55, 0x00, 0x02, 0x10, 0x03 },
            { 0xF5, 0x55, 0x00, 0x02, 0x10, 0x00 },
            { 0xF5, 0x55, 0x00, 0x02, 0x10, 0x03 },
            { 0xF5, 0x55, 0x00, 0x02, 0x10, 0x00 },
            { 0xF5, 0x55, 0x00, 0x02, 0x10, 0x00 },
            { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x01, 0x01 },
            { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x01, 0x01 },
            { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x01, 0x01 },
            { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x01, 0x01 },
            { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x01, 0x01 },
            { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x01, 0x01 },
            { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x01, 0x01 },
            { 0xF5, 0x55, 0x00, 0x02, 0x10, 0x00 },
            { 0xF5, 0x55, 0x00, 0x06, 0x00, 0x13, 0x35, 0x60, 0x03, 0x06 },
            { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x12, 0x04 },
            { 0xF5, 0x55, 0x00, 0x06, 0x00, 0x26, 0x01, 0x00, 0x00, 0x00 },
            { 0xF5, 0x55, 0x00, 0x02, 0x10, 0x03 },

          };

          static const uint8_t lengths[] = {
            6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,6,10,7,10,6,7,6,7,7,6,10,7,10,6,7,6
          };

        for (uint8_t j = 0; j < sizeof(lengths); ++j) {
          destination.write(response[j], lengths[j]);
        }


        Serial.printf("I faked the device\n");
        
        bufferLen = 0; // Clear after match
        deviceFaked = true;
        return;
      }
    }

    // Shift buffer to discard oldest byte if too large
    if (bufferLen >= 64) {
      memmove(buffer, buffer + 1, --bufferLen);
    }
  }
}
*/
void loop() {
  if (sendClockTick) {
    sendClockTick = false;
    onTick64();
  }

  static int lastPos = 0;
  static uint8_t midiValue = 5;

  noInterrupts();
  int pos = encoderPosition;
  interrupts();

  if (pos != lastPos) {
    int direction = (pos > lastPos) ? 1 : -1;
    lastPos = pos;

    midiValue = constrain(midiValue + direction, 0, 9);
    //Serial.printf("MIDI Volume %s → %d\n", direction > 0 ? "UP" : "DOWN", midiValue);
  }
  //handling of serial data from Neck KB -> Cyber G's rececived data points
  checkSerialGuitar(Serial1, dataBuffer1, bufferLen1, GUITAR_CHANNEL);
  if (deviceFaked)
  {
    checkSerialKB(Serial2, dataBuffer2, bufferLen2, KEYBOARD_CHANNEL);
    checkSerialG2KB(Serial4, dataBuffer4, bufferLen4);
  }
  else
  {
    fakeSerialKBPaddle(Serial4, Serial2, dataBuffer2, bufferLen2);
  }
  //handling of data received from Cyber G to neck and KB
  checkSerialG2Guitar(Serial3, dataBuffer3, bufferLen3);
  
  //handling serial commands from Client to Teensy
  checkSerialCmd(Serial, dataBuffer4, bufferLen4);  //handling of USB serial
  //checkSerialCmd(Serial5, dataBuffer5, bufferLen5); //uses same as USB serial port, handling passed data
  //Handling serial replies from ESP32s to Teensy
  //checkSerialBTClassic(Serial5, dataBuffer5, bufferLen5); //handling of received data from commands we sent to ESP32
  //checkSerialBTBLEMidi(Serial6, dataBuffer6, bufferLen6); //handling of received data from commands we sent to ESP32

  bool noteOffState = digitalRead(NOTE_OFF_PIN);
  if (noteOffState != prevNoteOffState && noteOffState == LOW) {
    if (debug) {
      Serial.println("NOTE_OFF_PIN pressed → All notes off");
    }
    presetButtonPressed = true;
    noteAllOff();
    //start timer
    if (muteWasRecentlyPressed && millis() - muteHoldTime >= MIN_SWITCH_PASS_TIME) {
      fullPassThroughSerial = !fullPassThroughSerial;
      if (!fullPassThroughSerial) {
        if (debug) {
          Serial.printf("Switching to no pass through\n");
        }

        SequencerNote n;
        n.holdTime = 128;
        n.offset = 1;
        n.relativeOctave = 0;
        n.note = 60;
        n.channel = 0;
        n.velocity = 127;
        CyberGSequencerNotes.push_back(n);
        n.holdTime = 64;
        n.offset = 1;
        n.relativeOctave = 0;
        n.note = 128;
        n.channel = 0;
        n.velocity = 127;
        CyberGSequencerNotes.push_back(n);
        n.holdTime = 128;
        n.offset = 1;
        n.relativeOctave = 0;
        n.note = 67;
        n.channel = 0;
        n.velocity = 127;
        CyberGSequencerNotes.push_back(n);

      } else {
        if (debug)
          Serial.printf("Switching to pass through\n");
      }
    }
    muteWasRecentlyPressed = false;
  } else if (noteOffState != prevNoteOffState && noteOffState == HIGH) {
    if (debug) {
      Serial.println("NOTE_OFF_PIN unpressed");
    }
    presetButtonPressed = false;
    muteWasRecentlyPressed = true;
    muteHoldTime = millis();
  }
  prevNoteOffState = noteOffState;

  bool ccState = digitalRead(START_TRIGGER_PIN);
  if (ccState != prevCCState && ccState == LOW) {
    if (playMode == 1)  //started
    {
      if (debug) {
        Serial.println("START_TRIGGER_PIN pressed → Send Stop");
      }
      sendStop();
      playMode = 0;
    } else {
      if (debug) {
        Serial.println("START_TRIGGER_PIN pressed → Send Start");
      }
      sendStart();
      playMode = 1;
    }

    //
    //start timer
    if (logoWasRecentlyPressed && millis() - logoHoldTime >= MIN_SWITCH_PASS_TIME) {
      if (BTClassicEnabled)
        Serial5.write("BTRA\r\n");
      if (BLEEnabled)
        Serial6.write("BTRA\r\n");

      if (debug) {
        Serial.printf("Disconnecting all devices\n");
      }
      //send cluser
      SequencerNote n;
      n.holdTime = 128;
      n.offset = 1;
      n.relativeOctave = 0;
      n.note = 60;
      n.channel = 0;
      n.velocity = 127;
      CyberGSequencerNotes.push_back(n);
      n.holdTime = 128;
      n.offset = 1;
      n.relativeOctave = 0;
      n.note = 61;
      n.channel = 0;
      n.velocity = 127;
      CyberGSequencerNotes.push_back(n);
      n.holdTime = 128;
      n.offset = 1;
      n.relativeOctave = 0;
      n.note = 62;
      n.channel = 0;
      n.velocity = 127;
      CyberGSequencerNotes.push_back(n);
      n.holdTime = 128;
      n.offset = 1;
      n.relativeOctave = 0;
      n.note = 63;
      n.channel = 0;
      n.velocity = 127;
      CyberGSequencerNotes.push_back(n);
      n.holdTime = 128;
      n.offset = 1;
      n.relativeOctave = 0;
      n.note = 64;
      n.channel = 0;
      n.velocity = 127;
      CyberGSequencerNotes.push_back(n);
    }
    logoWasRecentlyPressed = false;
  } else if (ccState != prevCCState && ccState == HIGH) {
    if (debug) {
      Serial.println("START/Logo unpressed");
    }

    logoWasRecentlyPressed = true;
    logoHoldTime = millis();
  }
  prevCCState = ccState;
  prevButton2State = button2State;
  button2State = digitalRead(BUTTON_2_PIN);
  /*
  if (presetButtonPressed && button2State == LOW)
  {
    if (debug) {
      Serial.println("BT Pairing should be done here\n");
    }
  }
*/
  if (button2State != prevButton2State && button2State == LOW) {
    {
      //if (debug) {
      //Serial.println("Button 2 Pressed");
      //}
      preset += 1;
      if (preset >= MAX_PRESET) {
        preset = 0;
      }
      //play a major interval depending on the preset
      SequencerNote n;
      n.holdTime = 128;
      n.offset = 1;
      n.relativeOctave = 0;
      n.note = 60;
      n.channel = 0;
      n.velocity = 127;
      CyberGSequencerNotes.push_back(n);
      n.holdTime = 128;
      n.offset = 193;
      n.relativeOctave = 0;
      n.note = GetPresetNote(60, preset);
      CyberGSequencerNotes.push_back(n);
      lightAKey(preset);
      presetChanged();

      //add preset handling here
      if (debug) {
        Serial.printf("Preset is now %d\n", preset);
      }
    }
  }
  usbMIDI.read();
  myusb.Task();  // Process incoming USB messages
  readExtUSBUnhandledMIDI();

  if (!fullPassThroughSerial) {
    //send fake off notes

    if (millis() - lastNotePressTime < NOTE_PASS_TIMEOUT && millis() - lastFakeNotePlayTime > FAKENOTETIME) {
      SequencerNote n;
      n.holdTime = 64;
      n.offset = 1;
      n.relativeOctave = 0;
      n.note = 60;
      n.channel = 0;
      n.velocity = 0;
      CyberGSequencerNotes.push_back(n);
      lastFakeNotePlayTime = millis();
    }
  }
  /*
  //turn off notes playing when changing to none pass through mode
  if (toMuteAfterSwitching && (millis() - toMuteAfterSwitchingTime > MUTE_NOTE_AFTER_SWITCH_TIME))
  {
    toMuteAfterSwitching = false;
    int sizeN = 0;
    char * fakeNotes = getFakeNotesKB(false, &sizeN);
    Serial2.write(fakeNotes, sizeof(sizeN));
    free(fakeNotes);
  }
  */
  //updateButtonPressedStates();
  delayMicroseconds(5);
}

void printMemoryUsage() {
  if (!debug) {
    return;
  }
  char stack_dummy = 0;

  uintptr_t heap_end = (uintptr_t)sbrk(0);        // Heap grows upward
  uintptr_t stack_ptr = (uintptr_t)&stack_dummy;  // Stack grows downward

  Serial.println("---- Teensy 4.1 RAM Usage ----");

  // DTCM (Stack, Globals)
  Serial.print("Stack Ptr (DTCM): 0x");
  Serial.println(stack_ptr, HEX);
  Serial.print("Used Stack (approx): ");
  Serial.print(0x20080000 - stack_ptr);
  Serial.println(" bytes");

  // OCRAM (Heap / malloc)
  Serial.print("Heap End (OCRAM): 0x");
  Serial.println(heap_end, HEX);
  Serial.print("Used Heap: ");
  Serial.print(heap_end - 0x20200000);
  Serial.println(" bytes");

  // Free in each region
  Serial.print("Free OCRAM Heap: ");
  Serial.print(0x20260000 - heap_end);
  Serial.println(" bytes");

  Serial.print("Free DTCM Stack: ");
  Serial.print(stack_ptr - 0x20000000);
  Serial.println(" bytes");

  Serial.println("------------------------------\n");
}