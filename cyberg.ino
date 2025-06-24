#include <usb_midi.h>
#include <vector>
#include <SD.h>
//defines
#define OMNICHORD_TRANSPOSE_MAX 2
#define OMNICHORD_TRANSPOSE_MIN -2
#define GUITAR_TRANSPOSE_MAX 12
#define GUITAR_TRANSPOSE_MIN -12
#define GUITAR_PALMING_DELAY_MIN 0
#define GUITAR_PALMING_DELAY_MAX 50
#define QUARTERNOTETICKS 192
#define INDEFINITE_HOLD_TIME 65535
#define MAX_BPM 300
#define MIN_BPM 50 //lowest BPM 
#define MAX_PRESET 6
#define NECK_COLUMNS 3
#define NECK_ROWS 7
#define SEMITONESPEROCTAVE 12
#define DEBUG true
#define DEBUGNOTEPRINT false
#define MAX_CUSTOM_PATTERNS 3
#define MAX_BASS_PATTERNS 1
#define MAX_STRUM_SEPARATION 100
#define MIN_STRUM_SEPARATION 0
#define MIN_IGNORED_GUITAR 21
#define MAX_IGNORED_GUITAR 26
#define BPM_COMPUTE_BUTTON 25
#define DRUM_STARTSTOP_BUTTON 26
#define DRUM_FILL_BUTTON 24
#define SUSTAIN_BUTTON 22
#define MIDI_BUTTON_CHANNEL_NOTE_OFFSET 0
#define BT_PRESS_ACTIVATE_COUNT 5

#define DRUM_INTRO_ID 0
#define DRUM_LOOP_ID 1
#define DRUM_LOOPHALF_ID 2
#define DRUM_FILL_ID 3
#define DRUM_END_ID 4

#define MAX_VELOCITY 127
#define MAX_NOTE 127
#define REST_NOTE 255

#define MAX_OFFSET 2047
#define MAX_HOLDTIME 768

//#define USE_AND
// --- PIN DEFINITIONS ---
#define NOTE_OFF_PIN 10       // Digital input for turning off all notes
#define START_TRIGGER_PIN 11  // Digital input for triggering CC message
#define BT_ON_PIN 19
#define BT_STATUS_PIN 20
#define BUTTON_1_PIN 4  //unused due to hardware issues (device turns on)
#define BUTTON_2_PIN 5
#define BUTTON_3_PIN 6  //unused due to hardware issues (device hangs)

// --- MIDI CONSTANTS ---
#define BASS_CHANNEL 4
#define GUITAR_BUTTON_CHANNEL 3
#define GUITAR_CHANNEL 2
#define KEYBOARD_CHANNEL 1
#define DRUM_CHANNEL 10

#define MAX_STRUM_STYLE 3
// Other constants
//#define ACTUAL_NECKBUTTONS 27
//#define NECKBUTTONS 21

extern "C" char* sbrk(int incr);  // Get current heap end

struct noteShift {
  uint8_t paddleNote;
  uint8_t assignedNote;
};

enum DrumState{
  DrumNone = -1, //for next only to indicate nothing next is in queue
  DrumStopped = 0,
  DrumIntro,
  DrumLoop, //expects a full bar worth at the end unless using half bar.
  DrumLoopHalfBar, //optional half bar end that fill replaces
  DrumLoopFill,
  DrumEnding,
};
enum StrumStyleType {
  SimpleStrum = 0, //Either guitar or piano chord
  AutoStrum = 1, //does a specific pattern automatically
  ManualStrum = 2, //user will have to press or strum to get next pattern
};
class SequencerNote {
public:
  uint16_t holdTime;
  uint16_t offset;
  uint8_t note;
  uint8_t channel;
  uint8_t velocity;
  SequencerNote()
  {
    note = 0;
    holdTime = 0;
    offset = 0;
    channel = -1;
    velocity = MAX_VELOCITY;
  }
  SequencerNote(uint8_t newNote, int newholdTime, uint16_t newoffset, uint8_t newchannel, uint8_t newvelocity = MAX_VELOCITY) {
    note = newNote;
    holdTime = newholdTime;
    offset = newoffset;
    channel = newchannel;
    velocity = newvelocity;
  }
};
class Chord {
public:
  uint8_t rootNote;
  // Constructor that accepts a vector of relative notes
  Chord() {
    rootNote = 0;
  }
  Chord(const std::vector<uint8_t>& relativeNotes)
    : notes(relativeNotes) {
    rootNote = 0;
  }

  // Get the relative notes of the chord
  std::vector<uint8_t> getChordNotes() const {
    return notes;
  }
  std::vector<uint8_t> getCompleteChordNotes() const {
    std::vector<uint8_t> complete;
    complete.push_back(rootNote);
    for (uint8_t i = 0; i < notes.size(); i++) {
      complete.push_back(notes[i] + rootNote);
    }
    return complete;
  }
  std::vector<uint8_t> getCompleteChordNotesNo5() const {
    std::vector<uint8_t> complete;
    complete.push_back(rootNote);
    bool skip;
    for (uint8_t i = 0; i < notes.size(); i++) {
      skip = false;
      if (notes.size() > 2 && i == 1) //do not add 5th which is on 1
      {
        skip = true;
      }
      else if (notes.size() > 3 && i != notes.size() - 1) //assume 3rd and last not only
      {
        skip = true; //only get last
      }
      if (!skip)
      {
        complete.push_back(notes[i] + rootNote);
      }
    }
    return complete;
  }
  uint8_t getRootNote() const {
    return rootNote;
  }
  void setRootNote(uint8_t newNote) {
    rootNote = newNote;
  }
  // Print the relative chord's notes
  void printChordInfo() const {
    Serial.print("Chord Notes: ");
    for (uint8_t note : notes) {
      Serial.print(note);
      Serial.print(" ");
    }
    Serial.println();
  }

  std::vector<uint8_t> notes;  // A vector of intervals relative to the root note
};

/*
class PatternNote {
public:
  uint8_t degree; //actual relative note
  uint16_t holdTime; // how long note is held
  uint16_t offset; //when it will be played, 1 = played next
  uint8_t velocity;

  PatternNote() {
    degree = 255;
    holdTime = 65535;
    offset = 65535;

    velocity = 127;
  }
  PatternNote(uint8_t newDegree, int newholdTime, uint16_t newoffset, uint8_t newvelocity = 127) {
    degree = newDegree;
    holdTime = newholdTime;
    offset = newoffset;
    velocity = newvelocity;
  }
  std::vector<SequencerNote> GetSequencerNote(Chord basis, std::vector<PatternNote>& SequencerPattern, uint8_t channel) {
    std::vector<SequencerNote> result;
    std::vector<uint8_t> notes;
    notes.push_back(basis.rootNote);

    for (uint8_t i = 0; i < basis.notes.size(); i++) {
      notes.push_back(basis.notes[i]);
    }
    uint8_t chordSize = notes.size();
    for (uint8_t i = 0; i < SequencerPattern.size(); i++) {
      if (SequencerPattern[i].degree <= chordSize)  //1 base
      {
        SequencerNote n(notes[SequencerPattern[i].degree - 1], SequencerPattern[i].holdTime, SequencerPattern[i].offset, channel, SequencerPattern[i].velocity);
        result.push_back(n);
      } else {
        //ignore or assume error
      }
    }
    return result;
  }
};

*/


std::vector<SequencerNote> SequencerNotes; //queue of guitar notes
std::vector<SequencerNote> StaggeredSequencerNotes; //queue of staggered guitar notes

//bass could be in ram 2
std::vector<SequencerNote> BassSequencerNotes;  //queue of bass notes assume monophonic
std::vector<SequencerNote> BassSequencerPattern;
//patterns in terms of Sequencer note

std::vector<SequencerNote> SequencerPatternA;
std::vector<SequencerNote> SequencerPatternB;
std::vector<SequencerNote> SequencerPatternC;

//probably store data of drums in Ram2
std::vector<SequencerNote> DrumLoopSequencer; // drum loop pattern/drum pattern
std::vector<SequencerNote> DrumLoopHalfBarSequencer; // drum loop pattern bar sequencer
std::vector<SequencerNote> DrumFillSequencer; // drum fill pattern
std::vector<SequencerNote> DrumIntroSequencer; // drum Intro pattern
std::vector<SequencerNote> DrumEndSequencer; // drum Intro pattern
std::vector<SequencerNote> DrumSequencerNotes; //queue of drum notes

std::vector<SequencerNote> * getGuitarPattern(uint8_t pattern)
{
  switch (pattern)
  {
    case 0:
      return &SequencerPatternA;
    case 1:
      return &SequencerPatternB;
    default:
      return &SequencerPatternC;
  }
}

std::vector<SequencerNote> * getBassPattern(uint8_t pattern)
{
  switch (pattern)
  {
    default:
      return &BassSequencerPattern;
  }
}

std::vector<SequencerNote> * getDrumPattern(uint8_t pattern)
{
  switch (pattern)
  {
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

void addEntryToPattern(std::vector<SequencerNote> *pattern, uint8_t note, uint16_t offset, uint16_t holdTime, uint8_t velocity, uint8_t channel)
{
  if (pattern == NULL)
  {
    Serial.printf("Error! Pattern passed is null!\n");
  }
  SequencerNote sn;
  sn.note = note;
  sn.offset = offset;
  sn.holdTime = holdTime;
  sn.velocity = velocity;
  sn.channel = channel;
  pattern->push_back(sn);
}

void sendPatternData(std::vector<SequencerNote> *pattern, bool isSerialOut = true)
{
  if (pattern == NULL)
  {
    Serial.printf("Error! Pattern passed is null!\n");
  }
  SequencerNote sn;
  
  char buffer[32];
  for (uint8_t i = 0; i < pattern->size(); i++)
  {
    sn = pattern->at(i);
    snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d,%d\n", sn.note, sn.offset, sn.holdTime, sn.velocity, sn.channel);
    if (!isSerialOut)
    {
      Serial.printf("%s", buffer);  
    }
    else
    {
      Serial.write(buffer);
    }
  }
}
class AssignedPattern {

public:
  AssignedPattern(uint8_t strumStyle, Chord toAssign, std::vector<uint8_t> newGuitarChord, uint8_t rootNote, bool ignored, uint8_t pattern) {
    //patternAssigned = 0;
    assignedChord = toAssign;
    assignedChord.setRootNote(rootNote);
    assignedGuitarChord = newGuitarChord;
    isIgnored = ignored;
    customPattern = pattern;  //actual auto/manual pattern for neck press
  }
  int customPattern; //strum pattern
  bool isIgnored;
  //uint8_t patternAssigned; //strum pattern assigned 
  Chord assignedChord;
  std::vector<uint8_t> assignedGuitarChord;
  void setChords(Chord newChord) {
    assignedChord = newChord;
  }
  void setGuitarChords(std::vector<uint8_t> newGuitarChord) {
    assignedGuitarChord = newGuitarChord;
  }
  Chord getChords() const {
    return assignedChord;
  }
  Chord getGuitarChords() const {
    return assignedGuitarChord;
  }
  //uint8_t getPatternAssigned() const {
//    return patternAssigned;
//  }
  bool getIgnored() const {
    return isIgnored;
  }
};




//state
bool buttonPressedChanged = false; //flag for kb know that fret button pressed was changed
bool isSustain = false; //actual sustain state
bool isSustainPressed = false; //sustain button is still pressed
bool bassStart = false;
DrumState drumState = DrumStopped;
DrumState drumNextState = DrumNone;
bool isStrumUp = false; // used for alternating strumming order
//int strum; 
//int lastStrum;
bool ignoringIdlePing = false;
int tickCount = 0;
int transpose = 0; //piano
uint8_t preset = 0;
int curProgram = 0;
uint8_t playMode = 0;
int neckButtonPressed = -1;
int lastNeckButtonPressed = -1;
int lastValidNeckButtonPressed = -1; 
int lastValidNeckButtonPressed2 = -1; //for use with non guitar serial
int lastBassNeckButtonPressed = -1;
bool isKeyboard = false;
bool prevButtonBTState = HIGH;
bool button2State = HIGH;
bool prevButton1State = HIGH;
bool prevButton2State = HIGH;
bool prevButton3State = HIGH;
volatile bool sendClockTick = false;
IntervalTimer tickTimer;
int lastTransposeValueDetected = 0;

//config

bool presetButtonPressed = false;
bool debug = DEBUG;
bool stopSoundsOnPresetChange = true;
bool midiClockEnable = true;
//uint16_t deviceBPM = 128;
std::vector<bool> enableAllNotesOnChords;
std::vector<bool> enableButtonMidi;
std::vector<uint8_t> omniChordModeGuitar; //omnichord modes
std::vector<bool> muteWhenLetGo;
std::vector<bool> ignoreSameChord;
std::vector<uint16_t> presetBPM;

std::vector<bool> chordHold;
std::vector<bool> alternateDirection;
std::vector<bool> useToggleSustain;
std::vector<int8_t> guitarTranspose; //capo setting
std::vector<int8_t> omniKBTransposeOffset;  //used to determine if omnichord keyboard is shifted up or down by octave
std::vector<uint8_t> simpleChordSetting; //strum style
std::vector<uint8_t> strumSeparation;             //time unit separation between notes //default 1
std::vector<uint8_t> muteSeparation;             //time unit separation between muting notes //default 1
std::vector<bool> drumsEnabled;
std::vector<bool> bassEnabled;

unsigned int computeTickInterval(int bpm) {
  // 60,000,000 microseconds per minute / (BPM * 192 ticks per quarter note)
  return ((60000 / bpm)/(QUARTERNOTETICKS*1.0))*1000;
}

std::vector<uint8_t> omniChordOrigNotes = { 65, 67, 69, 71, 72, 74, 76, 77, 79, 81, 83, 84 };
std::vector<uint8_t> omniChordNewNotes;

class TranspositionDetector {

  uint8_t lowest = omniChordOrigNotes[0];
  uint8_t highest = omniChordOrigNotes[omniChordOrigNotes.size() - 1];
  int offset = 0;
  int octaveShift = 0;

public:
  void transposeUp() {
    octaveShift += 1;
    if (octaveShift > 2) {
      octaveShift = 2;
    }
  }
  void transposeDown() {
    octaveShift -= 1;
    if (octaveShift < -2) {
      octaveShift = -2;
    }
  }
  void transposeReset() {
    octaveShift = 0;
  }

  void noteOn(int midiNote) {
    if (midiNote < lowest) {
      if (debug) {
        Serial.printf("New Low = Lowest %d Old %d high %d\n", midiNote, lowest, highest);
      }
      //53 is found instead of 65
      offset = omniChordOrigNotes[0] - midiNote;  //12
      lowest = midiNote;
      highest = omniChordOrigNotes[omniChordOrigNotes.size() - 1] - offset;  //84 -12
      if (debug) {
        Serial.printf("offset %d\n", offset);
      }
      //highest = midiNote + 12;
    } else if (midiNote > highest) {
      if (debug) {
        Serial.printf("New High = Highest %d Old %d low %d\n", midiNote, highest, lowest);
      }
      //96 is found instead of 84
      offset = midiNote - omniChordOrigNotes[omniChordOrigNotes.size() - 1];  //12
      highest = midiNote;
      lowest = omniChordOrigNotes[0] + offset;
      if (debug) {
        Serial.printf("offset %d\n", offset);
      }
      //lowest = midiNote - 12;
    }
  }

  uint8_t getBestNote(uint8_t note) {
    //given a transpose
    //uint8_t newNote = note - transpose;
    uint8_t newNote = note - offset;
    uint8_t bestNote = 0;
    bool found = false;
    // assumption is newNote is reverted to its original form
    for (uint8_t i = 0; i < omniChordOrigNotes.size() && !found; i++) {
      if (i + 1 >= (int)omniChordOrigNotes.size()) {
        //best Note is last note
        found = true;
        bestNote = omniChordNewNotes[i];
      } else if (omniChordOrigNotes[i] == newNote) {
        bestNote = omniChordNewNotes[i];
        found = true;
      }
      //assume if in between, use current note
      else if (i + 1 < (int)omniChordOrigNotes.size() && omniChordOrigNotes[i] < newNote && omniChordOrigNotes[i + 1] > newNote) {
        bestNote = omniChordNewNotes[i];
        found = true;
      } else {
        //do nothing
      }
    }
    return bestNote + SEMITONESPEROCTAVE * octaveShift;
  }
  int getBestTranspose() const {
    return offset;
  }
};

// --- STATE TRACKING ---
bool b2Ignored = false;
bool prevNoteOffState = LOW;
bool prevCCState = LOW;
std::vector<uint8_t> lastGuitarNotes;
std::vector<uint8_t> lastGuitarNotesButtons;
std::vector<struct noteShift> lastOmniNotes;
#define MAX_BUFFER_SIZE 150
uint8_t bufferLen2 = 0;
char dataBuffer1[MAX_BUFFER_SIZE + 1];  // +1 for null terminator
char dataBuffer2[MAX_BUFFER_SIZE + 1];  // +1 for null terminator
char dataBuffer3[MAX_BUFFER_SIZE + 1];  // +1 for null terminator
char dataBuffer4[MAX_BUFFER_SIZE + 1];  // +1 for null terminator
uint8_t bufferLen1 = 0;
uint8_t bufferLen3 = 0;
uint8_t bufferLen4 = 0;
bool lastSimple = false;
TranspositionDetector detector;

// --- HEX TO MIDI NOTE MAP ---
struct MidiMessage {
  const char* hex;
  uint8_t note;
};
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


MidiMessage hexToNote[] = {
  { "aa55000a22010000000000", 255 },  //note off
  { "aa55000a22010000000400", 0 },    // C1
  { "aa55000a22010000000200", 1 },    // C2
  { "aa55000a22010000000100", 2 },    // C3
  { "aa55000a22010000002000", 3 },    // D1
  { "aa55000a22010000001000", 4 },    // D2
  { "aa55000a22010000000800", 5 },    // D3
  { "aa55000a22010000010000", 6 },    // E1
  { "aa55000a22010000008000", 7 },    // E2
  { "aa55000a22010000004000", 8 },    // E3
  { "aa55000a22010000080000", 9 },    // F1
  { "aa55000a22010000040000", 10 },   // F2
  { "aa55000a22010000020000", 11 },   // F3
  { "aa55000a22010000400000", 12 },   // G1
  { "aa55000a22010000200000", 13 },   // G2
  { "aa55000a22010000100000", 14 },   // G3
  { "aa55000a22010002000000", 15 },   // A1
  { "aa55000a22010001000000", 16 },   // A2
  { "aa55000a22010000800000", 17 },   // A3
  { "aa55000a22010010000000", 18 },   // B1
  { "aa55000a22010008000000", 19 },   // B2
  { "aa55000a22010004000000", 20 },   // B3
  { "aa55000a22010080000000", 21 },   // C4
  { "aa55000a22010040000000", 22 },   // C5
  { "aa55000a22010020000000", 23 },   // C6
  { "aa55000a22010400000000", 24 },   // D4
  { "aa55000a22010200000000", 25 },   // D5
  { "aa55000a22010100000000", 26 }    // D6
};

struct HexToProgram {
  const char* hex;
  uint8_t program;
};

HexToProgram hexToProgram[] = {
  { "aa550003220200d8", 3 },  // Rocker Up
  { "aa5500032202ffd9", 9 },  // Rocker Down
};

struct HexToControl {
  const char* hex;
  uint8_t cc;
};


HexToControl hexToControl[] = {
  { "f5550003201401", 2 },  // Right
  { "f5550003201402", 1 },  // Left
  { "f5550003201403", 5 },  // Both
  { "f5550003201000", 4 },  // Circle not Lit
  { "f5550003201001", 3 },  // Circle Lit

  { "f5550003201400", 0 }  //ignore
};

Chord majorChord({ 4, 7 });
Chord minorChord({ 3, 7 });
Chord diminishedChord({ 3, 6 });
Chord augmentedChord({ 4, 8 });
Chord major7thChord({ 4, 7, 11 });
Chord minor7thChord({ 3, 7, 10 });
Chord dominant7thChord({ 4, 7, 10 });
Chord minor7Flat5Chord({ 3, 6, 10 });
Chord major6thChord({ 4, 7, 9 });
Chord minor6thChord({ 3, 7, 9 });
Chord suspended2Chord({ 2, 7 });
Chord suspended4Chord({ 5, 7 });
Chord add9Chord({ 4, 7, 2 });
Chord add11Chord({ 4, 7, 11 });

Chord ninthChord({ 4, 7, 10, 14 });
Chord eleventhChord({ 4, 7, 10, 14, 17 });
Chord thirteenthChord({ 4, 7, 10, 14, 17, 21 });

std::vector<Chord> chords = {
  majorChord,
  minorChord,
  diminishedChord,
  augmentedChord,
  major7thChord,
  minor7thChord,
  dominant7thChord,
  minor7Flat5Chord,
  major6thChord,
  minor6thChord,
  suspended2Chord,
  suspended4Chord,
  add9Chord,
  add11Chord,
  ninthChord,
  eleventhChord,
  thirteenthChord
};

std::vector<std::vector<uint8_t>> majorChordGuitar = {
  { 48, 52, 55, 60, 64 },      // C Major x
  { 49, 56, 61, 65, 68 },      // C# Major x
  { 50, 57, 62, 66 },          // D Major x
  { 51, 58, 63, 67, 70 },      // D# Major x
  { 40, 47, 52, 56, 59, 64 },  // E Major x
  { 41, 48, 53, 57, 60, 65 },  // F Major x
  { 42, 49, 54, 58, 61, 66 },  // F# Major x
  { 43, 47, 50, 55, 59, 67 },  // G Major x
  { 44, 51, 56, 60, 63, 68 },  // G# Major x
  { 45, 52, 57, 61, 64 },      // A Major x
  { 46, 50, 58, 65, 70 },      // A# Major x
  { 47, 54, 59, 63, 66 },      // B Major x
};

std::vector<std::vector<uint8_t>> minorChordGuitar = {
  { 48, 51, 60, 63, 67 },      // C Minor x
  { 49, 54, 61, 64, 68 },      // C# Minor x
  { 50, 57, 62, 65 },          // D Minor x
  { 51, 58, 63, 66, 70 },      // D# Minor x
  { 40, 47, 52, 55, 59, 64 },  // E Minor x
  { 41, 48, 53, 56, 60 },      // F Minor x
  { 42, 49, 54, 57, 61 },      // F# Minor x lowered
  { 43, 50, 55, 58, 62 },      // G Minor x lowered
  { 44, 51, 56, 59, 63 },      // G# Minor x lowered
  { 45, 52, 57, 60, 64 },      // A Minor x
  { 46, 53, 58, 61, 65 },      // A# Minor x
  { 47, 54, 59, 62, 66 },      // B Minor x
};

std::vector<std::vector<uint8_t>> diminishedChordGuitar = {
  { 60, 63, 66 },  // C Diminished x
  { 61, 64, 67 },
  { 62, 65, 68 },
  { 63, 66, 69 },
  { 64, 67, 70 },
  { 65, 68, 71 },
  { 66, 69, 72 },
  { 67, 70, 73 },
  { 68, 71, 74 },
  { 69, 72, 75 },
  { 70, 73, 76 },
  { 71, 74, 77 },

};

std::vector<std::vector<uint8_t>> augmentedChordGuitar = {
  { 48, 52, 56, 60, 64 },  // C Augmented x
  { 49, 53, 57, 61, 65 },
  { 50, 54, 58, 62, 66 },
  { 51, 55, 59, 63, 67 },
  { 52, 56, 60, 64, 68 },
  { 53, 57, 61, 65, 69 },
  { 54, 58, 62, 66, 70 },
  { 55, 59, 63, 67, 71 },
  { 56, 60, 64, 68, 72 },
  { 57, 61, 65, 69, 73 },
  { 58, 62, 66, 70, 74 },
  { 59, 63, 67, 71, 75 },
};

std::vector<std::vector<uint8_t>> major7thChordGuitar = {
  { 48, 52, 55, 71, 64 },  // C Major7 x
  { 49, 53, 56, 72, 65 },
  { 50, 54, 57, 73, 66 },
  { 51, 55, 58, 74, 67 },
  { 52, 56, 59, 75, 68 },
  { 53, 57, 60, 76, 69 },
  { 54, 58, 61, 77, 70 },
  { 55, 59, 62, 78, 71 },
  { 56, 60, 63, 79, 72 },
  { 57, 61, 64, 80, 73 },
  { 58, 62, 65, 81, 74 },
  { 59, 63, 66, 82, 75 },

};

std::vector<std::vector<uint8_t>> minor7thChordGuitar = {
  { 48, 55, 58, 63, 67 },  // C Minor7 x
  { 49, 56, 59, 64, 68 },
  { 50, 57, 60, 65, 69 },
  { 51, 58, 61, 66, 70 },
  { 52, 59, 62, 67, 71 },
  { 53, 60, 63, 68, 72 },
  { 54, 61, 64, 69, 73 },
  { 55, 62, 65, 70, 74 },
  { 56, 63, 66, 71, 75 },
  { 57, 64, 67, 72, 76 },
  { 58, 65, 68, 73, 77 },
  { 59, 66, 69, 74, 78 },

};

std::vector<std::vector<uint8_t>> dominant7thChordGuitar = {
  { 48, 52, 58, 60, 64 },      // C Dominant7 x
  { 49, 56, 59, 65, 68 },      // C# Dominant7 x
  { 50, 57, 60, 66 },          // D Dominant7 x
  { 51, 58, 61, 67, 68 },      // D# Dominant7 x
  { 40, 47, 52, 56, 62, 64 },  // E Dominant7 x
  { 41, 48, 51, 57, 60 },      // F Dominant7 x lowered
  { 42, 46, 49, 52 },          // F# Dominant7 x lowered
  //{55, 59, 62, 55, 59, 65},   // G Dominant7 x
  { 43, 47, 50, 55, 59, 65 },  // G Dominant7 x lowered
  { 56, 63, 66, 72, 75 },      // G# Dominant7 x
  { 45, 52, 55, 61, 64 },      // A Dominant7 x
  { 46, 53, 56, 62, 65 },      // A# Dominant7 x
  { 47, 51, 57, 59, 66 },      // B Dominant7 x
};

std::vector<std::vector<uint8_t>> minor7Flat5ChordGuitar = {
  { 48, 51, 58, 60, 66 },  // C Minor7Flat5 x
  { 49, 52, 59, 61, 67 },
  { 50, 53, 60, 62, 68 },
  { 51, 54, 61, 63, 69 },
  { 52, 55, 62, 64, 70 },
  { 53, 56, 63, 65, 71 },
  { 54, 57, 64, 66, 72 },
  { 55, 58, 65, 67, 73 },
  { 56, 59, 66, 68, 74 },
  { 57, 60, 67, 69, 75 },
  { 58, 61, 68, 70, 76 },
  { 59, 62, 69, 71, 77 },
};

std::vector<std::vector<uint8_t>> major6thChordGuitar = {
  { 48, 52, 57, 60, 64 },  // C Major6 x
  { 49, 53, 58, 61, 65 },
  { 50, 54, 59, 62, 66 },
  { 51, 55, 60, 63, 67 },
  { 52, 56, 61, 64, 68 },
  { 53, 57, 62, 65, 69 },
  { 54, 58, 63, 66, 70 },
  { 55, 59, 64, 67, 71 },
  { 56, 60, 65, 68, 72 },
  { 57, 61, 66, 69, 73 },
  { 58, 62, 67, 70, 74 },
  { 59, 63, 68, 71, 75 },

};

std::vector<std::vector<uint8_t>> minor6thChordGuitar = {
  { 48, 51, 57, 60, 67 },  // C Minor6 x
  { 49, 52, 58, 61, 68 },
  { 50, 53, 59, 62, 69 },
  { 51, 54, 60, 63, 70 },
  { 52, 55, 61, 64, 71 },
  { 53, 56, 62, 65, 72 },
  { 54, 57, 63, 66, 73 },
  { 55, 58, 64, 67, 74 },
  { 56, 59, 65, 68, 75 },
  { 57, 60, 66, 69, 76 },
  { 58, 61, 67, 70, 77 },
  { 59, 62, 68, 71, 78 },

};

std::vector<std::vector<uint8_t>> suspended2ChordGuitar = {
  { 48, 50, 55, 62, 67 },  // C Suspended2 x
  { 49, 51, 56, 63, 68 },
  { 50, 52, 57, 64, 69 },
  { 51, 53, 58, 65, 70 },
  { 52, 54, 59, 66, 71 },
  { 53, 55, 60, 67, 72 },
  { 54, 56, 61, 68, 73 },
  { 55, 57, 62, 69, 74 },
  { 56, 58, 63, 70, 75 },
  { 57, 59, 64, 71, 76 },
  { 58, 60, 65, 72, 77 },
  { 59, 61, 66, 73, 78 },

};

std::vector<std::vector<uint8_t>> suspended4ChordGuitar = {
  { 48, 53, 55, 60, 65 },      // C Suspended4 x
  { 49, 56, 61, 66, 68 },      // C# Suspended4 x
  { 50, 57, 62, 67 },          // D Suspende d4 x
  { 51, 58, 63, 68, 70 },      // D# Suspended4 x
  { 40, 47, 52, 57, 59, 64 },  // E Suspended4 x
  { 53, 60, 65, 70, 72 },      // F Suspended4 x
  { 54, 61, 66, 71, 73 },      // F# Suspended4 x
  { 43, 50, 55, 60, 67 },      // G Suspended4 x
  { 56, 63, 68, 73, 75 },      // G# Suspended4 x
  { 45, 52, 57, 62, 64 },      // A Suspended4 x
  { 46, 53, 58, 63, 65 },      // A# Suspended4 x
  { 47, 54, 59, 64 },          // B Suspended4, removed extra 59 x
};

std::vector<std::vector<uint8_t>> add9ChordGuitar = {
  { 48, 52, 55, 62, 64 },  // C Add9 x
  { 49, 53, 56, 63, 65 },
  { 50, 54, 57, 64, 66 },
  { 51, 55, 58, 65, 67 },
  { 52, 56, 59, 66, 68 },
  { 53, 57, 60, 67, 69 },
  { 54, 58, 61, 68, 70 },
  { 55, 59, 62, 69, 71 },
  { 56, 60, 63, 70, 72 },
  { 57, 61, 64, 71, 73 },
  { 58, 62, 65, 72, 74 },
  { 59, 63, 66, 73, 75 },

};

std::vector<std::vector<uint8_t>> add11ChordGuitar = {
  { 48, 52, 55, 60, 65 },  // C Add11 x
  { 50, 54, 57, 62, 67 },
  { 51, 55, 58, 63, 68 },
  { 52, 56, 59, 64, 69 },
  { 53, 57, 60, 65, 70 },
  { 54, 58, 61, 66, 71 },
  { 55, 59, 62, 67, 72 },
  { 56, 60, 63, 68, 73 },
  { 57, 61, 64, 69, 74 },
  { 58, 62, 65, 70, 75 },
  { 59, 63, 66, 71, 76 },

};

std::vector<std::vector<uint8_t>> ninthChordGuitar = {
  { 48, 55, 58, 64, 70, 74 },  // C Ninth x
  { 49, 56, 59, 65, 71, 75 },
  { 50, 57, 60, 66, 72, 76 },
  { 51, 58, 61, 67, 73, 77 },
  { 52, 59, 62, 68, 74, 78 },
  { 53, 60, 63, 69, 75, 79 },
  { 54, 61, 64, 70, 76, 80 },
  { 55, 62, 65, 71, 77, 81 },
  { 56, 63, 66, 72, 78, 82 },
  { 57, 64, 67, 73, 79, 83 },
  { 58, 65, 68, 74, 80, 84 },
  { 59, 66, 69, 75, 81, 85 },

};

std::vector<std::vector<uint8_t>> eleventhChordGuitar = {
  { 43, 53, 58, 60, 64 },  // C Eleventh x, but added E4
  { 44, 54, 59, 61, 65 },
  { 45, 55, 60, 62, 66 },
  { 46, 56, 61, 63, 67 },
  { 47, 57, 62, 64, 68 },
  { 48, 58, 63, 65, 69 },
  { 49, 59, 64, 66, 70 },
  { 50, 60, 65, 67, 71 },
  { 51, 61, 66, 68, 72 },
  { 52, 62, 67, 69, 73 },
  { 53, 63, 68, 70, 74 },
  { 54, 64, 69, 71, 75 },

};

std::vector<std::vector<uint8_t>> thirteenthChordGuitar = {
  { 60, 62, 64, 57, 58 },  // C Thirteenth X but AI
  { 61, 63, 65, 58, 59 },
  { 62, 64, 66, 59, 60 },
  { 63, 65, 67, 60, 61 },
  { 64, 66, 68, 61, 62 },
  { 65, 67, 69, 62, 63 },
  { 66, 68, 70, 63, 64 },
  { 67, 69, 71, 64, 65 },
  { 68, 70, 72, 65, 66 },
  { 69, 71, 73, 66, 67 },
  { 70, 72, 74, 67, 68 },
  { 71, 73, 75, 68, 69 },

};

//[chordtype][key] = actual note vector
std::vector<std::vector<std::vector<uint8_t>>> allChordsGuitar = {
  majorChordGuitar,
  minorChordGuitar,
  diminishedChordGuitar,
  augmentedChordGuitar,
  major7thChordGuitar,
  minor7thChordGuitar,
  dominant7thChordGuitar,
  minor7Flat5ChordGuitar,
  major6thChordGuitar,
  minor6thChordGuitar,
  suspended2ChordGuitar,
  suspended4ChordGuitar,
  add9ChordGuitar,
  add11ChordGuitar,
  ninthChordGuitar,
  eleventhChordGuitar,
  thirteenthChordGuitar
};

//[preset][buttonpressed] //combines patterns for paddle and piano mode
std::vector<std::vector<AssignedPattern>> assignedFretPatternsByPreset;
//
void preparePatterns() {
  SequencerPatternA.clear();
  SequencerPatternB.clear();
  SequencerPatternC.clear();
  BassSequencerPattern.clear();
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
  OmniChordOffType = 0,   //no omnichord
  OmniChordStandardType,  //needs button to be held
  OmniChordStandardHoldType,
  OmniChordGuitarType,  //requires paddle
};

enum ChordType {
  majorChordType = 0,
  minorChordType,
  diminishedChordType,
  augmentedChordType,
  major7thChordType,
  minor7thChordType,
  dominant7thChordType,
  minor7Flat5ChordType,
  major6thChordType,
  minor6thChordType,
  suspended2ChordType,
  suspended4ChordType,
  add9ChordType,
  add11ChordType,
  ninthChordType,
  eleventhChordType,
  thirteenthChordType
};

enum Note {
  NO_NOTE = -1,
  C_NOTE = 0,
  CSharp_NOTE,
  D_NOTE,
  DSharp_NOTE,
  E_NOTE,
  F_NOTE,
  FSharp_NOTE,
  G_NOTE,
  GSharp_NOTE,
  A_NOTE,
  ASharp_NOTE,
  B_NOTE
};
class neckAssignment {
public:
  Note key;
  ChordType chordType;
  neckAssignment() {
    key = NO_NOTE;
    chordType = majorChordType;
  }
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
  neckAssignment n = neckAssignments[usePreset][row * NECK_COLUMNS + column];
  //Serial.printf("GT row %d col %d preset %d\n",row, column, usePreset);
  //Serial.printf("ChordType GT to chord = %d\n",(int)n.chordType);
  //Serial.printf("ChordType GT to key = %d\n",(int)n.key);
  //now based on neck assignment, we will need to determine enum value then return the value
  return allChordsGuitar[(uint8_t)n.chordType][(uint8_t)n.key];
}

Chord getKeyboardChordNotesFromNeck(uint8_t row, uint8_t column, uint8_t usePreset) {
  //given preset get neck Assignment:
  //Serial.printf("KB row %d col %d preset %d index %d\n",row, column, usePreset, row * NECK_COLUMNS + column);

  neckAssignment n = neckAssignments[usePreset][row * NECK_COLUMNS + column];
  //Serial.printf("ChordType KB to row = %d preset = %d\n",(int)n.chordType, usePreset);
  //now based on neck assignment, we will need to determine enum value then return the value
  return chords[(uint8_t)n.chordType];
}

bool isStaggeredNotes()
{
  return simpleChordSetting[preset] == ManualStrum;
}

void prepareConfig() {
  presetBPM.clear();
  omniChordModeGuitar.clear();
  enableAllNotesOnChords.clear();
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
    drumsEnabled.push_back(true);
    bassEnabled.push_back(true);
  }
  //Serial.printf("prepareConfig! Exit\n");
}

void prepareChords() {
  //todo erase samples completely as needed
  std::vector<AssignedPattern> assignedFretPatterns;
  assignedFretPatternsByPreset.clear();
  uint8_t rootNotes[] = { 48, 50, 52, 53, 55, 57, 59 };
  // Populate the assignedFretPatterns with the chords for each root note
  for (int x = 0; x < MAX_PRESET; x++) 
  {
    assignedFretPatterns.clear();
    for (int i = 0; i < NECK_ROWS; i++) 
    {
      for (int j = 0; j < NECK_COLUMNS; j++) 
      {
        assignedFretPatterns.push_back(AssignedPattern(simpleChordSetting[x], getKeyboardChordNotesFromNeck(i, j, x), getGuitarChordNotesFromNeck(i, j, x), rootNotes[i], false, 0));
      }
    }
    lastPressedChord = assignedFretPatterns[0].assignedChord;
    lastPressedGuitarChord = allChordsGuitar[0][0];  // c major default strum
    //Serial.printf("assignedFretPatterns size is %d\n", assignedFretPatterns.size());
    assignedFretPatternsByPreset.push_back(assignedFretPatterns);
  }
  //Serial.printf("assignedFretPatternsByPreset size is %d\n", assignedFretPatternsByPreset.size());
}

void printChords() {
  // Major (Maj) - [4, 7]

  majorChord.printChordInfo();  // C Major chord (relative intervals)

  // Minor (Min) - [3, 7]

  minorChord.printChordInfo();  // C Minor chord (relative intervals)

  // Diminished (Dim) - [3, 6]

  diminishedChord.printChordInfo();  // C Diminished chord (relative intervals)

  // Augmented (Aug) - [4, 8]

  augmentedChord.printChordInfo();  // C Augmented chord (relative intervals)

  // Major 7th (Maj7) - [4, 7, 11]

  major7thChord.printChordInfo();  // C Major 7th chord (relative intervals)

  // Minor 7th (Min7) - [3, 7, 10]

  minor7thChord.printChordInfo();  // C Minor 7th chord (relative intervals)

  // Dominant 7th (7) - [4, 7, 10]

  dominant7thChord.printChordInfo();  // C Dominant 7th chord (relative intervals)

  // Minor 7th flat five (m7♭5) - [3, 6, 10]

  minor7Flat5Chord.printChordInfo();  // C Minor 7♭5 chord (relative intervals)

  // Major 6th (Maj6) - [4, 7, 9]

  major6thChord.printChordInfo();  // C Major 6th chord (relative intervals)

  // Minor 6th (Min6) - [3, 7, 9]

  minor6thChord.printChordInfo();  // C Minor 6th chord (relative intervals)

  // Suspended 2nd (Sus2) - [2, 7]

  suspended2Chord.printChordInfo();  // C Sus2 chord (relative intervals)

  // Suspended 4th (Sus4) - [5, 7]

  suspended4Chord.printChordInfo();  // C Sus4 chord (relative intervals)

  // Add 9 - [4, 7, 2]

  add9Chord.printChordInfo();  // C Add9 chord (relative intervals)

  // Add 11 - [4, 7, 11]

  add11Chord.printChordInfo();  // C Add11 chord (relative intervals)

  // 9th (9) - [4, 7, 10, 14]

  ninthChord.printChordInfo();  // C 9th chord (relative intervals)

  // 11th (11) - [4, 7, 10, 14, 17]

  eleventhChord.printChordInfo();  // C 11th chord (relative intervals)

  // 13th (13) - [4, 7, 10, 14, 17, 21]

  thirteenthChord.printChordInfo();  // C 13th chord (relative intervals)
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
void sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity = MAX_VELOCITY) {
  usbMIDI.sendNoteOn(note, velocity, channel);

  //todo add support for playing sound
  if (debug && DEBUGNOTEPRINT) {
    Serial.printf("Note ON: ch=%d note=%d vel=%d\n", channel, note, velocity);
  }
}

void sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity = 0) {
  usbMIDI.sendNoteOff(note, velocity, channel);
  //todo add support for playing sound
  if (debug && DEBUGNOTEPRINT) {
    Serial.printf("Note OFF: ch=%d note=%d vel=%d\n", channel, note, velocity);
  }
}
void noteAllOff(int channel = -1) 
{
  for (uint8_t n = 0; n < MAX_NOTE; n++) 
  {
    switch (channel)
    {
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
      default:
        sendNoteOff(GUITAR_CHANNEL, n);
        sendNoteOff(KEYBOARD_CHANNEL, n);
        sendNoteOff(GUITAR_BUTTON_CHANNEL, n);
        sendNoteOff(DRUM_CHANNEL, n);
        sendNoteOff(BASS_CHANNEL, n);
        //omniChordNewNotes.clear();
        //lastOmniNotes.clear();
        drumState = DrumStopped;
        bassStart  = false;
        DrumSequencerNotes.clear();
        BassSequencerNotes.clear();
        //StaggeredSequencerNotes.clear();
        SequencerNotes.clear();
        buttonPressedChanged = false;
    }
  }
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
}

void setup() {
  //strum = 0;
  pinMode(NOTE_OFF_PIN, INPUT_PULLUP);
  pinMode(START_TRIGGER_PIN, INPUT_PULLUP);
  pinMode(BUTTON_1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_2_PIN, INPUT_PULLUP);
  pinMode(BUTTON_3_PIN, INPUT_PULLUP);
  pinMode(BT_ON_PIN, OUTPUT);     // Set pin 19 as output
  pinMode(BT_STATUS_PIN, INPUT);  // Set pin 18 as output

  digitalWrite(BT_ON_PIN, HIGH);  // Set pin 19 to high

  Serial1.begin(250000);  // Guitar
  Serial2.begin(250000);  // Keyboard
  Serial3.begin(9600);    // BT
  //Serial4.begin(250000); // Cyber G to Piano

  //HM10_BLUETOOTH.begin(HM10_BAUD); //BT
  Serial.begin(115200);  // USB debug monitor

  usbMIDI.begin();

  while (!Serial && millis() < 3000)
    ;  // Wait for Serial Monitor
  if (debug) {
    Serial.println("Teensy MIDI Debug Start");
  }
  Serial3.println("AT\r\n");  // Send AT command

  if (!SD.begin(BUILTIN_SDCARD)) {
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
  if (!loadPatternFiles())
  {
    if (debug) {
      Serial.println("Error! Failed to open pattern file for reading.");
    }
  }
  
  tickTimer.begin(clockISR, computeTickInterval(presetBPM[preset]));  // in microseconds
}
void sendSustain(uint8_t channel, bool isOn)
{
  if (isOn)
  {
    usbMIDI.sendControlChange(64, 127, channel);
  }
  else
  {
    usbMIDI.sendControlChange(64, 0, channel);
  }
  if (debug)
  {
    Serial.printf("Sending Sustain %d on channel %d\n", isOn?1:0, channel);
  }

}




void sendStart() {
  usbMIDI.sendRealTime(0xFA);
  if (debug) {
    Serial.printf("Start!\n");
  }
}

void sendStop() {
  usbMIDI.sendRealTime(0xFC);
  if (debug) {
    Serial.printf("Stop\n");
  }
}

void sendContinue() {
  usbMIDI.sendRealTime(0xFB);
  if (debug) {
    Serial.printf("Continue\n");
  }
  playMode = 1;
}

void sendProgram(uint8_t channel, uint8_t program) {
  if (program == 2)  //right
  {
    curProgram = (curProgram + 1) % 128;
  } else {
    curProgram = curProgram - 1;
    if (curProgram < 0) {
      curProgram = 127;
    }
  }

  usbMIDI.sendProgramChange(curProgram, channel);
  if (debug) {
    Serial.printf("Program: ch=%d program=%d\n", channel, curProgram);
  }
}

void sendCC(uint8_t channel, uint8_t cc, uint8_t value) {

  //usbMIDI.sendControlChange(cc, value, channel);
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

#define MAX_SERIAL_FIELDS 49  // Max number of comma-separated fields, worst case is 16*3+1
#define MAX_SERIAL_FIELD_LENGTH 3
void handleSerialCommand(HardwareSerial& port, const char* label = "") {
  static String inputBuffer[2];  // One buffer per port (0: Serial, 1: Serial1)
  uint8_t index = (&port == &Serial3) ? 1 : 0;

  while (port.available()) {
    char c = port.read();
    inputBuffer[index] += c;

    if (inputBuffer[index].endsWith("\n\r")) {
      inputBuffer[index].trim();

      String fields[MAX_SERIAL_FIELDS];
      uint8_t fieldCount = 0;
      uint8_t start = 0;
      for (uint8_t i = 0; i <= inputBuffer[index].length(); i++) {
        if (inputBuffer[index][i] == ',' || i == inputBuffer[index].length()) {
          if (fieldCount < MAX_SERIAL_FIELDS) {
            fields[fieldCount] = inputBuffer[index].substring(start, i);
            if (fields[fieldCount].length() > MAX_SERIAL_FIELD_LENGTH)
              fields[fieldCount] = fields[fieldCount].substring(0, MAX_SERIAL_FIELD_LENGTH);
            fieldCount++;
          }
          start = i + 1;
        }
      }

      // Print with port label
      for (uint8_t i = 0; i < fieldCount; i++) {
        port.print(label);
        port.print("Field ");
        port.print(i);
        port.print(": ");
        port.println(fields[i]);
      }

      // Send ACK
      port.println("OK00");

      // Reset buffer
      inputBuffer[index] = "";
    }

    if (inputBuffer[index].length() > 512) {
      inputBuffer[index] = "";
    }
  }
}
unsigned long firstPressTime = 0;
unsigned long lastPressTime = 0;
uint8_t pressCount = 0;

bool areChordsEqual(std::vector<uint8_t> chordNotesA, std::vector<uint8_t> chordNotesB)
{
  bool curFound = false;
  for (uint8_t i = 0; i < chordNotesA.size(); i++)
  {
    curFound = false;
    for (uint8_t j = 0; j < chordNotesB.size(); j++)
    {
      //check if all notes in A have a match
      if (chordNotesA[i] == chordNotesB[j])
      {
        curFound = true;
        break;
      }
    }
    if (curFound == false)
    {
      return false;
    }
  }
  for (uint8_t i = 0; i < chordNotesB.size(); i++)
  {
    curFound = false;
    for (uint8_t j = 0; j < chordNotesA.size(); j++)
    {
      //check if all notes in A have a match
      if (chordNotesB[i] == chordNotesA[j])
      {
        curFound = true;
        break;
      }
    }
    if (curFound == false)
    {
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

void updateNotes(std::vector<uint8_t> &chordNotesA, std::vector<uint8_t> &chordNotesB, std::vector<SequencerNote> & SN)
{
  //uint8_t modded = 0;
  //uint8_t ignored = 0;
  std::vector<bool> noteModified(SN.size(), false);  // Track modifications
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
      if (SN[j].note == chordNotesA[i] && (SN[j].offset > 0 || (SN[j].offset == 0 && SN[j].holdTime == INDEFINITE_HOLD_TIME))) // we only care for unplayed notes
      {
        //if there is an equivalent on the new one, swap it out
        if (chordNotesB.size() > i)
        {
          //modded++;
          //Serial.printf("Note swapped!\n");
          noteModified[j] = true;
          SN[j].note = chordNotesB[i];
        }
        else //revert to placeholder
        {
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
        }
      }
    }
  }
  //Serial.printf("Updated notes is %d, ignored %d\n", modded, ignored);
}
void checkSerialGuitar(HardwareSerial& serialPort, char* buffer, uint8_t& bufferLen, uint8_t channel) {
  size_t last_len = 0;
  uint8_t rootNote = 0;
  // --- Step 1: Read bytes into buffer ---
  while (serialPort.available() && bufferLen < MAX_BUFFER_SIZE - 1) {
    char c = serialPort.read();
    sprintf(&buffer[bufferLen], "%02x", (unsigned char)c);
    bufferLen += 2;
  }
  buffer[bufferLen] = '\0';

  while (bufferLen >= 4) {
    if (strncmp(buffer, "aa55", 4) == 0) {
      break;
    }
    memmove(buffer, buffer + 2, bufferLen - 1);
    bufferLen -= 2;
    buffer[bufferLen] = '\0';
  }
  bool ignore;
  // --- Step 5: Main parser loop ---
  while (true) {
    ignore = false;
    uint8_t myTranspose = guitarTranspose[preset];
    bool processed = false;
    bool matched = false;
    if (bufferLen > 12) {
      for (const HexToProgram& msg : hexToProgram) {
        size_t len = strlen(msg.hex);
        if (bufferLen >= len && strncmp(buffer, msg.hex, len) == 0) {
          sendCC(channel, msg.program, MAX_VELOCITY);
          memmove(buffer, buffer + len, bufferLen - len + 1);
          bufferLen -= len;
          matched = true;
          processed = true;
          break;
        }
      }

      if (matched) continue;
    }
    if (bufferLen >= 22 and !matched) {
      for (const MidiMessage& msg : hexToNote) {
        size_t len = strlen(msg.hex);
        last_len = len;

        if (bufferLen >= len && strncmp(buffer, msg.hex, len) == 0) 
        {
          isSustainPressed = false;
          //handling for last 2 rows of guitar neck
          if (msg.note >= MIN_IGNORED_GUITAR && msg.note <= MAX_IGNORED_GUITAR) 
          {
            if (msg.note == BPM_COMPUTE_BUTTON) {
              BPMTap();
            }
            else if (msg.note == SUSTAIN_BUTTON)
            {
              if (useToggleSustain[preset])
              {
                isSustain = !isSustain;
              }
              else
              {
                isSustainPressed = true;
              }
              sendSustain(KEYBOARD_CHANNEL, isSustain);
            }
            else if (msg.note == DRUM_STARTSTOP_BUTTON)
            {
              if (drumsEnabled[preset])
              {
                if (drumState == DrumStopped)
                {
                  if (debug) {
                  Serial.printf("Starting drum sequencer\n");
                  }
                  if (drumsEnabled[preset])
                  {
                    drumState = DrumIntro;
                    drumNextState = DrumNone;
                    prepareDrumSequencer();
                  }
                  if (bassEnabled[preset])
                  {
                    bassStart = true;
                  }
                }
                else
                {
                  Serial.printf("Stopping drum sequencer\n");
                  if (drumsEnabled[preset])
                  {
                    drumNextState = DrumEnding;
                    prepareDrumSequencer();
                    bassStart = false;
                  }
                  else if (bassEnabled[preset])
                  {
                    bassStart = false;
                  }
                }
              }
              else
              {
                if (!bassStart)
                {
                  bassStart = true;
                }
                else
                {
                  bassStart = false;
                }
              }
            }
            else if (msg.note == DRUM_FILL_BUTTON)
            {
              if (drumState == DrumStopped || drumState == DrumNone || drumNextState == DrumEnding || drumState == DrumEnding)
              {
                //ignore
              }
              else
              {
                Serial.printf("Adding drum fill\n");
                drumNextState = DrumLoopFill;
              }
            }
          } 
          else //handling for first 7 rows
          {
            isSustainPressed = false;
            lastNeckButtonPressed = neckButtonPressed;
            if (lastNeckButtonPressed != -1) 
            {
              if (lastValidNeckButtonPressed != neckButtonPressed)
              {
                buttonPressedChanged = true;
              }
              lastValidNeckButtonPressed2 = lastValidNeckButtonPressed;
              lastValidNeckButtonPressed = neckButtonPressed;
              //Serial.printf("Guitar: last1=%d, last2= %d changed %d\n", lastValidNeckButtonPressed, lastValidNeckButtonPressed2, buttonPressedChanged?1:0);
            }
            else if (isSustain && !useToggleSustain[preset])
            {
              isSustain = false;
              sendSustain(KEYBOARD_CHANNEL, false);
            }
            if (msg.note != 255)  //button press
            {
              neckButtonPressed = msg.note;

              if (!isKeyboard) {
                myTranspose = 0;
              } else {
                myTranspose = guitarTranspose[preset];
              }
              //if (debug) {
//                Serial.printf("Get strum pattern %d button = %d\n", assignedFretPatternsByPreset[preset][msg.note].getPatternAssigned(), msg.note);
              //}
              //guitar plays chord on button press (always if keyboard)
              //omnichord mode for guitar and piano
              if (isKeyboard || omniChordModeGuitar[preset] > OmniChordOffType) 
              {
                ignore = true;
                //if omnichord mode and is using paddle or keyboard is in a legit omnichord mode, prepare the notes
                if (omniChordModeGuitar[preset] > OmniChordOffType && (!isKeyboard || (isKeyboard && omniChordModeGuitar[preset] != OmniChordOffType && omniChordModeGuitar[preset] != OmniChordGuitarType))) 
                {
                  //check if button pressed is the same
                  if (lastValidNeckButtonPressed != neckButtonPressed || omniChordNewNotes.size() == 0) {
                    //need to create a note list
                    omniChordNewNotes.clear();
                    int offset = -1 * SEMITONESPEROCTAVE;  //move down by 1 octave since original uses middle C

                    std::vector<uint8_t> chordNotes = assignedFretPatternsByPreset[preset][msg.note].getChords().getCompleteChordNotes();  //get notes

                    //Serial.printf("chordNotes = ");
                    //for (uint8_t i = 0; i < chordNotes.size(); i++)
                    //{
                    //Serial.printf("%d ", chordNotes[i]);
                    //}
                    //Serial.printf("\n");
                    //create list of notes that repeat and shift every time all notes have been added for omnichord mode
                    while (omniChordNewNotes.size() < omniChordOrigNotes.size()) {
                      if (omniChordNewNotes.size() != 0) {
                        offset += SEMITONESPEROCTAVE;
                      }
                      for (uint8_t i = 0; i < chordNotes.size() && omniChordNewNotes.size() < omniChordOrigNotes.size(); i++) 
                      {
                        omniChordNewNotes.push_back(chordNotes[i] + offset + SEMITONESPEROCTAVE * omniKBTransposeOffset[preset]);
                      }
                    }
                    //Serial.printf("OmnichordnewNotes = ");
                    //for (uint8_t i = 0; i < omniChordNewNotes.size(); i++)
                    //{
                    //  Serial.printf("%d ", omniChordNewNotes[i]);
                    //}
                    //Serial.printf("\n");
                  }
                }
                //plays guitar chords when neck button is pressed for non omnichord guitar mode
                //if (assignedFretPatternsByPreset[preset][msg.note].getPatternStyle() == SimpleStrum && omniChordModeGuitar[preset] != OmniChordGuitarType) 
                if (simpleChordSetting[preset] == SimpleStrum && omniChordModeGuitar[preset] != OmniChordGuitarType) 
                {
                  lastSimple = true;
                  if (lastGuitarNotes.size() > 0) {
                    for (uint8_t i = 0; i < lastGuitarNotes[i]; i++) {
                      sendNoteOff(channel, lastGuitarNotes[i]);
                    }
                    lastGuitarNotes.clear();
                  }
                  rootNote = assignedFretPatternsByPreset[preset][msg.note].getChords().getRootNote();
                  if (debug) {
                    Serial.printf("Root note is %d\n", rootNote);
                  }
                  //plays root to last notes
                  sendNoteOn(channel, rootNote + myTranspose);
                  lastGuitarNotes.push_back(rootNote + myTranspose);
                  for (uint8_t note : assignedFretPatternsByPreset[preset][msg.note].getChords().getChordNotes()) {
                    sendNoteOn(channel, rootNote + note + myTranspose);
                    lastGuitarNotes.push_back(rootNote + note + myTranspose);
                  }
                }
                //plays non simple patterns, manual press or automatic, tentatively piano chord?
                else if (omniChordModeGuitar[preset] != OmniChordGuitarType) // other strum type, omnichord mode = off
                {
                  lastSimple = false;
                  //auto or manual type
                  //if (debug) {
                    //Serial.printf("Not simple! Type is %d\n", (int) simpleChordSetting[preset]);
                  //}
                  bool isReverse = false;
                  if (alternateDirection[preset])
                  {
                    isStrumUp = !isStrumUp;
                    isReverse = isStrumUp;
                  }
                  if (lastValidNeckButtonPressed != neckButtonPressed && lastValidNeckButtonPressed != -1)
                  {
                    for (uint8_t i = 0; i < SequencerNotes.size(); i++)
                    {
                      if (SequencerNotes[i].channel == GUITAR_CHANNEL)
                      {
                        sendNoteOff(channel, SequencerNotes[i].note);
                      }
                    }
                    SequencerNotes.clear();
                  }
                  if (lastValidNeckButtonPressed2 != lastValidNeckButtonPressed && lastValidNeckButtonPressed2 != -1 && lastValidNeckButtonPressed != -1)// && StaggeredSequencerNotes.size() > 0)
                  {
                    //Serial.printf("You need to change the notes! %d vs %d\n", lastValidNeckButtonPressed, neckButtonPressed); 
                    std::vector<uint8_t> chordNotesA;
                    std::vector<uint8_t> chordNotesB;
                    if (enableAllNotesOnChords[preset])
                    {
                      chordNotesA = assignedFretPatternsByPreset[preset][lastValidNeckButtonPressed2].getChords().getCompleteChordNotes();  //get notes
                      chordNotesB = assignedFretPatternsByPreset[preset][lastValidNeckButtonPressed].getChords().getCompleteChordNotes();  //get notes
                    }
                    else
                    {
                      chordNotesA = assignedFretPatternsByPreset[preset][lastValidNeckButtonPressed2].getChords().getCompleteChordNotesNo5();  //get notes
                      chordNotesB = assignedFretPatternsByPreset[preset][lastValidNeckButtonPressed].getChords().getCompleteChordNotesNo5();  //get notes
                    }
                    if (StaggeredSequencerNotes.size() > 0)
                    {
                      updateNotes(chordNotesA, chordNotesB, StaggeredSequencerNotes);
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
                  buildAutoManualNotes(isReverse, msg.note); //always in order for 
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
                if (enableButtonMidi[preset])
                {
                  sendNoteOn(GUITAR_BUTTON_CHANNEL, msg.note + MIDI_BUTTON_CHANNEL_NOTE_OFFSET, 1);  //play the specific note on another channel at minimum non 0 volume
                  if (lastValidNeckButtonPressed != lastNeckButtonPressed && lastValidNeckButtonPressed >= 0)
                  {
                    sendNoteOff(GUITAR_BUTTON_CHANNEL, lastValidNeckButtonPressed+ MIDI_BUTTON_CHANNEL_NOTE_OFFSET);  //play the specific note on another channel at minimum non 0 volume
                  }
                  lastGuitarNotesButtons.push_back(msg.note + MIDI_BUTTON_CHANNEL_NOTE_OFFSET);
                }
                
              }
              //else //using paddle adapter - omnichord mode = guitar
              if (!ignore) 
              {
                //mutes previous if another button is pressed
                //Serial.printf("MuteWhenLetGo %d, ignoreSameChord[preset] %d, LastValidpressed = %d neckPressed = %d\n", muteWhenLetGo[preset]?1:0, ignoreSameChord[preset]?1:0, lastValidNeckButtonPressed, neckButtonPressed);
                if (getPreviousButtonPressed() >= 0 && getCurrentButtonPressed() >= 0 && !muteWhenLetGo[preset]) {
                  //std::vector<uint8_t> chordNotes = assignedFretPatternsByPreset[preset][msg.note].getChords().getCompleteChordNotes(); //get notes
                  //check if same notes are to be played
                  bool ignorePress = false;
                  if (ignoreSameChord[preset]) 
                  {
                    //Serial.printf("lastGuitarNotes size %d SequencerNotes size %d\n", lastGuitarNotes.size(), SequencerNotes.size());
                    bool isEqual = true;
                    std::vector<uint8_t> chordNotesA = assignedFretPatternsByPreset[preset][getPreviousButtonPressed()].getChords().getCompleteChordNotes();  //get notes
                    std::vector<uint8_t> chordNotesB = assignedFretPatternsByPreset[preset][getCurrentButtonPressed()].getChords().getCompleteChordNotes();           //get notes
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
                    if (!isEqual)
                    {
                     if (debug) 
                      {
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
                  if (!ignorePress)
                  {
                    while (lastGuitarNotes.size() > 0 && muteSeparation[preset] == 0) {
                      sendNoteOff(channel, lastGuitarNotes.back());
                      lastGuitarNotes.pop_back();
                    }
                    if (enableButtonMidi[preset])
                    {
                      while (lastGuitarNotesButtons.size() > 0) {
                        sendNoteOff(channel, lastGuitarNotesButtons.back());
                        lastGuitarNotesButtons.pop_back();
                      }
                    }
                    //handle string let go case and turn off all notes
                    if (muteSeparation[preset] == 0)
                    {
                      for (uint8_t i = 0; i < SequencerNotes.size(); i++) {
                        if (SequencerNotes[i].channel == GUITAR_CHANNEL) {
                          SequencerNotes[i].offset = 0;
                          SequencerNotes[i].holdTime = 0;
                        }
                      }
                    }
                    else
                    {
                      gradualCancelNotes(lastGuitarNotes,GUITAR_CHANNEL);
                      lastGuitarNotes.clear();
                    }
                  }
                }
                //do nothing as we only care about neck tracking
              }
            } 
            else  //no button pressed on neck
            {
              lastNeckButtonPressed = neckButtonPressed;
              neckButtonPressed = -1;
              if (muteWhenLetGo[preset]) 
              {
                //for paddle we actually stop playing when user lets go of "strings"
                //todo add check if it really is for paddle only
                while (lastGuitarNotes.size() > 0) 
                {
                  sendNoteOff(channel, lastGuitarNotes.back());
                  lastGuitarNotes.pop_back();
                }
                if (enableButtonMidi[preset])
                {
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
          memmove(buffer, buffer + len, bufferLen - len + 1);
          bufferLen -= len;
          matched = true;
          processed = true;  //did something
          break;
        }
      }
      if (matched) continue;
      else {
        memmove(buffer, buffer + last_len, bufferLen - last_len + 1);
        bufferLen -= last_len;
      }
      break;
    }

    if (!processed) break;
  }

  if (bufferLen > MAX_BUFFER_SIZE - 10) {
    memmove(buffer, buffer + bufferLen - 60, 60);
    bufferLen = 60;
    buffer[bufferLen] = '\0';
  }
}

void checkSerialBT(HardwareSerial& serialPort, char* buffer, uint8_t& bufferLen) {

  bool buttonBTState = digitalRead(BT_STATUS_PIN);
  if (prevButtonBTState != buttonBTState) {
    if (debug) {
      Serial.printf("BT is %d\n", buttonBTState ? 1 : 0);
    }
    if (buttonBTState) {
      digitalWrite(BT_ON_PIN, LOW);  // Set pin 19 to high
    } else {
      digitalWrite(BT_ON_PIN, HIGH);  // Set pin 19 to high
    }
    prevButtonBTState = buttonBTState;
  }
  while (serialPort.available()) {
    char c = serialPort.read();
    Serial.print(c);
  }
}

template<typename SerialType>
bool decodeCmd(SerialType& serialPort, String cmd, std::vector<String>* params) 
{
  char buffer[64];
  bool bTemp = false;
  if (debug) {
    Serial.printf("Serial command received is %s\n", cmd.c_str());
  }
  if (cmd == "SAVE") {
    if (saveSettings()) {
      serialPort.write("OK00\r\n");
    } else {
      serialPort.write("ER00\r\n");
    }
    // Match found
  } 
  else if (cmd == "SAVP") {
    if (savePatternFiles()) {
      serialPort.write("OK00\r\n");
    } else {
      serialPort.write("ER00\r\n");
    }
    // Match found
  } 
  else if (cmd == "RSTP")  // RESET Please
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
    if (!loadPatternFiles())
    {
      if (debug) {
        Serial.println("Error! Failed to open pattern file for reading.");
      }
    }
    
    setNewBPM(presetBPM[preset]);
    serialPort.write("OK00\r\n");
  } 
  else if (cmd == "RCFG")  //read conffig
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
  } 
  else if (cmd == "RINI")  //read config but through serial debug
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

  } 
  else if (cmd == "LOAD")  //reload config files
  {
    if (loadSettings()) {
      serialPort.write("OK00\r\n");
    } else {
      serialPort.write("ER00\r\n");
    }
  } 
  if (cmd == "LODP") {
    if (loadPatternFiles()) {
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
  } 
  else if (cmd == "DCLR")  //Drum data clear
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
  } 
  else if (cmd == "BCLR")  //Bass data clear
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
    getBassPattern(atoi(params->at(0).c_str()) )->clear();
    serialPort.write("OK00\r\n");
  } 

  else if (cmd == "DADD")  //Drum data add
  {
    if (params->size() != 5)  //missing parameter
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
      serialPort.write("ER01\r\n");  //invalid parameter
      return true;
    }
    addEntryToPattern(getDrumPattern(atoi(params->at(0).c_str())), atoi(params->at(1).c_str()), atoi(params->at(2).c_str()), atoi(params->at(3).c_str()), atoi(params->at(4).c_str()), DRUM_CHANNEL);
  } 
  else if (cmd == "GADD")  //Guitar data add
  {
    if (params->size() != 5)  //missing parameter
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
    addEntryToPattern(getGuitarPattern(atoi(params->at(0).c_str())), atoi(params->at(1).c_str()), atoi(params->at(2).c_str()), atoi(params->at(3).c_str()), atoi(params->at(4).c_str()), GUITAR_CHANNEL);
  } 
  else if (cmd == "BADD")  //Bass data add
  {
    if (params->size() != 5)  //missing parameter
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
    addEntryToPattern(getBassPattern(atoi(params->at(0).c_str())), atoi(params->at(1).c_str()), atoi(params->at(2).c_str()), atoi(params->at(3).c_str()), atoi(params->at(4).c_str()), BASS_CHANNEL);
  } 
  else if (cmd == "GPRN")  //Guitar data Print
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
    serialPort.write("OK00\r\n");
  } 
  else if (cmd == "BPRN")  //Bass data Print
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
    serialPort.write("OK00\r\n");
  } 
  else if (cmd == "DPRN")  //Drum data Print
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
    serialPort.write("OK00\r\n");
  } 
  //pattern handling end
  else if (cmd == "PRSW")  //Set Current Preset
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
  } 
  else if (cmd == "PRSR")  //Read Current Preset
  {
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", preset);
    serialPort.write(buffer);
  } 
  else if (cmd == "DBGW")  //Set Debug setting
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
  } 
  else if (cmd == "DBGR")  //Debug setting read
  {
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", debug ? 1 : 0);
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
  } 
  else if (cmd == "SPCR")  //stopSoundsOnPresetChange read
  {
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", stopSoundsOnPresetChange ? 1 : 0);
    serialPort.write(buffer);
  }

  else if (cmd == "CLKW")  //midiClockEnable
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
  } 
  else if (cmd == "CLKR")  //midiClockEnable
  {
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", midiClockEnable ? 1 : 0);
    serialPort.write(buffer);
  } 
  else if (cmd == "BPMW")  //presetBPM write
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
  } 
  else if (cmd == "BPMR")  //presetBPM Read
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
    omniChordModeGuitar[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str());
    serialPort.write("OK00\r\n");
  } 
  else if (cmd == "OMMR")  //omniChordModeGuitar Read
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
  } 
  else if (cmd == "MWLW")  //muteWhenLetGo write
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
  } 
  else if (cmd == "ISCW")  //ignoreSameChord write
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
  } 
  else if (cmd == "ISCR")  //ignoreSameChord Read
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
  } 
  else if (cmd == "CH_W")  //chordHold write
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
  } 
  else if (cmd == "CH_R")  //chordHold Read
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
  } 
  else if (cmd == "UTSR")  //useToggleSustain Read
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
  } 
  else if (cmd == "STSR")  //strumSeparation Read
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
  } 
  else if (cmd == "SCSW")  //simpleChordSetting write
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
  } 
  else if (cmd == "SCSR")  //simpleChordSetting Read
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
/*

  else if (cmd == "QNTW")  //QUARTERNOTETICKS write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > 999 || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    QUARTERNOTETICKS[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str());
    serialPort.write("OK00\r\n");
    setNewBPM(presetBPM[preset]);
  } 
  else if (cmd == "QNTR")  //QUARTERNOTETICKS Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", QUARTERNOTETICKS[atoi(params->at(0).c_str())]);
    serialPort.write(buffer);
  }
*/

  
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
    serialPort.write("OK00\r\n");
  } 
  else if (cmd == "OTOR")  //omniKBTransposeOffset Read
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
  }
  else if (cmd == "GTRW")  //guitarTranspose write
  {
    if (params->size() < 2) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > GUITAR_TRANSPOSE_MAX || atoi(params->at(1).c_str()) < GUITAR_TRANSPOSE_MIN ) {
      serialPort.write("ER00\r\n");
      return true;
    }
    noteAllOff(GUITAR_CHANNEL);
    guitarTranspose[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str());
    serialPort.write("OK00\r\n");
  } 
  else if (cmd == "GTRR")  //guitarTranspose Read
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
  } 
  else if (cmd == "GMSR")  //muteSeparation Read
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
  }
  else if (cmd == "EANW")  //enableAllNotesOnChords write
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
  } 
  else if (cmd == "EANR")  //enableButtonMidi Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", enableAllNotesOnChords[atoi(params->at(0).c_str())]?1:0);
    serialPort.write(buffer);
  }
  else if (cmd == "EBMW")  //enableButtonMidi write
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
  } 
  else if (cmd == "EBMR")  //enableButtonMidi Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", enableButtonMidi[atoi(params->at(0).c_str())]?1:0);
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
    if (!bassEnabled[preset])
    {
      bassStart = false;
    }
    serialPort.write("OK00\r\n");
  } 
  else if (cmd == "BENR")  //BassEnabled Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", bassEnabled[atoi(params->at(0).c_str())]?1:0);
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
    if (!drumsEnabled[preset])
    {
      drumNextState = DrumEnding;
      drumState = DrumEnding;
      prepareDrumSequencer();
    }
    serialPort.write("OK00\r\n");
  } 
  else if (cmd == "DENR")  //drumsEnabled Read
  {
    if (params->size() < 1) {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", drumsEnabled[atoi(params->at(0).c_str())]?1:0);
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
    if (atoi(params->at(4).c_str()) > (int)thirteenthChordType || atoi(params->at(4).c_str()) < (int)majorChordType) {
      serialPort.write("ER05\r\n");
      return true;
    }
    neckAssignments[atoi(params->at(0).c_str())][atoi(params->at(1).c_str()) * NECK_COLUMNS + atoi(params->at(2).c_str())].key = (Note)atoi(params->at(3).c_str());
    neckAssignments[atoi(params->at(0).c_str())][atoi(params->at(1).c_str()) * NECK_COLUMNS + atoi(params->at(2).c_str())].chordType = (ChordType)atoi(params->at(4).c_str());
    serialPort.write("OK00\r\n");
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
  } 
  else if (cmd == "NAPR")  //NeckAssignmentcustom.Pattern Read
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
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0) 
    {
      serialPort.write("ER01\r\n");
      return true;
    }
    //patterns
    if (atoi(params->at(1).c_str()) >= MAX_CUSTOM_PATTERNS || atoi(params->at(1).c_str()) < 0) {
      serialPort.write("ER02\r\n");
      return true;
    }
    for (int i = 0; i < NECK_ROWS; i++) 
    {
      for (int j = 0; j < NECK_COLUMNS; j++) 
      {
        Serial.printf("At %d: %d, %d = %d\n", atoi(params->at(0).c_str()), i, j, i * NECK_COLUMNS + j);
        assignedFretPatternsByPreset[atoi(params->at(0).c_str())][i * NECK_COLUMNS + j].customPattern = atoi(params->at(1).c_str());
      }
    }
    serialPort.write("OK00\r\n");
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
        serialPort.printf("Received CMD: %s", cmd);

        //int paramIndex = 1;
        char* token;
        while ((token = strtok(NULL, ",")) != NULL) {
          //serialPort.printf("Param %d: %s\n", paramIndex++, token);
          serialPort.printf(",%s", token);
          params.push_back(String(token));
        }
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
  for (uint8_t chordNote : chordNotes) {
    for (auto& seqNote : SequencerNotes) {
      
      if (seqNote.note == chordNote && seqNote.channel == channel) 
      {
        // Match found — cancel the note
        seqNote.offset = 0; //note will not be played, hold time is set to low value to delay/force note off
        //assumes simple chord hold at this point?
        if (muteSeparation[preset] == 0)
        {
          seqNote.holdTime = 0;
        }
        else
        {
          seqNote.holdTime = stopDelay; //order is always the same
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
        seqNote.offset = 0; //note will not be played, hold time is set to low value to delay/force note off
        //assumes simple chord hold at this point?
        if (muteSeparation[preset] == 0)
        {
          seqNote.holdTime = 0;
        }
        else
        {
          if (seqNote.holdTime != INDEFINITE_HOLD_TIME)
          {
            seqNote.holdTime = stopDelay; //order is always the same
            stopDelay += muteSeparation[preset];
          }
        }
      }
    }
  }
}

std::vector<SequencerNote> getCurrentPattern(uint8_t button)
{
  if (button < 0 || button >= NECK_COLUMNS * NECK_ROWS)
  {
    Serial.printf("Error! Invalid button value %d\n", button);
    button = 0; 
  }
  switch (assignedFretPatternsByPreset[preset][button].customPattern)
  {
    case 0:
      return SequencerPatternA;
    case 1:
      return SequencerPatternB;
    default:
      return SequencerPatternC; 
  }
}
    
//replace note place holders with actual notes from chord
std::vector<SequencerNote> getPatternNotesFromChord(bool isGuitar, int buttonPressed)
{
  //int buttonPressed = lastValidNeckButtonPressed;
  // /Serial.printf("getPatternNotesFromChord is %d\n", buttonPressed);
  std::vector<SequencerNote> result;
  if (!isGuitar)
  {
    result = BassSequencerPattern;
  }
  else
  {
    result = getCurrentPattern(buttonPressed);
  }
    
  std::vector<uint8_t> chordNotesA = assignedFretPatternsByPreset[preset][buttonPressed].getChords().getCompleteChordNotesNo5();  //get notes
  for(uint8_t i = 0; i <chordNotesA.size(); i++)
  {
    for (uint8_t j = 0; j < result.size(); j++)
    {
      if (result[j].note == i)
      {
        result[j].note = chordNotesA[i];
      }
    }
  }
  return result;
}

//for manual strum, it will prepare the staggered notes and update the strum sequence
//for auto strum, it will assign the notes and add to the strum sequence
void buildAutoManualNotes(bool reverse, int buttonPressed)
{
  bool isManual = false;
  if (buttonPressed < 0 || buttonPressed >= NECK_COLUMNS * NECK_ROWS)
  {
    Serial.printf("Error! Invalid button value %d\n", buttonPressed);
    return; // error!
  }
  //if staggered notes is empty
  //if manual
  uint16_t currentOrder = 0;
  if (simpleChordSetting[preset] == ManualStrum)
  {
    isManual = true;
    if (StaggeredSequencerNotes.size() == 0)
    {
      //get assigned 
      //assignedFretPatternsByPreset[preset][buttonPressed].getPatternAssigned();
      StaggeredSequencerNotes = getPatternNotesFromChord(true, buttonPressed);
      auto it = StaggeredSequencerNotes.begin();
      while (it != StaggeredSequencerNotes.end()) {
          if (it->note == REST_NOTE) {
              // Remove the note and update the iterator
              it = StaggeredSequencerNotes.erase(it);
          } else {
              // Move to the next element
              ++it;
          }
      }
      //start with current first offset
      
    }
    currentOrder = StaggeredSequencerNotes[0].offset;

    uint16_t lastOffset = 255;
    
    
    //mute all notes before clearing, todo see if really needed or be simplified
    //turn off currently playing notes
    for (uint8_t i = 0; i < SequencerNotes.size(); i++)
    {
      if (SequencerNotes[i].note > 12)
      {
        sendNoteOff(GUITAR_CHANNEL, SequencerNotes[i].note); 
      }
    }
    //Clear Sequencer notes
    //
    SequencerNotes.clear();
    //get last offset
    for (uint8_t i = 0; i < StaggeredSequencerNotes.size(); i++)
    {
      if (StaggeredSequencerNotes[i].offset != currentOrder)
      {
        lastOffset = i; //know limit
        break;
      }
    }
    
    if (lastOffset == 255)
    {
      lastOffset = StaggeredSequencerNotes.size();
    }
    reverse = false;
    uint8_t separation = 0;
    if (!reverse)
    {
      for (int i = 0; i < lastOffset; i++)
      {
        if (StaggeredSequencerNotes[i].offset != currentOrder)
        {
          lastOffset = i; //know limit
          break;
        }
        if (StaggeredSequencerNotes[i].note > 12)
        {
          StaggeredSequencerNotes[i].offset += separation;
          separation += strumSeparation[preset];
          SequencerNotes.push_back(StaggeredSequencerNotes[i]);
        }
      }
    }
    else
    {
      for (int i = lastOffset - 1; i >= 0; i--)
      {
        if (StaggeredSequencerNotes[i].offset != currentOrder)
        {
          lastOffset = i; //know limit
          break;
        }
        if (StaggeredSequencerNotes[i].note > 12)
        {
          StaggeredSequencerNotes[i].offset += separation;
          separation += strumSeparation[preset];
          SequencerNotes.push_back(StaggeredSequencerNotes[i]);
        }
      }
    }
    //erase last pattern
    StaggeredSequencerNotes.erase(StaggeredSequencerNotes.begin(), StaggeredSequencerNotes.begin() + lastOffset);
  }
  else
  {
    SequencerNotes = getCurrentPattern(buttonPressed);
  }
  
  //basically adjust offset and holdtime for manual strum and get the notes typically just gets 3 notes at 
  //todo check if ths is better than 3 notes only
  //std::vector<uint8_t> chordNotes = assignedFretPatternsByPreset[preset][buttonPressed].assignedChord.getCompleteChordNotesNo5(); 
  std::vector<uint8_t> chordNotes;
  if (enableAllNotesOnChords[preset])
  {
    chordNotes = assignedFretPatternsByPreset[preset][buttonPressed].assignedChord.getCompleteChordNotes(); 
  }
  else
  {
    chordNotes = assignedFretPatternsByPreset[preset][buttonPressed].assignedChord.getCompleteChordNotesNo5(); 
  }
  for (uint8_t i = 0; i < SequencerNotes.size(); i++)
  {
    if (isManual) // adds virtual strumming delay
    {
      SequencerNotes[i].offset = 1 + strumSeparation[preset] * i;
      SequencerNotes[i].holdTime = INDEFINITE_HOLD_TIME; //chord hold like simple mode
    }
    else
    {
      //nothing in theory as it is setup properly
    }
    if (SequencerNotes[i].note < chordNotes.size())
    {
      SequencerNotes[i].note = chordNotes[SequencerNotes[i].note];
    }
    else
    {
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
  if (muteSeparation[preset] > 0)
  {
    for (auto& seqNoteNew : SequencerNoteTemp) 
    {
      for (auto it = SequencerNotes.begin(); it != SequencerNotes.end(); ) 
      {
        // new note is to be played and there is a current note with offset == 0 && holdtime that is greater than offset
        if (seqNoteNew.note == it->note &&  // same note
            it->channel == GUITAR_CHANNEL &&  //same channel
            seqNoteNew.offset < it->holdTime //new note will play before the note is supposed to be off
            ) 
        {
          it->holdTime = seqNoteNew.offset; // force it to turn off before note is played again
        }
        else
        {
          ++it;
        }
      }
    }
  }
  //copy new notes to the sequencer
  for (auto& seqNoteNew : SequencerNoteTemp) 
  {
    SequencerNotes.push_back(seqNoteNew);
  }
}

void cancelAllGuitarNotes()
{
  //Serial.printf("Cancelling all guitar notes!\n");
  for (uint8_t i = 0; i < SequencerNotes.size(); i++)
  {
    if (SequencerNotes[i].channel == GUITAR_CHANNEL)
    {
      SequencerNotes[i].offset = 0;
      SequencerNotes[i].holdTime = 0;
      if (SequencerNotes[i].note > 12)
      {
        sendNoteOff(GUITAR_CHANNEL, SequencerNotes[i].note); 
      }
    }

  }
}
int getCurrentButtonPressed()
{
  if (neckButtonPressed < 0)
  {
    if (chordHold[preset])
    {
      //Serial.printf("Paddle Current lastValid %d\n", lastValidNeckButtonPressed);
      return lastValidNeckButtonPressed;
    }
  }
  //Serial.printf("Paddle Current neckButtonPressed %d\n", neckButtonPressed);
  return neckButtonPressed;
}
int getPreviousButtonPressed()
{
    if (neckButtonPressed < 0)
    {
      //Serial.printf("Paddle Previous lastValid2 %d\n", lastValidNeckButtonPressed2);
      return lastValidNeckButtonPressed2;
    }
  //Serial.printf("Paddle Previous lastValid1 %d\n", lastValidNeckButtonPressed);
  return lastValidNeckButtonPressed;
}

void checkSerialKB(HardwareSerial& serialPort, char* buffer, uint8_t& bufferLen, uint8_t channel) {
  // --- Step 1: Read bytes into buffer ---
  while (serialPort.available() && bufferLen < MAX_BUFFER_SIZE - 1) {
    char c = serialPort.read();
    sprintf(&buffer[bufferLen], "%02x", (unsigned char)c);
    bufferLen += 2;
  }
  buffer[bufferLen] = '\0';
  // if (bufferLen > 0)
  // {
  //   Serial.printf("Raw buffer (KB): %s\n", buffer);
  // }
  if (!ignoringIdlePing) {
    //Serial.printf("Raw buffer (KB): %s\n", buffer);
  }

  // --- Step 2: If we're ignoring idle ping, skip until we see a valid start ---
  if (ignoringIdlePing) {
    while (bufferLen >= 2) {
      if ((buffer[0] == '8' && buffer[1] == '0') || (buffer[0] == '9' && buffer[1] == '0') ||
          //(bufferLen >= 4 && strncmp(buffer, "f555", 4) == 0)) {
          (buffer[0] == 'f' && buffer[1] == '5')) {
        ignoringIdlePing = false;
        break;  // valid data found, resume normal parsing
      }

      // Remove 1 byte (2 hex chars) and keep scanning
      memmove(buffer, buffer + 2, bufferLen - 1);
      bufferLen -= 2;
      buffer[bufferLen] = '\0';
    }

    // Still in idle ignore mode, no need to parse
    if (ignoringIdlePing) return;
  }

  // --- Step 3: Check for idle ping pattern and enter ignore mode ---
  if (bufferLen >= 12 && strncmp(buffer, "f55500282000", 12) == 0) {
    ignoringIdlePing = true;
    //todo if we do built-in sound, send to the device
    isKeyboard = true;
    memmove(buffer, buffer + 12, bufferLen - 11);
    bufferLen -= 12;
    buffer[bufferLen] = '\0';
    return;
  }
  //handling for guitar strum attachment
  if (bufferLen >= 16 && strncmp(buffer, "f55500042017", 12) == 0) {
    isKeyboard = false;
    // Get the value of the last 2 bytes
    uint16_t value = 0;
    sscanf(buffer + 12, "%04hx", &value);

    // we don't care about this in omnichord mode
    //if (omniChordModeGuitar[preset] == 0)
    //{
    //lastStrum = strum;
    //Serial.printf("Preset is %d\n", preset);
    int lastPressed = neckButtonPressed;
    
    if (chordHold[preset]) {
      if (lastPressed == -1 && lastValidNeckButtonPressed != -1) {
        lastPressed = lastValidNeckButtonPressed;
      }
    }
    if (value >= 0x200 && (lastPressed > -1)) 
    {
      //strum = -1;  // up
      if (omniChordModeGuitar[preset] == OmniChordOffType || omniChordModeGuitar[preset] == OmniChordGuitarType) 
      {
        //if (assignedFretPatternsByPreset[preset][msg.note].getPatternStyle() == SimpleStrum)
        if (simpleChordSetting[preset] == SimpleStrum)
        {
          if (getPreviousButtonPressed() >= 0)
          {
            cancelGuitarChordNotes(assignedFretPatternsByPreset[preset][getPreviousButtonPressed()].assignedGuitarChord);
          }
          buildGuitarSequencerNotes(assignedFretPatternsByPreset[preset][getCurrentButtonPressed()].assignedGuitarChord, true);
        }
        //else if (assignedFretPatternsByPreset[preset][msg.note].getPatternStyle() == ManualStrum)
        else if (simpleChordSetting[preset] == ManualStrum)
        {
          //update guitar notes to match chord button
          if (buttonPressedChanged && getPreviousButtonPressed() != -1 && getCurrentButtonPressed() != -1 && getPreviousButtonPressed() != getCurrentButtonPressed())
          {
            //todo check if no 5 is needed            
            std::vector<uint8_t> chordNotesA;
            std::vector<uint8_t> chordNotesB;
            if (enableAllNotesOnChords[preset])
            {
              chordNotesA = assignedFretPatternsByPreset[preset][getPreviousButtonPressed()].getChords().getCompleteChordNotes();  //get notes
              chordNotesB = assignedFretPatternsByPreset[preset][getCurrentButtonPressed()].getChords().getCompleteChordNotes();  //get notes
            }
            else
            {
              chordNotesA = assignedFretPatternsByPreset[preset][getPreviousButtonPressed()].getChords().getCompleteChordNotesNo5();  //get notes
              chordNotesB = assignedFretPatternsByPreset[preset][getCurrentButtonPressed()].getChords().getCompleteChordNotesNo5();  //get notes
            }
            //Serial.printf("I was supposed to change notes sample\n");
            if (SequencerNotes.size() > 0)
            {
              updateNotes(chordNotesA, chordNotesB, StaggeredSequencerNotes);
            }
            buttonPressedChanged = false;
          }
          
          buildAutoManualNotes(true, lastValidNeckButtonPressed); //gets next manual note
          //if there is an existing set of notes, kill them
          //build get next sequence of notes from pattern
          //add to StaggeredSequencerNotes but reversed as needed
        }
        else //autostrum
        {
          cancelAllGuitarNotes();
          buildAutoManualNotes(true, lastPressed);
        }
      } 
      else  //omnichord mode just transposes the notes
      {
        detector.transposeUp();
      }
      //queue buttons to be played
    } else if (value >= 0x100 && lastPressed > -1) 
    {
      //strum = 1;  //down
      if (omniChordModeGuitar[preset] == OmniChordOffType || omniChordModeGuitar[preset] == OmniChordGuitarType) 
      {
        //if (assignedFretPatternsByPreset[preset][msg.note].getPatternStyle() == SimpleStrum)
        if (simpleChordSetting[preset] == SimpleStrum)
        {
          if (getPreviousButtonPressed() >= 0)
          {
            cancelGuitarChordNotes(assignedFretPatternsByPreset[preset][getPreviousButtonPressed()].assignedGuitarChord);
          }
          buildGuitarSequencerNotes(assignedFretPatternsByPreset[preset][getCurrentButtonPressed()].assignedGuitarChord, false);
        }
        //else if (assignedFretPatternsByPreset[preset][msg.note].getPatternStyle() == ManualStrum)
        else if (simpleChordSetting[preset] == ManualStrum)
        {
          //update guitar notes to match chord button
          if (buttonPressedChanged && getPreviousButtonPressed()!= -1 && getCurrentButtonPressed() != -1 && getPreviousButtonPressed() != getCurrentButtonPressed())
          {
            //todo check if no5 is better
            std::vector<uint8_t> chordNotesA;
            std::vector<uint8_t> chordNotesB;
            if(enableAllNotesOnChords[preset])
            {
              chordNotesA = assignedFretPatternsByPreset[preset][getPreviousButtonPressed()].getChords().getCompleteChordNotes();  //get notes
              chordNotesB = assignedFretPatternsByPreset[preset][getCurrentButtonPressed()].getChords().getCompleteChordNotes();  //get notes
            }
            else
            {
              chordNotesA = assignedFretPatternsByPreset[preset][getPreviousButtonPressed()].getChords().getCompleteChordNotesNo5();  //get notes
              chordNotesB = assignedFretPatternsByPreset[preset][getCurrentButtonPressed()].getChords().getCompleteChordNotesNo5();  //get notes
            }
            if (SequencerNotes.size() > 0)
            {
              updateNotes(chordNotesA, chordNotesB, StaggeredSequencerNotes);
            }
            buttonPressedChanged = false;
          }
          
          buildAutoManualNotes(false, lastValidNeckButtonPressed); //gets next manual note
        }
        else //autostrum
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
    //}
    //Serial.printf("strum is %d\n", strum);
    memmove(buffer, buffer + 16, bufferLen - 15);
    bufferLen -= 16;
    buffer[bufferLen] = '\0';
    
    return;
  }

  // --- Step 4: Clean junk at start ---
  while (bufferLen >= 2) {
    if ((buffer[0] == '8' && buffer[1] == '0') || (buffer[0] == '9' && buffer[1] == '0') ||
        //(bufferLen >= 4 && strncmp(buffer, "f555", 4) == 0)) {
        (bufferLen >= 2 && strncmp(buffer, "f5", 2) == 0)) {
      break;
    }
    memmove(buffer, buffer + 2, bufferLen - 1);
    bufferLen -= 2;
    buffer[bufferLen] = '\0';
  }

  // --- Step 5: Main parser loop ---
  bool skipNote;
  uint8_t oldNote = 0;
  while (true) {
    bool processed = false;
    skipNote = false;
    if (bufferLen >= 6 && (strncmp(buffer, "80", 2) == 0 || strncmp(buffer, "90", 2) == 0)) {
      char temp[3];
      temp[2] = '\0';
      temp[0] = buffer[0];
      temp[1] = buffer[1];
      uint8_t status = strtol(temp, NULL, 16);
      temp[0] = buffer[2];
      temp[1] = buffer[3];
      uint8_t note = strtol(temp, NULL, 16);
      temp[0] = buffer[4];
      temp[1] = buffer[5];
      uint8_t vel = strtol(temp, NULL, 16);
      //if paddle and omnichord or keyboard with omnichord (non omnichord guitar mode), keys will play omnichord style

      if (omniChordModeGuitar[preset] != OmniChordOffType && (!isKeyboard || (isKeyboard && omniChordModeGuitar[preset] != OmniChordGuitarType))) {
        //Serial.printf("omniChordNewNotes.size() %d\n", omniChordNewNotes.size());
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
      if (!skipNote) {
        if ((status & 0xF0) == 0x90) {
          sendNoteOn(channel, note + transpose, vel);
          struct noteShift ns;
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
          sendNoteOff(channel, note, vel);
        }
      }
      memmove(buffer, buffer + 6, bufferLen - 5);
      bufferLen -= 6;
      buffer[bufferLen] = '\0';
      processed = true;
      continue;
    } else if (bufferLen >= 4 && strncmp(buffer, "f555", 4) == 0) {
      bool matched = false;
      for (const HexToControl& msg : hexToControl) {
        size_t len = strlen(msg.hex);
        //Serial.printf("Raw buffer (KB): %s vs %s %d\n", buffer, msg.hex, len);
        if (bufferLen >= len && strncmp(buffer, msg.hex, len) == 0) {
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

          memmove(buffer, buffer + len, bufferLen - len + 1);
          bufferLen -= len;
          matched = true;
          break;
        }
      }
      if (matched) continue;
      break;
    } else  //clean up?
    {
      while (bufferLen >= 2) {
        if ((buffer[0] == '8' && buffer[1] == '0') || (buffer[0] == '9' && buffer[1] == '0') ||
            //(bufferLen >= 4 && strncmp(buffer, "f555", 4) == 0)) {
            (bufferLen >= 2 && strncmp(buffer, "f5", 2) == 0)) {
          break;
        }
        memmove(buffer, buffer + 2, bufferLen - 1);
        bufferLen -= 2;
        buffer[bufferLen] = '\0';
      }
    }
    if (!processed) break;
  }

  if (bufferLen > MAX_BUFFER_SIZE - 10) {
    memmove(buffer, buffer + bufferLen - 60, 60);
    bufferLen = 60;
    buffer[bufferLen] = '\0';
  }
}

// void checkSerialG2Piano(HardwareSerial& serialPort, char* buffer, uint8_t& bufferLen, uint8_t channel) {
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

void advanceSequencerStep() {
  for (auto it = SequencerNotes.begin(); it != SequencerNotes.end();) {
    SequencerNote& note = *it;  // Reference to current note

    if (note.offset == 1) {
      //do not play rest and placeholder notes
      if (note.note > 0 && note.note != REST_NOTE && note.note > 12)
      {
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
      if (note.note > 0 && note.note != REST_NOTE && note.note > 12)
      {
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
    
  if (!bassEnabled[preset] && !bassStart && BassSequencerNotes.size() == 0)
  {
    return;
  }
  if (drumsEnabled[preset] && drumState == DrumIntro) //no playing during drum intro
  {
    return;
  }
  
  if (lastValidNeckButtonPressed == -1) //only play if a button has been pressed
  {
    return;
  }
  
  //check if BassSequencerNotes is empty, populate
  //Serial.printf("BassSequencerNotes.size() = %d BassSequencerPattern.size() = %d\n", BassSequencerNotes.size(), BassSequencerPattern.size());
  if (BassSequencerNotes.size() == 0 && BassSequencerPattern.size() > 0)
  {
    if (lastBassNeckButtonPressed == -1)
    {
      lastBassNeckButtonPressed = lastValidNeckButtonPressed;
    }
    BassSequencerNotes = getPatternNotesFromChord(false, lastValidNeckButtonPressed);
    //given the ppattern, we replace the notes based on chord
  }
  else if (lastValidNeckButtonPressed != lastBassNeckButtonPressed)
  {
    //todo check if no5 is better 
    std::vector<uint8_t> chordNotesA;
    std::vector<uint8_t> chordNotesB;
    if(enableAllNotesOnChords[preset])
    {
      chordNotesA = assignedFretPatternsByPreset[preset][lastBassNeckButtonPressed].getChords().getCompleteChordNotes();  //get notes
      chordNotesB = assignedFretPatternsByPreset[preset][lastValidNeckButtonPressed].getChords().getCompleteChordNotes();  //get notes
    }
    else
    {
      chordNotesA = assignedFretPatternsByPreset[preset][lastBassNeckButtonPressed].getChords().getCompleteChordNotesNo5();  //get notes
      chordNotesB = assignedFretPatternsByPreset[preset][lastValidNeckButtonPressed].getChords().getCompleteChordNotesNo5();  //get notes
    }
    updateNotes(chordNotesA, chordNotesB, BassSequencerNotes);
    lastBassNeckButtonPressed = lastValidNeckButtonPressed;
    //we need to switch the notes
    //get the notes of the previous chord
    //get the notes of the new chord
    //swap them out
  }
  else if (BassSequencerNotes.size() == 0)
  {
    BassSequencerNotes = getPatternNotesFromChord(false, lastValidNeckButtonPressed);
  }
  for (auto it = BassSequencerNotes.begin(); it != BassSequencerNotes.end();) 
  {
    SequencerNote& note = *it;  // Reference to current note
    if ((drumsEnabled[preset] && (drumState == DrumStopped || drumState == DrumEnding)) || !bassStart)
    {
      //stop everything and send them to be deleted
      note.offset = 0;
      note.holdTime = 0;  
      bassStart = false;
    }
    else
    {
      if (note.offset == 1) {
        if (note.note > 0  && note.note != REST_NOTE && note.note > 12)
        {
          sendNoteOn(BASS_CHANNEL, note.note - 12, note.velocity);
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
      if (note.note > 0 && note.note != REST_NOTE && note.note > 12) 
      {
        sendNoteOff(BASS_CHANNEL, note.note - 12);
      }
      // Remove the note from the vector
      it = BassSequencerNotes.erase(it);  // `erase` returns the next iterator
    }
    else 
    {
      ++it;  // Only increment if we didn’t erase the note
    }
  }
}

void prepareDrumSequencer()
{
  bool nextIsNotLoop = false;
  if (DrumSequencerNotes.size() == 0)
  {
    switch(drumState)
    {
      case DrumLoop:
        if (drumNextState == DrumNone) // continue loop
        {
          if (DrumLoopHalfBarSequencer.size() > 0) //play half bar end of loop
          {
            DrumSequencerNotes = DrumLoopHalfBarSequencer;
            drumState = DrumLoopHalfBar;
            Serial.printf("DrumLoop->DrumLoopHalfBar\n");
          }
          else
          {
            DrumSequencerNotes = DrumLoopSequencer;
            Serial.printf("DrumLoop->DrumLoop\n");
          }
        }
        else
        {
          if (drumNextState == DrumLoopFill && DrumLoopHalfBarSequencer.size() > 0)
          {
            Serial.printf("DrumLoop->DrumLoopFill\n");
            drumState = drumNextState;
            drumNextState = DrumNone; 
            nextIsNotLoop = true;
          }
          else if (DrumLoopHalfBarSequencer.size() > 0 && drumNextState != DrumEnding) //not a fill and there is a half bar, half should be played first
          {
            Serial.printf("DrumLoop->DrumLoopHalfbar\n");
            //play half file
            drumState = DrumLoopHalfBar;
            nextIsNotLoop = true;
          }
          else // no half bar
          {
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
        if (drumNextState == DrumNone) // continue loop
        {
            Serial.printf("DrumLoopHalf->DrumLoop\n");
            DrumSequencerNotes = DrumLoopSequencer;
            drumState = DrumLoop;
        }
        else if (drumNextState == DrumLoopFill) //fill has to play during half bar part, so it will loop again then gets played at half bar
        {
          Serial.printf("DrumLoopHalf->DrumFill\n");
          drumState = DrumLoop;
          DrumSequencerNotes = DrumLoopSequencer;
        }
        else
        {
          Serial.printf("DrumLoopHalf->Next %d\n", drumNextState);
          //if state change;
          drumState = drumNextState;
          drumNextState = DrumNone;    
          nextIsNotLoop = true;
        }
        break;
      case DrumIntro:
        if (drumNextState == DrumNone) // start loop
        {
          Serial.printf("DrumLoopIntro->DrumLoop\n");
          DrumSequencerNotes = DrumIntroSequencer;
          //drumState = DrumLoop;
          //drumState = DrumIntro;
          drumNextState = DrumLoop;
        }
        else
        {
          Serial.printf("DrumLoopIntro->? %d\n", drumNextState);
          //if state change;
          drumState = drumNextState;
          drumNextState = DrumNone;    
          DrumSequencerNotes = DrumLoopSequencer;
          if (drumState == DrumEnding)
          {
            nextIsNotLoop = true;
          }
          else
          {
            nextIsNotLoop = false;
          }
        }
        break;
      case DrumLoopFill:
        if (drumNextState == DrumNone) // continue loop
        {
          Serial.printf("DrumLoopFill->DrumLoop\n");
            DrumSequencerNotes = DrumLoopSequencer;
            drumState = DrumLoop;
        }
        else
        {
          Serial.printf("DrumLoopFill->?\n");
          //if state change;
          drumState = drumNextState;
          drumNextState = DrumNone;    
          nextIsNotLoop = true;
        }
        break;
      case DrumEnding:
        if (drumNextState == DrumIntro) // continue loop
        {
          Serial.printf("DrumEnd->DrumIntro\n");
          DrumSequencerNotes = DrumIntroSequencer;
          drumState = DrumIntro;
        }
        else
        {
          Serial.printf("DrumEnd->Stop\n");
          drumState = DrumStopped; // should not loop or do anything else
        }
        break;
      default:
        //drumStopped; //do nothing
        break;
    }
    //actually load the sequencer for the interrupt kinds
    if (nextIsNotLoop)
    {
      switch (drumState)
      {
        case DrumIntro:
          DrumSequencerNotes = DrumIntroSequencer;
          break;
        case DrumEnding:
            DrumSequencerNotes = DrumEndSequencer;
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
      if (note.note > 0 && note.note != REST_NOTE)
      {
        sendNoteOn(DRUM_CHANNEL, note.note, note.velocity);
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
      if (note.note > 0 && note.note != REST_NOTE)
      {
        sendNoteOff(DRUM_CHANNEL, note.note);
      }
      // Remove the note from the vector
      it = DrumSequencerNotes.erase(it);  // `erase` returns the next iterator
    } else {
      ++it;  // Only increment if we didn’t erase the note
    }
  }
  //if last note was played in drum sequence, play the next if applicable
  if (DrumSequencerNotes.size() == 0 && drumState != DrumStopped)
  {
    prepareDrumSequencer();
  }
}

void onTick64() {
  tickCount++;

  // Send MIDI Clock every 1/24 note (i.e., every other 1/48 tick)
  if (midiClockEnable && tickCount % 2 == 0) {
    usbMIDI.sendRealTime(usbMIDI.Clock);
  }
  //if note queue is not empty
  if (SequencerNotes.size() > 0)
  {
    advanceSequencerStep();  // or send a note, etc.
  }
  if (DrumSequencerNotes.size() > 0)
  {
    advanceDrumSequencerStep();  // or send a note, etc.
  }
  advanceBassSequencerStep();  // or send a note, etc.

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
    if (!isSerialOut) {
      Serial.printf("Failed to open %s for reading\n", filename);
    }
    return false;
  }

  if (!isSerialOut) {
    Serial.printf("Contents of %s:\n", filename);
  }

  while (f.available()) {
    char c = f.read();  // Read one character
    if (isSerialOut) {
      Serial.write(c);  // Print using printf
    } else {
      Serial.printf("%c", c);  // Print using printf
    }
  }

  f.close();
  if (isSerialOut) {
    Serial.write("\n");  // Print using printf
  } else {
    Serial.println();  // Optional: newline after file contents
  }
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
    snprintf(buffer, sizeof(buffer), "debug,%d\n", debug);
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
      snprintf(buffer, sizeof(buffer), "alternateDirection,%d,%d\n", i, (int) alternateDirection[i]);
      snprintf(buffer, sizeof(buffer), "useToggleSustain,%d,%d\n", i, (int) alternateDirection[i]);

      snprintf(buffer, sizeof(buffer), "omniKBTransposeOffset,%d,%d\n", i, omniKBTransposeOffset[i]);
      snprintf(buffer, sizeof(buffer), "guitarTranspose,%d,%d\n", i, (int) guitarTranspose[i]);
      //snprintf(buffer, sizeof(buffer), "useGradualMute,%d,%d\n", i, (int) useGradualMute[i]);
      snprintf(buffer, sizeof(buffer), "enableButtonMidi,%d,%d\n", i, (int) enableButtonMidi[i]);
      snprintf(buffer, sizeof(buffer), "enableAllNotesOnChords,%d,%d\n", i, (int) enableAllNotesOnChords[i]);
      snprintf(buffer, sizeof(buffer), "drumsEnabled,%d,%d\n", i, (int) drumsEnabled[i]);
      snprintf(buffer, sizeof(buffer), "bassEnabled,%d,%d\n", i, (int) bassEnabled[i]);
      f.print(buffer);
    }
  } else {
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
    Serial.println("Error saving complex config!");
    return false;
  }
  f.close();
  return true;
}
bool savePatternFiles() {
  File f = SD.open("pattern.ini", FILE_WRITE);
  if (f) 
  {
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
    if (DrumIntroSequencer.size() > 0)
    {
      snprintf(buffer, sizeof(buffer), "DRUM_INTRO_START\n");
      f.print(buffer);
      for (auto& seqNote : DrumIntroSequencer)
      {
        snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d\n", seqNote.note, seqNote.holdTime, seqNote.offset, seqNote.velocity);
        f.print(buffer);
      }
      snprintf(buffer, sizeof(buffer), "DRUM_INTRO_END\n");
      f.print(buffer);
    }
    if (DrumLoopHalfBarSequencer.size() > 0)
    {
      snprintf(buffer, sizeof(buffer), "DRUM_HALFLOOP_START\n");
      f.print(buffer);
      for (auto& seqNote : DrumLoopHalfBarSequencer)
      {
        snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d\n", seqNote.note, seqNote.holdTime, seqNote.offset, seqNote.velocity);
        f.print(buffer);
      }
      snprintf(buffer, sizeof(buffer), "DRUM_HALFLOOP_END\n");
      f.print(buffer);
    }
    if (DrumLoopSequencer.size() > 0)
    {
      snprintf(buffer, sizeof(buffer), "DRUM_LOOP_START\n");
      f.print(buffer);
      for (auto& seqNote : DrumLoopSequencer)
      {
        snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d\n", seqNote.note, seqNote.holdTime, seqNote.offset, seqNote.velocity);
        f.print(buffer);
      }
      snprintf(buffer, sizeof(buffer), "DRUM_LOOP_END\n");
      f.print(buffer);
    }

    if (DrumFillSequencer.size() > 0)
    {
      snprintf(buffer, sizeof(buffer), "DRUM_FILL_START\n");
      f.print(buffer);
      for (auto& seqNote : DrumFillSequencer)
      {
        snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d\n", seqNote.note, seqNote.holdTime, seqNote.offset, seqNote.velocity);
        f.print(buffer);
      }
      snprintf(buffer, sizeof(buffer), "DRUM_FILL_END\n");
      f.print(buffer);
    }
    
    if (DrumEndSequencer.size() > 0)
    {
      snprintf(buffer, sizeof(buffer), "DRUM_END_START\n");
      f.print(buffer);
      for (auto& seqNote : DrumEndSequencer)
      {
        snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d\n", seqNote.note, seqNote.holdTime, seqNote.offset, seqNote.velocity);
        f.print(buffer);
      }
      snprintf(buffer, sizeof(buffer), "DRUM_END_END\n");
      f.print(buffer);
    }

    //save guitar patterns
    if (SequencerPatternA.size() > 0)
    {
      snprintf(buffer, sizeof(buffer), "GUITAR_A_START\n");
      f.print(buffer);
      for (auto& seqNote : SequencerPatternA)
      {
        snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d\n", seqNote.note, seqNote.holdTime, seqNote.offset, seqNote.velocity);
        f.print(buffer);
      }
      snprintf(buffer, sizeof(buffer), "GUITAR_A_END\n");
      f.print(buffer);
    }
    if (SequencerPatternB.size() > 0)
    {
      snprintf(buffer, sizeof(buffer), "GUITAR_B_START\n");
      f.print(buffer);
      for (auto& seqNote : SequencerPatternB)
      {
        snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d\n", seqNote.note, seqNote.holdTime, seqNote.offset, seqNote.velocity);
        f.print(buffer);
      }
      snprintf(buffer, sizeof(buffer), "GUITAR_B_END\n");
      f.print(buffer);
    }
    if (SequencerPatternC.size() > 0)
    {
      snprintf(buffer, sizeof(buffer), "GUITAR_C_START\n");
      f.print(buffer);
      for (auto& seqNote : SequencerPatternC)
      {
        snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d\n", seqNote.note, seqNote.holdTime, seqNote.offset, seqNote.velocity);
        f.print(buffer);
      }
      snprintf(buffer, sizeof(buffer), "GUITAR_C_END\n");
      f.print(buffer);
    }
    //load bass pattern
    if (BassSequencerPattern.size() > 0)
    {
      snprintf(buffer, sizeof(buffer), "BASS_START\n");
      f.print(buffer);
      for (auto& seqNote : BassSequencerPattern)
      {
        snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d\n", seqNote.note, seqNote.holdTime, seqNote.offset, seqNote.velocity);
        f.print(buffer);
      }
      snprintf(buffer, sizeof(buffer), "BASS_END\n");
      f.print(buffer);
    }
    f.print(buffer);
  } 
  else 
  {
    Serial.println("Error saving pattern config!");
    return false;
  }
  f.close();
  return true;
}

bool saveSettings() {
  if (!saveSimpleConfig()) {
    Serial.printf("Error loading Simple Config!\n");
    return false;
  }
  if (!saveComplexConfig()) {

    Serial.printf("Error loading Complex Config!\n");
    return false;
  }
  return true;
}

bool loadSimpleConfig() {
  File f = SD.open("config.ini", FILE_READ);
  if (!f) {
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
      if (readPreset > MAX_PRESET) {
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
    } else if (strcmp(key, "debug") == 0) {
      debug = atoi(arg1);
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
  return true;
}

bool loadPatternFiles() {
  File f = SD.open("pattern.ini", FILE_READ);
  if (!f) {
    Serial.println("Error loading pattern config!");
    return false;
  }

  SequencerPatternA.clear();
  SequencerPatternB.clear();
  SequencerPatternC.clear();
  BassSequencerPattern.clear();

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
    } else if (currentPattern) {
      // Parse note line: note,holdTime,offset,velocity
      SequencerNote note;
      int n, h, o, v;
      if (sscanf(trimmed.c_str(), "%d,%d,%d,%d", &n, &h, &o, &v) == 4) {
        note.note = n;
        note.holdTime = h;
        note.offset = o;
        note.velocity = v;
        note.channel = currentChannel;
        currentPattern->push_back(note);
      }
    }
  }

  f.close();
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
      Serial.printf("Error loading Simple Config!\n");
      return false;
    }
    if (!loadComplexConfig()) {
      file.close();
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

void loop() {
  if (sendClockTick) {
    sendClockTick = false;
    onTick64();
  }

  checkSerialGuitar(Serial1, dataBuffer1, bufferLen1, GUITAR_CHANNEL);
  checkSerialKB(Serial2, dataBuffer2, bufferLen2, KEYBOARD_CHANNEL);
  checkSerialBT(Serial3, dataBuffer3, bufferLen3);
  //checkSerialG2Piano(Serial4, dataBuffer4, bufferLen4, 3);
  checkSerialCmd(Serial, dataBuffer4, bufferLen4);
  bool noteOffState = digitalRead(NOTE_OFF_PIN);
  if (noteOffState != prevNoteOffState && noteOffState == LOW) {
    if (debug) {
      Serial.println("NOTE_OFF_PIN pressed → All notes off");
    }
    presetButtonPressed = true;
    noteAllOff();
  }
  else
  {
    presetButtonPressed = false;
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
  }
  prevCCState = ccState;
  /*
 // --- Button Handling ---
  bool button1State = digitalRead(BUTTON_1_PIN);
  if (button1State != prevButton1State && button1State == LOW) {
    Serial.println("Button 1 Pressed");
    preset = 0;

  }
  prevButton1State = button1State;
*/
  prevButton2State = button2State;
  button2State = digitalRead(BUTTON_2_PIN);

  if (presetButtonPressed && button2State == LOW)
  {
    if (debug) {
      Serial.println("BT Pairing should be done here\n");
    }
  }

  if (button2State != prevButton2State && button2State == LOW) {
    
    {
      if (debug) {
        Serial.println("Button 2 Pressed");
      }
      preset += 1;
      if (preset >= MAX_PRESET) {
        preset = 0;
      }
      presetChanged();

      //add preset handling here
      if (debug) {
        Serial.printf("Preset is now %d\n", preset);
      }
    }
  }

  /*
  bool button3State = digitalRead(BUTTON_3_PIN);
  if (button3State != prevButton3State && button3State == LOW) {
    Serial.println("Button 3 Pressed");
    preset = 2;
  }
  prevButton3State = button3State;
*/
  // --- HM10 Bluetooth handling ---
  // while (HM10_BLUETOOTH.available()) {
  //   char c = HM10_BLUETOOTH.read();
  //   Serial.print("[BT] ");
  //   Serial.println(c); // You can customize this later (e.g., trigger MIDI)
  // }
  usbMIDI.read();
  delayMicroseconds(5);
  // printMemoryUsage();
}

void printMemoryUsage() {
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
