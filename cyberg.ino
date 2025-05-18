#include <usb_midi.h>
#include <vector>
#include <SD.h>
#define MAX_BPM 300
#define MIN_BPM 300
#define MAX_PRESET 6
#define NECK_COLUMNS 3
#define NECK_ROWS 7
#define NECK_ACTUALROWS 9
#define MAX_CUSTOM_PATTERNS 3
#define MAX_STRUM_SEPARATION 100
#define MIN_STRUM_SEPARATION 1
#define MIN_IGNORED_GUITAR 21
#define MAX_IGNORED_GUITAR 26
#define BPM_COMPUTE_BUTTON 25

//#define USE_AND
// --- PIN DEFINITIONS ---
#define NOTE_OFF_PIN 10     // Digital input for turning off all notes
#define START_TRIGGER_PIN 11   // Digital input for triggering CC message
#define BT_ON_PIN 19
#define BT_STATUS_PIN 20
#define BUTTON_1_PIN 4
#define BUTTON_2_PIN 5
#define BUTTON_3_PIN 6

// --- MIDI CONSTANTS ---
#define GUITAR_BUTTON_CHANNEL 3
#define GUITAR_CHANNEL 2
#define KEYBOARD_CHANNEL 1

// Other constants
#define ACTUAL_NECKBUTTONS 27
#define NECKBUTTONS 21

extern "C" char* sbrk(int incr); // Get current heap end

struct noteShift {
  uint8_t paddleNote;
  uint8_t assignedNote;
};

class SequencerNote
{
  public:
    uint8_t note;
    uint16_t holdTime;
    uint16_t offset;
    uint8_t channel;
    uint8_t velocity;
  
  SequencerNote(uint8_t newNote, int newholdTime, uint16_t newoffset, uint8_t newchannel, uint8_t newvelocity = 127)
  {
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
    Chord()
    {
      rootNote = 0;
    }
    Chord(const std::vector<uint8_t>& relativeNotes)
        : notes(relativeNotes) {
          rootNote = 0;
        }

    // Add a relative note (interval) to the chord
    void addRelativeNote(uint8_t interval) {
        notes.push_back(interval);
    }

    // Get the relative notes of the chord
    std::vector<uint8_t> getChordNotes() const {
        return notes;
    }
    std::vector<uint8_t> getCompleteChordNotes() const {
        std::vector<uint8_t> complete;
        complete.push_back(rootNote);
        for (uint8_t i = 0; i < notes.size();i++)
        {
          complete.push_back(notes[i] + rootNote);
        }
        return complete;
    }
    uint8_t getRootNote() const {
      return rootNote;
    }
    void setRootNote(uint8_t newNote)
    {
      rootNote = newNote;
    }
    // Print the relative chord's notes
    void printChordInfo() const 
    {
      Serial.print("Chord Notes: ");
      for (uint8_t note : notes) {
          Serial.print(note);
          Serial.print(" ");
      }
      Serial.println();
    }

    std::vector<uint8_t> notes;  // A vector of intervals relative to the root note
};

class PatternNote
{
  public:
    uint8_t degree;
    uint16_t holdTime;
    uint16_t offset;
    
    uint8_t velocity;
    PatternNote()
    {
      degree = 255;
      holdTime = 65535;
      offset = 65535;
    
      velocity = 127;
    }
    PatternNote(uint8_t newDegree, int newholdTime, uint16_t newoffset, uint8_t newvelocity = 127)
    {
      degree = newDegree;
      holdTime = newholdTime;
      offset = newoffset;
      velocity = newvelocity;
    }
    std::vector<SequencerNote> GetSequencerNote(Chord basis, std::vector<PatternNote>& SequencerPattern, uint8_t channel)
    {
      std::vector<SequencerNote> result;
      std::vector<uint8_t> notes;
      notes.push_back(basis.rootNote);
      
      for (uint8_t i = 0; i < basis.notes.size(); i++)
      {
        notes.push_back(basis.notes[i]);
      }
      uint8_t chordSize = notes.size();
      for (uint8_t i = 0; i < SequencerPattern.size(); i++)
      {
        if (SequencerPattern[i].degree <= chordSize) //1 base
        {
          SequencerNote n (notes[SequencerPattern[i].degree-1], SequencerPattern[i].holdTime, SequencerPattern[i].offset, channel, SequencerPattern[i].velocity);
          result.push_back(n);
        }
        else
        {
          //ignore or assume error
        }
      }
      return result;
    }
 
};

std::vector<SequencerNote> SequencerNotes;

//patterns in terms of Sequencer note
std::vector<PatternNote> SequencerPatternA;
std::vector<PatternNote> SequencerPatternB;
std::vector<PatternNote> SequencerPatternC;

class AssignedPattern
{
  
  public:
    AssignedPattern(bool isBasic, Chord toAssign, std::vector<uint8_t> newGuitarChord, uint8_t rootNote, bool ignored, uint8_t pattern)
    {
      isSimple = isBasic;
      assignedChord = toAssign;
      assignedChord.setRootNote(rootNote);
      assignedGuitarChord = newGuitarChord;
      isIgnored = ignored;
      customPattern = pattern;
    }
    int customPattern;
    bool isIgnored;
    bool isSimple;
    Chord assignedChord;
    std::vector<uint8_t> assignedGuitarChord;
    void setChords(Chord newChord)
    {
      assignedChord = newChord;
    }
    void setGuitarChords(std::vector<uint8_t> newGuitarChord)
    {
      assignedGuitarChord = newGuitarChord;
    }
    Chord getChords() const
    {
      return assignedChord;
    }
    Chord getGuitarChords() const
    {
      return assignedGuitarChord;
    }
    bool getSimple() const
    {
      return isSimple;
    }
    bool getIgnored() const
    {
      return isIgnored;
    }
};




//state
int strum;
int lastStrum;
bool ignoringIdlePing = false;
int tickCount = 0;
int transpose = 0;
uint8_t preset = 0;
int curProgram = 0;
uint8_t playMode = 0;
int neckButtonPressed = -1;
int lastNeckButtonPressed = -1;
int lastValidNeckButtonPressed = -1;
bool isKeyboard = false;
bool prevButtonBTState = HIGH;
bool prevButton1State = HIGH;
bool prevButton2State = HIGH;
bool prevButton3State = HIGH;
volatile bool sendClockTick = false;
IntervalTimer tickTimer;
int lastTransposeValueDetected = 0;

//config
bool debug = true;
bool stopSoundsOnPresetChange = true;
bool midiClockEnable = true;
uint16_t deviceBPM = 128;
std::vector<uint8_t> omniChordModeGuitar;
std::vector<bool> muteWhenLetGo;
std::vector<bool> ignoreSameChord;
std::vector<uint16_t> presetBPM;
std::vector<uint16_t> QUARTERNOTETICKS;
std::vector<uint8_t> TimeNumerator;
std::vector<uint8_t> TimeDenominator; // in terms of 1/48 notes
std::vector<uint16_t> MaxBeatsPerBar;
std::vector<bool> chordHold;
std::vector<bool> simpleChordSetting;
std::vector<uint8_t> strumSeparation; //time unit separation between notes //default 1

unsigned int computeTickInterval(int bpm) {
  // 60,000,000 microseconds per minute / (BPM * 48 ticks per quarter note)
  return 60000000UL / (bpm * QUARTERNOTETICKS[preset]);
}



std::vector<uint8_t> omniChordOrigNotes = {65,67,69,71,72,74,76,77,79,81,83,84};
std::vector<uint8_t> omniChordNewNotes;

class TranspositionDetector 
{
    
  uint8_t lowest = omniChordOrigNotes[0] ;
  uint8_t highest = omniChordOrigNotes[omniChordOrigNotes.size()-1];
  int offset = 0;
  int octaveShift = 0;
  
  public:
    void transposeUp()
    {
      octaveShift += 1;
      if (octaveShift > 2)
      {
        octaveShift = 2;
      }
    }
    void transposeDown()
    {
      octaveShift -= 1;
      if (octaveShift < -2)
      {
        octaveShift = -2;
      }
    }
    void transposeReset()
    {
      octaveShift = 0;
    }

    void noteOn(int midiNote) {
        if (midiNote < lowest)
        {
          if (debug)
          {
            Serial.printf("New Low = Lowest %d Old %d\n", midiNote, lowest);          
          }
          //53 is found instead of 65
          offset = omniChordOrigNotes[0] - midiNote; //12
          lowest = midiNote;
          highest = omniChordOrigNotes[omniChordOrigNotes.size()-1] - offset; //84 -12 
        }
        else if (midiNote > highest)
        {
          if (debug)
          {
            Serial.printf("New High = Highest %d Old %d\n", midiNote, highest);          
          }
          //96 is found instead of 84
          offset = midiNote - omniChordOrigNotes[omniChordOrigNotes.size()-1]; //12
          highest = midiNote;
          lowest = omniChordOrigNotes[0] + offset;
        }
    }

    uint8_t getBestNote(uint8_t note)
    {
      //given a transpose
      //uint8_t newNote = note - transpose;
      uint8_t newNote = note - offset;
      uint8_t bestNote = 0;
      bool found = false;
      // assumption is newNote is reverted to its original form
      for (uint8_t i = 0; i < omniChordOrigNotes.size() && !found; i++)
      {
        if (i + 1 >= (int) omniChordOrigNotes.size())
        {
          //best Note is last note
          found = true;
          bestNote = omniChordNewNotes[i];
        }
        else if (omniChordOrigNotes[i] == newNote) 
        {
          bestNote = omniChordNewNotes[i];
          found = true;
        }
        //assume if in between, use current note
        else if (i + 1 < (int) omniChordOrigNotes.size() && omniChordOrigNotes[i] < newNote && omniChordOrigNotes[i + 1] > newNote) 
        {
          bestNote = omniChordNewNotes[i];
          found = true;
        }
        else
        {
          //do nothing
        }
      }
      return bestNote + 12 * octaveShift;
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
  {"aa55000a22010000000000", 255}, //note off
  {"aa55000a22010000000400", 0},  // C1
  {"aa55000a22010000000200", 1},  // C2
  {"aa55000a22010000000100", 2},  // C3
  {"aa55000a22010000002000", 3},  // D1
  {"aa55000a22010000001000", 4},  // D2
  {"aa55000a22010000000800", 5},  // D3
  {"aa55000a22010000010000", 6},  // E1
  {"aa55000a22010000008000", 7},  // E2
  {"aa55000a22010000004000", 8},  // E3
  {"aa55000a22010000080000", 9},  // F1
  {"aa55000a22010000040000", 10},  // F2
  {"aa55000a22010000020000", 11},  // F3
  {"aa55000a22010000400000", 12},  // G1
  {"aa55000a22010000200000", 13},  // G2
  {"aa55000a22010000100000", 14},  // G3
  {"aa55000a22010002000000", 15},  // A1
  {"aa55000a22010001000000", 16},  // A2
  {"aa55000a22010000800000", 17},  // A3
  {"aa55000a22010010000000", 18},  // B1
  {"aa55000a22010008000000", 19},  // B2
  {"aa55000a22010004000000", 20},  // B3
  {"aa55000a22010080000000", 21},  // C4
  {"aa55000a22010040000000", 22},  // C5
  {"aa55000a22010020000000", 23},  // C6
  {"aa55000a22010400000000", 24},  // D4
  {"aa55000a22010200000000", 25},  // D5
  {"aa55000a22010100000000", 26}   // D6
};

struct HexToProgram {
  const char* hex;
  uint8_t program;
};

HexToProgram hexToProgram[] = {
  {"aa550003220200d8", 3}, // Rocker Up
  {"aa5500032202ffd9", 9}, // Rocker Down
};

struct HexToControl {
  const char* hex;
  uint8_t cc;
};


HexToControl hexToControl[] = {
  {"f5550003201401", 2},  // Right
  {"f5550003201402", 1},  // Left
  {"f5550003201403", 5},  // Both
  {"f5550003201000", 4},  // Circle not Lit
  {"f5550003201001", 3},  // Circle Lit
  
  {"f5550003201400", 0} //ignore
};

    Chord majorChord({4, 7});  
    Chord minorChord({3, 7});    
    Chord diminishedChord({3, 6});
    Chord augmentedChord({4, 8});
    Chord major7thChord({4, 7, 11});    
    Chord minor7thChord({3, 7, 10});    
    Chord dominant7thChord({4, 7, 10});    
    Chord minor7Flat5Chord({3, 6, 10});
    Chord major6thChord({4, 7, 9});
    Chord minor6thChord({3, 7, 9});    
    Chord suspended2Chord({2, 7});    
    Chord suspended4Chord({5, 7});
    Chord add9Chord({4, 7, 2});
    Chord add11Chord({4, 7, 11});
    Chord ninthChord({4, 7, 10, 14});    
    Chord eleventhChord({4, 7, 10, 14, 17});    
    Chord thirteenthChord({4, 7, 10, 14, 17, 21});    

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
    {48, 52, 55, 60, 64},   // C Major x
    {49, 56, 61, 65, 68},   // C# Major x
    {50, 57, 62, 66},       // D Major x
    {51, 58, 63, 67, 70},   // D# Major x
    {40, 47, 52, 56, 59, 64}, // E Major x
    {41, 48, 53, 57, 60, 65},   // F Major x
    {42, 49, 54, 58, 61, 66},   // F# Major x
    {43, 47, 50, 55, 59, 67},   // G Major x
    {44, 51, 56, 60, 63, 68},   // G# Major x
    {45, 52, 57, 61, 64},   // A Major x
    {46, 50, 58, 65, 70},       // A# Major x
    {47, 54, 59, 63, 66},   // B Major x
};

std::vector<std::vector<uint8_t>> minorChordGuitar = {
    {48, 51, 60, 63, 67},   // C Minor x
    {49, 54, 61, 64, 68},   // C# Minor x
    {50, 57, 62, 65},   // D Minor x
    {51, 58, 63, 66, 70},       // D# Minor x
    {40, 47, 52, 55, 59, 64},   // E Minor x
    {41, 48, 53, 56, 60},       // F Minor x
    {42, 49, 54, 57, 61},       // F# Minor x lowered
    {43, 50, 55, 58, 62},       // G Minor x lowered
    {44, 51, 56, 59, 63},       // G# Minor x lowered
    {45, 52, 57, 60, 64},       // A Minor x
    {46, 53, 58, 61, 65},       // A# Minor x 
    {47, 54, 59, 62, 66},       // B Minor x
};

std::vector<std::vector<uint8_t>> diminishedChordGuitar = {
    {60, 63, 66},   // C Diminished x
    {61, 64, 67},     
    {62, 65, 68},     
    {63, 66, 69},     
    {64, 67, 70},     
    {65, 68, 71},     
    {66, 69, 72},     
    {67, 70, 73},     
    {68, 71, 74},     
    {69, 72, 75},     
    {70, 73, 76},     
    {71, 74, 77},     

};

std::vector<std::vector<uint8_t>> augmentedChordGuitar = {
    {48, 52, 56, 60, 64},   // C Augmented x
    {49, 53, 57, 61, 65},
    {50, 54, 58, 62, 66},
    {51, 55, 59, 63, 67},
    {52, 56, 60, 64, 68},
    {53, 57, 61, 65, 69},
    {54, 58, 62, 66, 70},
    {55, 59, 63, 67, 71},
    {56, 60, 64, 68, 72},
    {57, 61, 65, 69, 73},
    {58, 62, 66, 70, 74},
    {59, 63, 67, 71, 75},
};

std::vector<std::vector<uint8_t>> major7thChordGuitar = {
    {48, 52, 55, 71, 64},   // C Major7 x
    {49, 53, 56, 72, 65},
    {50, 54, 57, 73, 66},
    {51, 55, 58, 74, 67},
    {52, 56, 59, 75, 68},
    {53, 57, 60, 76, 69},
    {54, 58, 61, 77, 70},
    {55, 59, 62, 78, 71},
    {56, 60, 63, 79, 72},
    {57, 61, 64, 80, 73},
    {58, 62, 65, 81, 74},
    {59, 63, 66, 82, 75},

};

std::vector<std::vector<uint8_t>> minor7thChordGuitar = {
    {48, 55, 58, 63, 67},   // C Minor7 x
    {49, 56, 59, 64, 68},
    {50, 57, 60, 65, 69},
    {51, 58, 61, 66, 70},
    {52, 59, 62, 67, 71},
    {53, 60, 63, 68, 72},
    {54, 61, 64, 69, 73},
    {55, 62, 65, 70, 74},
    {56, 63, 66, 71, 75},
    {57, 64, 67, 72, 76},
    {58, 65, 68, 73, 77},
    {59, 66, 69, 74, 78},

};

std::vector<std::vector<uint8_t>> dominant7thChordGuitar = {
    {48, 52, 58, 60, 64},   // C Dominant7 x
    {49, 56, 59, 65, 68},   // C# Dominant7 x
    {50, 57, 60, 66},   // D Dominant7 x
    {51, 58, 61, 67, 68},   // D# Dominant7 x
    {40, 47, 52, 56, 62, 64},   // E Dominant7 x
    {41, 48, 51, 57, 60},   // F Dominant7 x lowered
    {42, 46, 49, 52},   // F# Dominant7 x lowered
    //{55, 59, 62, 55, 59, 65},   // G Dominant7 x
    {43, 47, 50, 55, 59, 65},   // G Dominant7 x lowered
    {56, 63, 66, 72, 75},   // G# Dominant7 x
    {45, 52, 55, 61, 64},   // A Dominant7 x
    {46, 53, 56, 62, 65},   // A# Dominant7 x
    {47, 51, 57, 59, 66},   // B Dominant7 x
};

std::vector<std::vector<uint8_t>> minor7Flat5ChordGuitar = {
    {48, 51, 58, 60, 66},   // C Minor7Flat5 x
    {49, 52, 59, 61, 67},
    {50, 53, 60, 62, 68},
    {51, 54, 61, 63, 69},
    {52, 55, 62, 64, 70},
    {53, 56, 63, 65, 71},
    {54, 57, 64, 66, 72},
    {55, 58, 65, 67, 73},
    {56, 59, 66, 68, 74},
    {57, 60, 67, 69, 75},
    {58, 61, 68, 70, 76},
    {59, 62, 69, 71, 77},
};

std::vector<std::vector<uint8_t>> major6thChordGuitar = {
    {48, 52, 57, 60, 64},   // C Major6 x
    {49, 53, 58, 61, 65},
    {50, 54, 59, 62, 66},
    {51, 55, 60, 63, 67},
    {52, 56, 61, 64, 68},
    {53, 57, 62, 65, 69},
    {54, 58, 63, 66, 70},
    {55, 59, 64, 67, 71},
    {56, 60, 65, 68, 72},
    {57, 61, 66, 69, 73},
    {58, 62, 67, 70, 74},
    {59, 63, 68, 71, 75},

};

std::vector<std::vector<uint8_t>> minor6thChordGuitar = {
    {48, 51, 57, 60, 67},   // C Minor6 x
    {49, 52, 58, 61, 68},
    {50, 53, 59, 62, 69},
    {51, 54, 60, 63, 70},
    {52, 55, 61, 64, 71},
    {53, 56, 62, 65, 72},
    {54, 57, 63, 66, 73},
    {55, 58, 64, 67, 74},
    {56, 59, 65, 68, 75},
    {57, 60, 66, 69, 76},
    {58, 61, 67, 70, 77},
    {59, 62, 68, 71, 78},
   
};

std::vector<std::vector<uint8_t>> suspended2ChordGuitar = {
    {48, 50, 55, 62, 67},   // C Suspended2 x
    {49, 51, 56, 63, 68},
    {50, 52, 57, 64, 69},
    {51, 53, 58, 65, 70},
    {52, 54, 59, 66, 71},
    {53, 55, 60, 67, 72},
    {54, 56, 61, 68, 73},
    {55, 57, 62, 69, 74},
    {56, 58, 63, 70, 75},
    {57, 59, 64, 71, 76},
    {58, 60, 65, 72, 77},
    {59, 61, 66, 73, 78},

};

std::vector<std::vector<uint8_t>> suspended4ChordGuitar = {
    {48, 53, 55, 60, 65},   // C Suspended4 x
    {49, 56, 61, 66, 68},   // C# Suspended4 x
    {50, 57, 62, 67},       // D Suspende d4 x
    {51, 58, 63, 68, 70},   // D# Suspended4 x
    {40, 47, 52, 57, 59, 64},   // E Suspended4 x
    {53, 60, 65, 70, 72},   // F Suspended4 x
    {54, 61, 66, 71, 73},   // F# Suspended4 x
    {43, 50, 55, 60, 67},   // G Suspended4 x
    {56, 63, 68, 73, 75},   // G# Suspended4 x
    {45, 52, 57, 62, 64},   // A Suspended4 x
    {46, 53, 58, 63, 65},   // A# Suspended4 x
    {47, 54, 59, 64},   // B Suspended4, removed extra 59 x
};

std::vector<std::vector<uint8_t>> add9ChordGuitar = {
    {48, 52, 55, 62, 64},   // C Add9 x
    {49, 53, 56, 63, 65},
    {50, 54, 57, 64, 66},
    {51, 55, 58, 65, 67},
    {52, 56, 59, 66, 68},
    {53, 57, 60, 67, 69},
    {54, 58, 61, 68, 70},
    {55, 59, 62, 69, 71},
    {56, 60, 63, 70, 72},
    {57, 61, 64, 71, 73},
    {58, 62, 65, 72, 74},
    {59, 63, 66, 73, 75},

};

std::vector<std::vector<uint8_t>> add11ChordGuitar = {
    {48, 52, 55, 60, 65},   // C Add11 x
    {50, 54, 57, 62, 67},
    {51, 55, 58, 63, 68},
    {52, 56, 59, 64, 69},
    {53, 57, 60, 65, 70},
    {54, 58, 61, 66, 71},
    {55, 59, 62, 67, 72},
    {56, 60, 63, 68, 73},
    {57, 61, 64, 69, 74},
    {58, 62, 65, 70, 75},
    {59, 63, 66, 71, 76},

};

std::vector<std::vector<uint8_t>> ninthChordGuitar = {
    {48, 55, 58, 64, 70, 74},   // C Ninth x
    {49, 56, 59, 65, 71, 75},
    {50, 57, 60, 66, 72, 76},
    {51, 58, 61, 67, 73, 77},
    {52, 59, 62, 68, 74, 78},
    {53, 60, 63, 69, 75, 79},
    {54, 61, 64, 70, 76, 80},
    {55, 62, 65, 71, 77, 81},
    {56, 63, 66, 72, 78, 82},
    {57, 64, 67, 73, 79, 83},
    {58, 65, 68, 74, 80, 84},
    {59, 66, 69, 75, 81, 85},

};

std::vector<std::vector<uint8_t>> eleventhChordGuitar = {
    {43, 53, 58, 60, 64},   // C Eleventh x, but added E4
    {44, 54, 59, 61, 65},
    {45, 55, 60, 62, 66},
    {46, 56, 61, 63, 67},
    {47, 57, 62, 64, 68},
    {48, 58, 63, 65, 69},
    {49, 59, 64, 66, 70},
    {50, 60, 65, 67, 71},
    {51, 61, 66, 68, 72},
    {52, 62, 67, 69, 73},
    {53, 63, 68, 70, 74},
    {54, 64, 69, 71, 75},

};

std::vector<std::vector<uint8_t>> thirteenthChordGuitar = {
    {60, 62, 64, 57, 58},   // C Thirteenth X but AI
    {61, 63, 65, 58, 59},
    {62, 64, 66, 59, 60},
    {63, 65, 67, 60, 61},
    {64, 66, 68, 61, 62},
    {65, 67, 69, 62, 63},
    {66, 68, 70, 63, 64},
    {67, 69, 71, 64, 65},
    {68, 70, 72, 65, 66},
    {69, 71, 73, 66, 67},
    {70, 72, 74, 67, 68},
    {71, 73, 75, 68, 69},

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
void preparePatterns()
{
  SequencerPatternA.clear();
  SequencerPatternB.clear();
  SequencerPatternC.clear();
  PatternNote p;
  //pattern 1
  p.degree = 1;
  p.offset = 1;
  p.velocity = 127;
  p.holdTime = 48;
  SequencerPatternA.push_back(p);
  p.degree = 2;
  p.offset = 25;
  p.velocity = 127;
  p.holdTime = 24;
  SequencerPatternA.push_back(p);
  p.degree = 3;
  p.offset = 37;
  p.velocity = 127;
  p.holdTime = 36;
  SequencerPatternA.push_back(p);

  //pattern 2
  p.degree = 3;
  p.offset = 1;
  p.velocity = 127;
  p.holdTime = 48;
  SequencerPatternB.push_back(p);
  p.degree = 2;
  p.offset = 25;
  p.velocity = 127;
  p.holdTime = 24;
  SequencerPatternB.push_back(p);
  p.degree = 1;
  p.offset = 37;
  p.velocity = 127;
  p.holdTime = 36;
  SequencerPatternB.push_back(p);
  
  //pattern 3
  p.degree = 2;
  p.offset = 1;
  p.velocity = 127;
  p.holdTime = 48;
  SequencerPatternC.push_back(p);
  p.degree = 3;
  p.offset = 25;
  p.velocity = 127;
  p.holdTime = 24;
  SequencerPatternC.push_back(p);
  p.degree = 1;
  p.offset = 37;
  p.velocity = 127;
  p.holdTime = 36;
  SequencerPatternC.push_back(p);
}

enum OmniChordModeType {
  OmniChordOffType = 0,
  OmniChordStandardType,
  OmniChordStandardHoldType,
  OmniChordGuitarType,
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
class neckAssignment
{
  public:
    Note key;
    ChordType chordType;
    neckAssignment()
    {
      key = NO_NOTE;
      chordType = majorChordType;
    }
};
std::vector<std::vector<neckAssignment>> neckAssignments; //[preset][neck assignment 0-27]

//current presets:
// 1 standard
// 2 standard but different 3rd column sus4
// 3 standard but different 3rd column major 6th
// 4 simple mode
// 5 omnichord mode
// 6 omnichord mode but hybrid mode
void prepareNeck()
{
  Note myN;
  neckAssignments.clear();
  std::vector<neckAssignment> tempNeck;
  //preset 1
  tempNeck.clear();
  for (int i = 0; i < NECK_ACTUALROWS; i++)
  {
    
    switch(i)
    {
      case 0:
        myN = C_NOTE; // C
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
    for (uint8_t j = 0; j < NECK_COLUMNS; j++)
    {
      neckAssignment n;
      switch(j)
      {
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
  for (int i = 0; i < NECK_ACTUALROWS; i++)
  {
    neckAssignment n;
    switch(i)
    {
      case 0:
        n.key = C_NOTE; // C
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
    for (uint8_t j = 0; j < NECK_COLUMNS; j++)
    {
      switch(j)
      {
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
  for (int i = 0; i < NECK_ACTUALROWS; i++)
  {
    neckAssignment n;
    switch(i)
    {
      case 0:
        n.key = C_NOTE; // C
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
    for (uint8_t j = 0; j < NECK_COLUMNS; j++)
    {
      switch(j)
      {
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
  for (int i = 0; i < NECK_ACTUALROWS; i++)
  {
    neckAssignment n;
    n.chordType = majorChordType;
    switch(i)
    {
      case 0:
        n.key = C_NOTE; // C
        
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
    for (uint8_t j = 0; j < NECK_COLUMNS; j++)
    {
      tempNeck.push_back(n);
    }
  }
  neckAssignments.push_back(tempNeck);
  //preset 5
  tempNeck.clear();
  for (int i = 0; i < NECK_ACTUALROWS; i++)
  {
    
    switch(i)
    {
      case 0:
        myN = C_NOTE; // C
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
    for (uint8_t j = 0; j < NECK_COLUMNS; j++)
    {
      neckAssignment n;
      switch(j)
      {
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
  for (int i = 0; i < NECK_ACTUALROWS; i++)
  {
    
    switch(i)
    {
      case 0:
        myN = C_NOTE; // C
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
    for (uint8_t j = 0; j < NECK_COLUMNS; j++)
    {
      neckAssignment n;
      switch(j)
      {
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

std::vector<uint8_t> getGuitarChordNotesFromNeck(uint8_t row, uint8_t column, uint8_t usePreset)
{
  //given preset get neck Assignment:
  neckAssignment n = neckAssignments[usePreset][row * NECK_COLUMNS + column]; 
  //Serial.printf("GT row %d col %d preset %d\n",row, column, usePreset);
  //Serial.printf("ChordType GT to chord = %d\n",(int)n.chordType);
  //Serial.printf("ChordType GT to key = %d\n",(int)n.key);
  //now based on neck assignment, we will need to determine enum value then return the value
  return allChordsGuitar[(uint8_t)n.chordType][(uint8_t)n.key];
}

Chord getKeyboardChordNotesFromNeck(uint8_t row, uint8_t column, uint8_t usePreset)
{
  //given preset get neck Assignment:
  //Serial.printf("KB row %d col %d preset %d index %d\n",row, column, usePreset, row * NECK_COLUMNS + column);

  neckAssignment n = neckAssignments[usePreset][row * NECK_COLUMNS + column]; 
  //Serial.printf("ChordType KB to row = %d preset = %d\n",(int)n.chordType, usePreset);
  //now based on neck assignment, we will need to determine enum value then return the value
  return chords[(uint8_t)n.chordType];
}

void prepareConfig()
{
  presetBPM.clear();
  omniChordModeGuitar.clear();
  TimeNumerator.clear();
  TimeDenominator.clear();
  MaxBeatsPerBar.clear();
  strumSeparation.clear();
  QUARTERNOTETICKS.clear();
  muteWhenLetGo.clear();
  ignoreSameChord.clear();
  chordHold.clear();
  simpleChordSetting.clear();

  //Serial.printf("prepareConfig! Enter\n");
  uint8_t temp8;
  uint16_t temp16;
  for (uint8_t i = 0; i< MAX_PRESET; i++)
  {
    //set default values first:
    temp16 = 128 + i * 12;
    presetBPM.push_back(temp16);
    if (i == 4)
    {
      omniChordModeGuitar.push_back(OmniChordStandardType);
    }
    else if (i == 5)
    {
      omniChordModeGuitar.push_back(OmniChordGuitarType);
    }
    else
    {
      omniChordModeGuitar.push_back(0);
    }
    temp8 = 4;
    TimeNumerator.push_back(temp8);
    temp8 = 12;
    TimeDenominator.push_back(temp8);
    MaxBeatsPerBar.push_back(TimeNumerator.back() * TimeDenominator.back());
    temp8 = 1;
    strumSeparation.push_back(temp8);
    temp16 = 48;
    QUARTERNOTETICKS.push_back(temp16);
    muteWhenLetGo.push_back(false);
    ignoreSameChord.push_back(true);
    ignoreSameChord[4] = false;
    chordHold.push_back(true);
    simpleChordSetting.push_back(true);
  }
  //Serial.printf("prepareConfig! Exit\n");
}

void prepareChords()
{
  //todo update based on config (simple) + patterns set based on presets
  std::vector<AssignedPattern> assignedFretPatterns;
  assignedFretPatternsByPreset.clear();
  uint8_t rootNotes[] = {48, 50, 52, 53, 55, 57, 59};
  // Populate the assignedFretPatterns with the chords for each root note
  for (int x = 0; x < MAX_PRESET; x++)
  {
    assignedFretPatterns.clear();
    for (int i = 0; i < NECK_ROWS; i++) 
    {
      for (int j = 0; j < NECK_COLUMNS; j++)
      {
        assignedFretPatterns.push_back(AssignedPattern(simpleChordSetting[x], getKeyboardChordNotesFromNeck(i,j,x), getGuitarChordNotesFromNeck(i,j,x), rootNotes[i], false,0));
      }
    }
    lastPressedChord = assignedFretPatterns[0].assignedChord;
    lastPressedGuitarChord = allChordsGuitar[0][0]; // c major default strum
    //other ignored
    for (int i = 0; i < NECK_ACTUALROWS - NECK_ROWS; ++i) 
    {
        for (int j = 0; j < NECK_COLUMNS; j++)
      {
        assignedFretPatterns.push_back(AssignedPattern(simpleChordSetting[x], getKeyboardChordNotesFromNeck(i,j,x), getGuitarChordNotesFromNeck(i+7,j,x), rootNotes[i], true,0));
      } 
    }

    //Serial.printf("assignedFretPatterns size is %d\n", assignedFretPatterns.size());
    assignedFretPatternsByPreset.push_back(assignedFretPatterns);
  }
  //Serial.printf("assignedFretPatternsByPreset size is %d\n", assignedFretPatternsByPreset.size());
}

void printChords()
{
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
  deviceBPM = newBPM;
  unsigned int newInterval = computeTickInterval(deviceBPM);
  tickTimer.begin(tickISR, newInterval); // restart timer with new interval
}

//generic preset Change handler when vectors aren't enough
void presetChanged()
{
  setNewBPM(presetBPM[preset]);
  omniChordNewNotes.clear();
  detector.transposeReset();
  lastOmniNotes.clear();
  //if all sounds must be stopped
  if (stopSoundsOnPresetChange)
  {
    noteAllOff();
  }
  neckButtonPressed = -1; // set last button is null
}

void setup() {
  strum = 0;
  pinMode(NOTE_OFF_PIN, INPUT_PULLUP);
  pinMode(START_TRIGGER_PIN, INPUT_PULLUP);
  pinMode(BUTTON_1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_2_PIN, INPUT_PULLUP);
  pinMode(BUTTON_3_PIN, INPUT_PULLUP);
  pinMode(BT_ON_PIN, OUTPUT);  // Set pin 19 as output
  pinMode(BT_STATUS_PIN, INPUT);  // Set pin 18 as output
  
  digitalWrite(BT_ON_PIN, HIGH);  // Set pin 19 to high
  
  Serial1.begin(250000); // Guitar
  Serial2.begin(250000); // Keyboard
  Serial3.begin(9600);  // BT
  //Serial4.begin(250000); // Cyber G to Piano

      //HM10_BLUETOOTH.begin(HM10_BAUD); //BT
  Serial.begin(115200);  // USB debug monitor

  usbMIDI.begin();

  while (!Serial && millis() < 3000);  // Wait for Serial Monitor
  if (debug)
  {
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
  if (!loadSettings())
  {
    if (debug)
    {
      Serial.println("Error! Failed to open file for reading.");
    }
  }
  prepareChords();
  tickTimer.begin(clockISR, computeTickInterval(deviceBPM)); // in microseconds
}

void sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity = 127) {
  usbMIDI.sendNoteOn(note, velocity, channel);
  if (debug)
  {
    Serial.printf("Note ON: ch=%d note=%d vel=%d\n", channel, note, velocity);
  }
}

void sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity = 0) {
  usbMIDI.sendNoteOff(note, velocity, channel);
  if (debug)
  {
    Serial.printf("Note OFF: ch=%d note=%d vel=%d\n", channel, note, velocity);
  }
}
void sendStart() 
{
  usbMIDI.sendRealTime(0xFA);
  if (debug)
  {
    Serial.printf("Start!\n");
  }
}

void sendStop() 
{
  usbMIDI.sendRealTime(0xFC);
  if (debug)
  {
    Serial.printf("Stop\n");
  }
}

void sendContinue() 
{
  usbMIDI.sendRealTime(0xFB);
  if (debug)
  {
    Serial.printf("Continue\n");
  }
  playMode = 1;
}

void sendProgram(uint8_t channel, uint8_t program) {
  if (program == 2) //right
  {
    curProgram = (curProgram + 1)%128;
  }
  else
  {
    curProgram = curProgram - 1;
    if (curProgram < 0)
    {
      curProgram = 127;
    }
  }
  
  usbMIDI.sendProgramChange(curProgram, channel);
  if (debug)
  {
    Serial.printf("Program: ch=%d program=%d\n", channel, curProgram);
  }
}

void sendCC(uint8_t channel, uint8_t cc, uint8_t value) {
  
  //usbMIDI.sendControlChange(cc, value, channel);
  //Serial.printf("CC: ch=%d cc=%d val=%d\n", channel, cc, value);
  if (cc == 9) // transpose up
  {
    if (transpose < 12)
    {
      transpose++;
    }
  }
  else if (cc == 3) //transpose down
  {
    if (transpose > -12)
    {
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

#define MAX_SERIAL_FIELDS 49         // Max number of comma-separated fields, worst case is 16*3+1
#define MAX_SERIAL_FIELD_LENGTH 3
void handleSerialCommand(HardwareSerial &port, const char* label = "") {
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
        port.print(label); port.print("Field ");
        port.print(i); port.print(": ");
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

void BPMTap() 
{
    unsigned long currentTime = millis();

    // Reset if more than 3 seconds since last press
    if (currentTime - lastPressTime > 2000) {
        pressCount = 0;
        firstPressTime = currentTime;
    }

    lastPressTime = currentTime;
    pressCount++;

    if (pressCount == 1) {
        firstPressTime = currentTime; // Record first tap
    }

    if (pressCount >= 2) 
    {
        unsigned long timeInterval = lastPressTime - firstPressTime;
        float bpm = (pressCount - 1) * 60000.0 / timeInterval;
        if (debug)
        {
          Serial.print("New BPM: ");
          Serial.println((int)bpm);
        }
        presetBPM[preset] = (int)bpm;
        setNewBPM(presetBPM[preset]);
        
    }
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
    if (strncmp(buffer, "aa55", 4) == 0) 
    {
      break;
    }
    memmove(buffer, buffer + 2, bufferLen - 1);
    bufferLen -= 2;
    buffer[bufferLen] = '\0';
  }
  bool ignore;
  // --- Step 5: Main parser loop ---
  while (true) 
  {
    ignore = false;
    uint8_t myTranspose = transpose;
    bool processed = false;
    bool matched = false;
    if (bufferLen > 12)
    {
      for (const HexToProgram& msg : hexToProgram) {
              size_t len = strlen(msg.hex);
              if (bufferLen >= len && strncmp(buffer, msg.hex, len) == 0) {
                sendCC(channel, msg.program, 127);
                memmove(buffer, buffer + len, bufferLen - len + 1);
                bufferLen -= len;
                matched = true;
                processed = true;
                break;
              }
            }

            if (matched) continue;
    }
    if (bufferLen >= 22 and !matched) 
    {
      for (const MidiMessage& msg : hexToNote)  
      {
        size_t len = strlen(msg.hex);
        last_len = len;
        

        if (bufferLen >= len && strncmp(buffer, msg.hex, len) == 0) 
        {
          if (msg.note >= MIN_IGNORED_GUITAR && msg.note <= MAX_IGNORED_GUITAR)
          {
            //todo add handling for C5, D4-D6 - D6 = drum start/stop, D5 - BPM compute tap. C5 - Sustain of sorts, D4 - Fill
            if (msg.note == BPM_COMPUTE_BUTTON)
            {
              BPMTap();
            }
          }
          else
          {
            lastNeckButtonPressed = neckButtonPressed;
            if (lastNeckButtonPressed != -1)
            {
              lastValidNeckButtonPressed = neckButtonPressed;
            }
            if (msg.note != 255) 
            {
              neckButtonPressed = msg.note;

              if (!isKeyboard)
              {
                myTranspose = 0;
              }
              else
              {
                myTranspose = transpose;
              }
              if (debug)
              {
                Serial.printf("Get simple %d button = %d\n",assignedFretPatternsByPreset[preset][msg.note].getSimple(), msg.note);
              }
              //guitar plays chord on button press
              if (isKeyboard || omniChordModeGuitar[preset] > OmniChordOffType) 
              {
                ignore = true;
                if (omniChordModeGuitar[preset] > 0 && !isKeyboard)
                {
                  //check if button pressed is the same
                  //if (lastNeckButtonPressed != neckButtonPressed || omniChordNewNotes.size() == 0)
                  if (lastValidNeckButtonPressed != neckButtonPressed || omniChordNewNotes.size() == 0)
                  {
                    //need to create a note list
                    omniChordNewNotes.clear();
                    int offset = -12;
                    
                    std::vector<uint8_t> chordNotes = assignedFretPatternsByPreset[preset][msg.note].getChords().getCompleteChordNotes(); //get notes
                    
                    //Serial.printf("chordNotes = ");
                    //for (uint8_t i = 0; i < chordNotes.size(); i++)
                    //{
                      //Serial.printf("%d ", chordNotes[i]);
                    //}
                    //Serial.printf("\n");
                    while (omniChordNewNotes.size() < omniChordOrigNotes.size())
                    {
                      if (omniChordNewNotes.size() != 0)
                      {
                        offset += 12;
                      }
                      for (uint8_t i = 0; i < chordNotes.size() && omniChordNewNotes.size() < omniChordOrigNotes.size(); i++)
                      {
                        omniChordNewNotes.push_back(chordNotes[i] + offset);
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
                if (assignedFretPatternsByPreset[preset][msg.note].getSimple() && omniChordModeGuitar[preset] != OmniChordGuitarType)
                {
                  lastSimple = true;
                  if (lastGuitarNotes.size() > 0)
                  {
                    for (uint8_t i = 0; i < lastGuitarNotes[i]; i++) 
                    {
                      sendNoteOff(channel, lastGuitarNotes[i]);
                    }  
                    lastGuitarNotes.clear();
                  }
                  rootNote = assignedFretPatternsByPreset[preset][msg.note].getChords().getRootNote();
                  if (debug)
                  {
                    Serial.printf("Root note is %d\n", rootNote);
                  }
                  sendNoteOn(channel, rootNote + myTranspose);
                  lastGuitarNotes.push_back(rootNote + myTranspose);
                  for (uint8_t note : assignedFretPatternsByPreset[preset][msg.note].getChords().getChordNotes()) 
                  {
                    sendNoteOn(channel, rootNote + note + myTranspose);
                    lastGuitarNotes.push_back(rootNote+note + myTranspose);
                  }
                }
                else if (omniChordModeGuitar[preset] != OmniChordGuitarType)
                {
                  lastSimple = false;
                  if (debug)
                  {
                    Serial.printf("Not simple!");
                  }
                  //play pattern here
                }
                else
                {
                  ignore = false; //force to still use paddle
                  //do nothing
                }
                sendNoteOn(GUITAR_BUTTON_CHANNEL, msg.note,1); //play the specific note on another channel at minimum non 0 volume
                lastGuitarNotesButtons.push_back(msg.note);
              }
              //else //using paddle adapter
              if (!ignore)
              {
                //mutes previous if another button is pressed
                //Serial.printf("MuteWhenLetGo %d, ignoreSameChord[preset] %d, LastValidpressed = %d neckPressed = %d\n", muteWhenLetGo[preset]?1:0, ignoreSameChord[preset]?1:0, lastValidNeckButtonPressed, neckButtonPressed);
                if (lastValidNeckButtonPressed >= 0 && !muteWhenLetGo[preset]) 
                {
                  //std::vector<uint8_t> chordNotes = assignedFretPatternsByPreset[preset][msg.note].getChords().getCompleteChordNotes(); //get notes
                  //check if same notes are to be played
                  bool ignorePress = false;
                  if (ignoreSameChord[preset])
                  {
                    //Serial.printf("lastGuitarNotes size %d SequencerNotes size %d\n", lastGuitarNotes.size(), SequencerNotes.size());
                    bool isEqual = true;
                    std::vector<uint8_t> chordNotesA = assignedFretPatternsByPreset[preset][lastValidNeckButtonPressed].getChords().getCompleteChordNotes(); //get notes
                    std::vector<uint8_t> chordNotesB = assignedFretPatternsByPreset[preset][neckButtonPressed].getChords().getCompleteChordNotes(); //get notes

                    for (uint8_t i = 0; i < chordNotesA.size() && i <chordNotesB.size(); i++)
                    {
                      if (chordNotesA[i] != chordNotesB[i])
                      {
                        if (debug)
                        {
                          Serial.printf("Chords are not the same! %d vs %d\n", chordNotesA[i], chordNotesB[i]);
                        }
                        isEqual = false;
                        break;
                      }
                    }
                    if (isEqual)
                    {
                      ignorePress = true;
                    }
                  }
                  if (debug)
                  {
                    Serial.printf("ignorePress = %d\n", ignorePress?1:0);
                  }
                  //cancelGuitarChordNotes(assignedFretPatternsByPreset[preset][lastNeckButtonPressed].assignedGuitarChord);
                  //for paddle we actually stop playing when user lets go of "strings"
                  if (!ignorePress)
                  {
                    while (lastGuitarNotes.size() > 0)
                    {
                      sendNoteOff(channel, lastGuitarNotes.back());
                      lastGuitarNotes.pop_back();
                    }
                    while (lastGuitarNotesButtons.size() > 0)
                    {
                      sendNoteOff(channel, lastGuitarNotesButtons.back());
                      lastGuitarNotesButtons.pop_back();
                    }
                    
                    //handle string let go case and turn off all notes
                    for (uint8_t i = 0; i < SequencerNotes.size(); i++)
                    {
                      if (SequencerNotes[i].channel == GUITAR_CHANNEL)
                      {
                        SequencerNotes[i].offset = 0;
                        SequencerNotes[i].holdTime = 0;
                      }
                    }
                  }
                }
                //do nothing as we only care about neck tracking
              }
            }
            else
            {
              lastNeckButtonPressed = neckButtonPressed;
              neckButtonPressed = -1;
              if (muteWhenLetGo[preset])
              {
                //for paddle we actually stop playing when user lets go of "strings"
                while (lastGuitarNotes.size() > 0)
                {
                  sendNoteOff(channel, lastGuitarNotes.back());
                  lastGuitarNotes.pop_back();
                }
                while (lastGuitarNotesButtons.size() > 0)
                {
                  sendNoteOff(channel, lastGuitarNotesButtons.back());
                  lastGuitarNotesButtons.pop_back();
                }
                if (lastNeckButtonPressed != -1 && !isKeyboard)
                {
                  //handle string let go case and turn off all notes
                  for (uint8_t i = 0; i < SequencerNotes.size(); i++)
                  {
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
          processed = true; //did something
          break;
        }
      }
      if (matched) continue;
      else
      {
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

void checkSerialBT(HardwareSerial& serialPort, char* buffer, uint8_t& bufferLen) 
{

  bool buttonBTState = digitalRead(BT_STATUS_PIN);
  if (prevButtonBTState != buttonBTState)
  {
    if (debug)
    {
      Serial.printf("BT is %d\n", buttonBTState?1:0);
    }
    if (buttonBTState)
    {
      digitalWrite(BT_ON_PIN, LOW);  // Set pin 19 to high
    }
    else
    {
      digitalWrite(BT_ON_PIN, HIGH);  // Set pin 19 to high
    }
    prevButtonBTState = buttonBTState;
    
  }
  while (serialPort.available()) {
    char c = serialPort.read();
    Serial.print(c);

  }
}

template <typename SerialType>
bool decodeCmd(SerialType& serialPort, String cmd, std::vector<String> *params)
{
  char buffer[64];
  bool bTemp = false;
  if (debug)
  {
    Serial.printf("Serial command received is %s\n", cmd.c_str());
  }
  if (cmd == "SAVE") 
  {
    if (saveSettings())
    {
      serialPort.write("OK00\r\n");
    }
    else
    {
      serialPort.write("ER00\r\n");
    }
    // Match found
  }
  else if (cmd == "RSTP")  // RESET Please
  {
    prepareConfig();
    prepareNeck();
    preparePatterns();
    prepareChords();
    preset = 0;
    setNewBPM(presetBPM[preset]);
    noteAllOff();
    serialPort.write("OK00\r\n");
  }
  else if (cmd == "RCFG") //read conffig
  {
    if (params->size() == 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) == 0)
    {
      printFileContents("config.ini", true);
    }
    else if (atoi(params->at(0).c_str()) == 1)
    {
      printFileContents("config2.ini", true);
    }
    else
    {
      serialPort.write("ER01\r\n");  
    }
    serialPort.write("OK00\r\n");
      
  }
  else if (cmd == "RINI") //read config but through serial debug
  {
    if (params->size() == 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) == 0)
    {
      bTemp = printFileContents("config.ini", false);
    }
    else if (atoi(params->at(0).c_str()) == 1)
    {
      bTemp = printFileContents("config2.ini", false);
    }
    else
    {
      serialPort.write("ER01\r\n");  
      return true;
    }
    if (!bTemp)
    {
      serialPort.write("ER02\r\n");  
      return true;
    }
    serialPort.write("OK00\r\n");
      
  }
  else if (cmd == "LOAD") //reload config files
  {
    if (loadSettings())
    {
      serialPort.write("OK00\r\n");
    }
    else
    {
      serialPort.write("ER00\r\n");
    }
  }
  else if (cmd == "PRSW") //Set Current Preset
  {
    if (params->size() == 0) //missing parameter
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) >= MAX_PRESET || atoi(params->at(0).c_str()) < 0)
    {
      serialPort.write("ER01\r\n"); //invalid parameter
      return true;
    }
    uint8_t newPreset = atoi(params->at(0).c_str());
    if (newPreset != preset)
    {
      presetChanged();
      preset = newPreset;
    }
    serialPort.write("OK00\r\n");
  }
  else if (cmd == "PRSR") //Read Current Preset
  {
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", preset);
    serialPort.write(buffer);
  }
  else if (cmd == "DBGW") //Set Debug setting 
  {
    if (params->size() == 0) //missing parameter
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > 1 || atoi(params->at(0).c_str()) < 0)
    {
      serialPort.write("ER01\r\n"); //invalid parameter
      return true;
    }
    debug = atoi(params->at(0).c_str()) > 0;
    serialPort.write("OK00\r\n");
  }
  else if (cmd == "DBGR") //Debug setting read
  {
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", debug?1:0);
    serialPort.write(buffer);
  }

  else if (cmd == "SPCW") //stopSoundsOnPresetChange
  {
    if (params->size() == 0) 
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > 1 || atoi(params->at(0).c_str()) < 0)
    {
      serialPort.write("ER01\r\n"); //invalid parameter
      return true;
    }
    stopSoundsOnPresetChange = atoi(params->at(0).c_str()) > 0;
    serialPort.write("OK00\r\n");
  }
  else if (cmd == "SPCR") //stopSoundsOnPresetChange read
  {
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", stopSoundsOnPresetChange?1:0);
    serialPort.write(buffer);
  }

  else if (cmd == "CLKW") //midiClockEnable
  {
    if (params->size() == 0) 
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > 1 || atoi(params->at(0).c_str()) < 0)
    {
      serialPort.write("ER01\r\n"); //invalid parameter
      return true;
    }
    midiClockEnable = atoi(params->at(0).c_str()) > 0;
    serialPort.write("OK00\r\n");
  }
  else if (cmd == "CLKR") //midiClockEnable
  {
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", midiClockEnable?1:0);
    serialPort.write(buffer);
  }
  else if (cmd == "BPMW") //presetBPM write
  {
    if (params->size() < 2) 
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > 300 || atoi(params->at(1).c_str()) < 30)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    presetBPM[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str());
    if (preset == atoi(params->at(0).c_str()))
    {
      setNewBPM(presetBPM[atoi(params->at(0).c_str())]);
    }
    serialPort.write("OK00\r\n");
  }
  else if (cmd == "BPMR") //presetBPM Read
  {
    if (params->size() < 1) 
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", presetBPM[atoi(params->at(0).c_str())]);
    serialPort.write(buffer);
  }
  else if (cmd == "OMMW") //omniChordModeGuitar write
  {
    if (params->size() < 2) 
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > (int) OmniChordGuitarType || atoi(params->at(1).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    omniChordModeGuitar[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str());
    serialPort.write("OK00\r\n");
  }
  else if (cmd == "OMMR") //omniChordModeGuitar Read
  {
    if (params->size() < 1) 
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", omniChordModeGuitar[atoi(params->at(0).c_str())]);
    serialPort.write(buffer);
  }
  else if (cmd == "MWLW") //muteWhenLetGo write
  {
    if (params->size() < 2) 
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > 1 || atoi(params->at(1).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    muteWhenLetGo[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str()) > 0;
    serialPort.write("OK00\r\n");
  }
  else if (cmd == "MWLR") //muteWhenLetGo Read
  {
    if (params->size() < 1) 
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", muteWhenLetGo[atoi(params->at(0).c_str())]?1:0);
    serialPort.write(buffer);
  }
  else if (cmd == "ISCW") //ignoreSameChord write
  {
    if (params->size() < 2) 
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > 1 || atoi(params->at(1).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    ignoreSameChord[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str()) > 0;
    serialPort.write("OK00\r\n");
  }
  else if (cmd == "ISCR") //ignoreSameChord Read
  {
    if (params->size() < 1) 
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", ignoreSameChord[atoi(params->at(0).c_str())]?1:0);
    serialPort.write(buffer);
  }
  else if (cmd == "CH_W") //chordHold write
  {
    if (params->size() < 2) 
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > 1 || atoi(params->at(1).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    chordHold[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str()) > 0;
    serialPort.write("OK00\r\n");
  }
  else if (cmd == "CH_R") //chordHold Read
  {
    if (params->size() < 1) 
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", chordHold[atoi(params->at(0).c_str())]?1:0);
    serialPort.write(buffer);
  }
  else if (cmd == "STSW") //strumSeparation write
  {
    if (params->size() < 2) 
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > MAX_STRUM_SEPARATION || atoi(params->at(1).c_str()) < MIN_STRUM_SEPARATION)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    strumSeparation[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str());
    serialPort.write("OK00\r\n");
  }
  else if (cmd == "STSR") //strumSeparation Read
  {
    if (params->size() < 1) 
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", strumSeparation[atoi(params->at(0).c_str())]);
    serialPort.write(buffer);
  }
  else if (cmd == "SCSW") //simpleChordSetting write
  {
    if (params->size() < 2) 
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(1).c_str()) > 1 || atoi(params->at(1).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    simpleChordSetting[atoi(params->at(0).c_str())] = atoi(params->at(1).c_str()) > 0;
    serialPort.write("OK00\r\n");
  }
  else if (cmd == "SCSR") //simpleChordSetting Read
  {
    if (params->size() < 1) 
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d\r\n", simpleChordSetting[atoi(params->at(0).c_str())]?1:0);
    serialPort.write(buffer);
  }
  //simple
  //std::vector<uint16_t> QUARTERNOTETICKS;
  //std::vector<uint8_t> TimeNumerator;
  //std::vector<uint8_t> TimeDenominator; // in terms of 1/48 notes
  //std::vector<uint16_t> MaxBeatsPerBar;

  //complex
  //std::vector<std::vector<AssignedPattern>> assignedFretPatternsByPreset;
  else if (cmd == "NASW") //neckAssignments write
  {
    if (params->size() < 5) 
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    //row
    if (atoi(params->at(1).c_str()) > NECK_ACTUALROWS || atoi(params->at(1).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    //column
    if (atoi(params->at(2).c_str()) > NECK_COLUMNS || atoi(params->at(2).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(3).c_str()) > (int) B_NOTE || atoi(params->at(3).c_str()) < (int) C_NOTE)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    if (atoi(params->at(4).c_str()) > (int) thirteenthChordType || atoi(params->at(4).c_str()) < (int) majorChordType)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    neckAssignments[atoi(params->at(0).c_str())][atoi(params->at(1).c_str())*NECK_COLUMNS+atoi(params->at(2).c_str())].key = (Note) atoi(params->at(3).c_str());
    neckAssignments[atoi(params->at(0).c_str())][atoi(params->at(1).c_str())*NECK_COLUMNS+atoi(params->at(2).c_str())].chordType = (ChordType) atoi(params->at(4).c_str());
    serialPort.write("OK00\r\n");
  }
  else if (cmd == "NASR") //simpleChordSetting Read
  {
    if (params->size() < 3) 
    {
      serialPort.write("ER00\r\n");
      return true;
    }
if (atoi(params->at(0).c_str()) > MAX_PRESET || atoi(params->at(0).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    //row
    if (atoi(params->at(1).c_str()) > NECK_ACTUALROWS || atoi(params->at(1).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    //column
    if (atoi(params->at(2).c_str()) > NECK_COLUMNS || atoi(params->at(2).c_str()) < 0)
    {
      serialPort.write("ER00\r\n");
      return true;
    }
    snprintf(buffer, sizeof(buffer), "OK00,%d,%d\r\n", (int) neckAssignments[atoi(params->at(0).c_str())][atoi(params->at(1).c_str())*NECK_COLUMNS+atoi(params->at(2).c_str())].key, (int) neckAssignments[atoi(params->at(0).c_str())][atoi(params->at(1).c_str())*NECK_COLUMNS+atoi(params->at(2).c_str())].chordType);
    serialPort.write(buffer);
  }
  return true;
}


template <typename SerialType>
void checkSerialCmd(SerialType& serialPort, char* buffer, uint8_t& bufferLen) 
{
  std::vector<String> params;
  while (serialPort.available()) {
    char c = serialPort.read();

    // Prevent buffer overflow
    if (bufferLen < 255) {
      buffer[bufferLen++] = c;
    }

    // Look for command terminator
    if (c == '\n' && bufferLen >= 2 && buffer[bufferLen - 2] == '\r') {
      buffer[bufferLen] = '\0'; // Null-terminate the string

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
      if (!decodeCmd(serialPort, String(cmd), &params))
      {
        serialPort.write("ER99\r\n");
      }
      // Reset buffer for next command
      bufferLen = 0;
    }
  }
}


void cancelGuitarChordNotes(const std::vector<uint8_t>& chordNotes) {
  // If the input chord is empty, there's nothing to cancel
  if (chordNotes.empty()) {
    return;
  }

  // For each note in the chord, look for matching notes in SequencerNotes
  for (uint8_t chordNote : chordNotes) {
    for (auto& seqNote : SequencerNotes) {
      if (seqNote.note == chordNote && seqNote.channel == GUITAR_CHANNEL) {
        // Match found — cancel the note
        seqNote.offset = 0;
        seqNote.holdTime = 0;
      }
    }
  }
}


// This function builds a list of SequencerNotes from a given chord.
// If 'reverse' is true, the notes are added in reverse order.
void buildGuitarSequencerNotes(const std::vector<uint8_t>& chordNotes, bool reverse = false) {
  
  uint8_t currentOffset = 1;
  
  if (reverse) {
    // Loop through the chord notes in reverse
    for (auto it = chordNotes.rbegin(); it != chordNotes.rend(); ++it) {
      SequencerNote note(*it, 65535, currentOffset, GUITAR_CHANNEL);
      SequencerNotes.push_back(note);
      currentOffset += strumSeparation[preset];
    }
  } else {
    // Loop through the chord notes in normal order
    for (uint8_t midiNote : chordNotes) {
      SequencerNote note(midiNote, 65535, currentOffset, GUITAR_CHANNEL);
      SequencerNotes.push_back(note);
      currentOffset += strumSeparation[preset]; 
    }
  }
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
  if (!ignoringIdlePing)
  {
    //Serial.printf("Raw buffer (KB): %s\n", buffer);
  }

  // --- Step 2: If we're ignoring idle ping, skip until we see a valid start ---
  if (ignoringIdlePing) {
    while (bufferLen >= 2) {
      if ((buffer[0] == '8' && buffer[1] == '0') ||
          (buffer[0] == '9' && buffer[1] == '0') ||
          //(bufferLen >= 4 && strncmp(buffer, "f555", 4) == 0)) {
          (buffer[0] == 'f' && buffer[1] == '5') ){
        ignoringIdlePing = false;
        break; // valid data found, resume normal parsing
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
    isKeyboard = true;
    memmove(buffer, buffer + 12, bufferLen - 11);
    bufferLen -= 12;
    buffer[bufferLen] = '\0';
    return;
  }
  //handling for guitar strum attachment
  if (bufferLen >= 16 && strncmp(buffer, "f55500042017", 12) == 0)
  {
    isKeyboard = false;
    // Get the value of the last 2 bytes
    uint16_t value = 0;
    sscanf(buffer + 12, "%04hx", &value);

    // we don't care about this in omnichord mode
    //if (omniChordModeGuitar[preset] == 0)
    //{
      lastStrum = strum;
      //Serial.printf("Preset is %d\n", preset);
      int lastPressed = neckButtonPressed;
      if (chordHold[preset])
      {
        if (lastPressed == -1 && lastValidNeckButtonPressed != -1)
        {
          lastPressed = lastValidNeckButtonPressed;            
        }
      }
      if (value >= 0x200 && (lastPressed > -1)) { 
          strum = -1; // up
          if (omniChordModeGuitar[preset] == OmniChordOffType || omniChordModeGuitar[preset] == OmniChordGuitarType)
          {
            cancelGuitarChordNotes(assignedFretPatternsByPreset[preset][lastPressed].assignedGuitarChord);
            buildGuitarSequencerNotes(assignedFretPatternsByPreset[preset][lastPressed].assignedGuitarChord, true);
          }
          else
          {
            detector.transposeUp();
          }
          //queue buttons to be played
      } else if (value >= 0x100 && lastPressed > -1) {
          strum = 1;//down
          if (omniChordModeGuitar[preset] == OmniChordOffType || omniChordModeGuitar[preset] == OmniChordGuitarType)
          {
            cancelGuitarChordNotes(assignedFretPatternsByPreset[preset][lastPressed].assignedGuitarChord);
            buildGuitarSequencerNotes(assignedFretPatternsByPreset[preset][lastPressed].assignedGuitarChord, false);
          }
          else
          {
            detector.transposeDown();
          }
          //queue buttons to be played
      } else if (value == 0x000) {
        strum = 0; //neutral 
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
    if ((buffer[0] == '8' && buffer[1] == '0') ||
        (buffer[0] == '9' && buffer[1] == '0') ||
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
    if (bufferLen >= 6 && (strncmp(buffer, "80", 2) == 0 || strncmp(buffer, "90", 2) == 0)) 
    {
      char temp[3]; temp[2] = '\0';
      temp[0] = buffer[0]; temp[1] = buffer[1]; uint8_t status = strtol(temp, NULL, 16);
      temp[0] = buffer[2]; temp[1] = buffer[3]; uint8_t note   = strtol(temp, NULL, 16);
      temp[0] = buffer[4]; temp[1] = buffer[5]; uint8_t vel    = strtol(temp, NULL, 16);
      
      if (omniChordModeGuitar[preset] != OmniChordOffType && !isKeyboard)
      {
        //Serial.printf("omniChordNewNotes.size() %d\n", omniChordNewNotes.size());
        if (omniChordNewNotes.size() == 0) //no guitar button pressed
        {
          skipNote = true;
        }
        else
        {
          //Serial.printf("Original Note is %d\n", note);
          detector.noteOn(note);
          oldNote = note;
          //lastTransposeValueDetected = detector.getBestTranspose();
          //Serial.printf("lastTransposeValueDetected = %d\n", lastTransposeValueDetected);
          note = detector.getBestNote(note);
        }
      }
      if (!skipNote)
      {
        if ((status & 0xF0) == 0x90) 
        {
          sendNoteOn(channel, note+transpose, vel);
          struct noteShift ns;
          ns.assignedNote = note+transpose;
          ns.paddleNote = oldNote;
          lastOmniNotes.push_back(ns);
        }
        else
        {
          for (auto it = lastOmniNotes.begin(); it != lastOmniNotes.end(); ++it)
          {
              if (it->paddleNote == oldNote)
              {
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
    }
    else if (bufferLen >= 4 && strncmp(buffer, "f555", 4) == 0) 
    {
      bool matched = false;
      for (const HexToControl& msg : hexToControl) {
        size_t len = strlen(msg.hex);
        //Serial.printf("Raw buffer (KB): %s vs %s %d\n", buffer, msg.hex, len);
        if (bufferLen >= len && strncmp(buffer, msg.hex, len) == 0) {
          if (msg.cc != 0)
          {
            if (msg.cc == 3 || msg.cc == 4)
            {
              if (playMode == 1) //started
              {
                sendStop();
                playMode = 0; //paused/stop
              }
              else
              {
                sendContinue();
                playMode = 1;
              }
            }
            else if (msg.cc == 5)
            {
              transpose = 0;
              if (debug)
              {
                Serial.printf("Reset transpose to 0\n");              
              }
            }
            else
            {
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
    }
    else //clean up?
      {
        while (bufferLen >= 2) {
          if ((buffer[0] == '8' && buffer[1] == '0') ||
              (buffer[0] == '9' && buffer[1] == '0') ||
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
  for (auto it = SequencerNotes.begin(); it != SequencerNotes.end(); ) 
  {
    SequencerNote& note = *it; // Reference to current note

    if (note.offset == 1) {
      sendNoteOn(note.channel, note.note, note.velocity);
      note.offset = 0;
    } else if (note.offset > 1) 
    {
      note.offset--;
    }
    // count down to playing the note
    if (note.offset == 0 && note.holdTime != 65535 && note.holdTime > 0) 
    {
      note.holdTime--;
    }
    //note is done playing, delete the note
    if (note.offset == 0 && note.holdTime == 0) {
      sendNoteOff(note.channel, note.note);
      
      // Remove the note from the vector
      it = SequencerNotes.erase(it); // `erase` returns the next iterator
    } 
    else 
    {
      ++it; // Only increment if we didn’t erase the note
    }
  }
}


void onTick48() 
{
  tickCount++;

  // Send MIDI Clock every 1/24 note (i.e., every other 1/48 tick)
  if (midiClockEnable && tickCount % 2 == 0) {
    usbMIDI.sendRealTime(usbMIDI.Clock);
  }
  //if note queue is not empty
  if (SequencerNotes.size() > 0)
  {
    advanceSequencerStep(); // or send a note, etc.
  }
  
  // Optional: wrap counter
  if (tickCount >= 4800) tickCount = 0;
}
void noteAllOff()
{
  for (uint8_t n = 0; n < 127; n++) 
  {
    sendNoteOff(GUITAR_CHANNEL, n);
    sendNoteOff(KEYBOARD_CHANNEL, n);
    sendNoteOff(GUITAR_BUTTON_CHANNEL, n);
  }
  omniChordNewNotes.clear();
  lastOmniNotes.clear();
}

bool printFileContents(const char* filename, bool isSerialOut) {
  File f = SD.open(filename, FILE_READ);
  if (!f) 
  {
    if (!isSerialOut)
    {
      Serial.printf("Failed to open %s for reading\n", filename);
    }
    return false;
  }

  if (!isSerialOut)
  {
    Serial.printf("Contents of %s:\n", filename);
  }

  while (f.available()) {
    char c = f.read();       // Read one character
    if (isSerialOut)
    {
      Serial.write(c);  // Print using printf
    }
    else
    {
      Serial.printf("%c", c);  // Print using printf
    }
  }

  f.close();
  if (isSerialOut)
  {
    Serial.write("\n");  // Print using printf
  }
  else
  {
    Serial.println(); // Optional: newline after file contents
  }
  return true;
}

bool saveSimpleConfig()
{
  File f = SD.open("config.ini", FILE_WRITE);
  if (f) 
  {
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
    snprintf(buffer, sizeof(buffer), "stopSoundsOnPresetChange,%d\n", stopSoundsOnPresetChange?1:0);
    f.print(buffer);
    //bool midiClockEnable = true;
    snprintf(buffer, sizeof(buffer), "midiClockEnable,%d\n", midiClockEnable?1:0);
    f.print(buffer);
  
    for (uint8_t i = 0; i < MAX_PRESET; i++)
    {
      //std::vector<uint8_t> omniChordModeGuitar;
      snprintf(buffer, sizeof(buffer), "omniChordModeGuitar,%d,%d\n", i, omniChordModeGuitar[i]);
      f.print(buffer);

      //std::vector<bool> muteWhenLetGo;
      snprintf(buffer, sizeof(buffer), "muteWhenLetGo,%d,%d\n", i, muteWhenLetGo[i]?1:0);
      f.print(buffer);
      
      //std::vector<bool> ignoreSameChord;
      snprintf(buffer, sizeof(buffer), "ignoreSameChord,%d,%d\n", i, ignoreSameChord[i]?1:0);
      f.print(buffer);
      //std::vector<uint16_t> presetBPM;
      snprintf(buffer, sizeof(buffer), "presetBPM,%d,%d\n", i, presetBPM[i]);
      f.print(buffer);
      //std::vector<uint16_t> QUARTERNOTETICKS;
      snprintf(buffer, sizeof(buffer), "QUARTERNOTETICKS,%d,%d\n", i, QUARTERNOTETICKS[i]);
      f.print(buffer);
      //std::vector<uint8_t> TimeNumerator;
      snprintf(buffer, sizeof(buffer), "TimeNumerator,%d,%d\n", i, TimeNumerator[i]);
      f.print(buffer);
      //std::vector<uint8_t> TimeDenominator; // in terms of 1/48 notes
      snprintf(buffer, sizeof(buffer), "TimeDenominator,%d,%d\n", i, TimeDenominator[i]);
      f.print(buffer);
      //std::vector<uint16_t> MaxBeatsPerBar;
      snprintf(buffer, sizeof(buffer), "MaxBeatsPerBar,%d,%d\n", i, MaxBeatsPerBar[i]);
      f.print(buffer);
      //std::vector<bool> chordHold;
      snprintf(buffer, sizeof(buffer), "chordHold,%d,%d\n", i, chordHold[i]?1:0);
      f.print(buffer);
      //std::vector<uint8_t> simpleChordSetting;
      snprintf(buffer, sizeof(buffer), "simpleChordSetting,%d,%d\n", i, simpleChordSetting[i]?1:0);
      f.print(buffer);
      //std::vector<uint8_t> strumSeparation; //time unit separation between notes //default 1
      snprintf(buffer, sizeof(buffer), "strumSeparation,%d,%d\n", i, strumSeparation[i]);
      f.print(buffer);
    }
  }
  else
  {
    Serial.println("Error saving simple config!");
    return false;
  }
  f.close();
  return true;
}

bool saveComplexConfig()
{
  File f = SD.open("config2.ini", FILE_WRITE);
  if (f) 
  {
    f.seek(0);
    f.truncate();
    char buffer[50];
    //std::vector<std::vector<AssignedPattern>> assignedFretPatternsByPreset;
    //std::vector<std::vector<neckAssignment>> neckAssignments; //[preset][neck assignment 0-27]
    for (uint8_t k = 0; k < MAX_PRESET; k++)
    {
      for (uint8_t i = 0; i < NECK_ACTUALROWS; i++)
      {
        for (uint8_t j = 0; j < NECK_COLUMNS; j++)
        {
          snprintf(buffer, sizeof(buffer), "neckAssignmentsKey,%d,%d,%d,%d,%d\n", k, i, j, neckAssignments[k][i*NECK_COLUMNS+j].key, neckAssignments[k][i*NECK_COLUMNS+j].chordType);
          f.print(buffer);
          //seems this is derived from neckAssignments + simpleChordSetting + row, column, preset
          //simpleChord setting - not needed
          //AssignedPattern(simpleChordSetting[x], getKeyboardChordNotesFromNeck(i,j,x), getGuitarChordNotesFromNeck(i,j,x), rootNotes[i], false);
          //prepareChords() could be used for loading but must clear the data first
        }
      }
    }
  }
  else
  {
    Serial.println("Error saving complex config!");
    return false;
  }
  f.close();
  return true;
}
bool savePatternFiles(File f)
{
  //std::vector<PatternNote> SequencerPatternA;
  //std::vector<PatternNote> SequencerPatternB;
  //std::vector<PatternNote> SequencerPatternC;
  return true;
}

bool saveSettings()
{
  if (!saveSimpleConfig())
  {
    Serial.printf("Error loading Simple Config!\n");
    return false;
  }
  if (!saveComplexConfig())
  {
    
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
    line[len] = '\0'; // Null-terminate

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
    if (strcmp(key, "MAX_PRESET") == 0) 
    {
      readPreset = atoi(arg1);
      if (readPreset > MAX_PRESET)
      {
        Serial.printf("Error preset read is %d > MAX_PRESET\n", readPreset, MAX_PRESET);
        f.close();
        return false;
      }
    } 
    else if (strcmp(key, "CUR_PRESET") == 0) 
    {
      uint8_t newPreset = atoi(arg1);
      if (debug)
      {
        Serial.printf("new vs old preset is %d vs %d\n", newPreset, preset);
      }
      if (newPreset != preset)
      {
        presetChanged();
        preset = newPreset;
      }
      
    } 
    else if (strcmp(key, "stopSoundsOnPresetChange") == 0) 
    {
      stopSoundsOnPresetChange = atoi(arg1);
    } 
    else if (strcmp(key, "debug") == 0) 
    {
      debug = atoi(arg1);
    } 
    else if (strcmp(key, "midiClockEnable") == 0) 
    {
      midiClockEnable = atoi(arg1);
    } 
    else if (arg2) 
    {
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

      else if (strcmp(key, "QUARTERNOTETICKS") == 0 && i < QUARTERNOTETICKS.size())
        QUARTERNOTETICKS[i] = val;

      else if (strcmp(key, "TimeNumerator") == 0 && i < TimeNumerator.size())
        TimeNumerator[i] = val;

      else if (strcmp(key, "TimeDenominator") == 0 && i < TimeDenominator.size())
        TimeDenominator[i] = val;

      else if (strcmp(key, "MaxBeatsPerBar") == 0 && i < MaxBeatsPerBar.size())
        MaxBeatsPerBar[i] = val;

      else if (strcmp(key, "chordHold") == 0 && i < chordHold.size())
        chordHold[i] = val;

      else if (strcmp(key, "simpleChordSetting") == 0 && i < simpleChordSetting.size())
        simpleChordSetting[i] = val;

      else if (strcmp(key, "strumSeparation") == 0 && i < strumSeparation.size())
        strumSeparation[i] = val;
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
      continue; // invalid line
    }

    if (strcmp(key, "neckAssignmentsKey") == 0) {
      uint8_t presetT = atoi(arg0);
      uint8_t row = atoi(arg1);
      uint8_t col = atoi(arg2);
      uint8_t keyVal = atoi(arg3);
      uint8_t chordTypeVal = atoi(arg4);

      if (presetT < neckAssignments.size() && 
          (row * NECK_COLUMNS + col) < (int) neckAssignments[presetT].size()) {
        neckAssignments[presetT][row * NECK_COLUMNS + col].key = (Note) keyVal;
        neckAssignments[presetT][row * NECK_COLUMNS + col].chordType = (ChordType) chordTypeVal;
      }
    }
  }

  f.close();
  return true;
}

bool loadPatternFiles()
{
  //std::vector<PatternNote> SequencerPatternA;
  //std::vector<PatternNote> SequencerPatternB;
  //std::vector<PatternNote> SequencerPatternC;
  return true;
}

bool loadSettings()
{
  File file = SD.open("config.ini", FILE_READ);
  if (file) 
  {
    file.seek(0);
    file.truncate();
    //file.println("your new config data");
    //snprintf(buffer, sizeof(buffer), "Name: %s, Int: %d, Float: %.2f\n", name, someValue, anotherValue);
    //file.print(buffer);
    if (!loadSimpleConfig())
    {
      file.close();
      Serial.printf("Error loading Simple Config!\n");
      return false;
    }
    if (!loadComplexConfig())
    {
      file.close();
      Serial.printf("Error loading Complex Config!\n");
      return false;
    }
    file.close();
  }
  else
  {
    return false;
  } 
  if (debug)
  {
    Serial.println("Loading config is OK!");
  }
  return true;
}

void loop() {
    if (sendClockTick) 
    {
      sendClockTick = false;
      onTick48();
    }
  
  checkSerialGuitar(Serial1, dataBuffer1, bufferLen1, GUITAR_CHANNEL);
  checkSerialKB(Serial2, dataBuffer2, bufferLen2, KEYBOARD_CHANNEL);
  checkSerialBT(Serial3, dataBuffer3, bufferLen3);
  //checkSerialG2Piano(Serial4, dataBuffer4, bufferLen4, 3);
  checkSerialCmd(Serial, dataBuffer4, bufferLen4);
  bool noteOffState = digitalRead(NOTE_OFF_PIN);
  if (noteOffState != prevNoteOffState && noteOffState == LOW) {
    if (debug)
    {
      Serial.println("NOTE_OFF_PIN pressed → All notes off");
    }
    noteAllOff();
  }
  prevNoteOffState = noteOffState;

  bool ccState = digitalRead(START_TRIGGER_PIN);
  if (ccState != prevCCState && ccState == LOW) {
    if (playMode == 1) //started
    {
      if (debug)
      {
        Serial.println("START_TRIGGER_PIN pressed → Send Stop");
      }
      sendStop();
      playMode = 0;
    }
    else
    {
      if (debug)
      {
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
  bool button2State = digitalRead(BUTTON_2_PIN);
  if (b2Ignored == false)
  {
    prevButton2State = digitalRead(BUTTON_2_PIN);
    b2Ignored = true;
  }
  if (button2State != prevButton2State && button2State == LOW) {
    if (debug)
    {
      Serial.println("Button 2 Pressed");
    }
    presetChanged();
    preset +=1;
    if (preset >= MAX_PRESET)
    {
      preset = 0;
    }
    
    //add preset handling here
    if (debug)
    {
      Serial.printf("Preset is now %d\n", preset);
    }
  }
  prevButton2State = button2State;
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

  uintptr_t heap_end = (uintptr_t)sbrk(0);     // Heap grows upward
  uintptr_t stack_ptr = (uintptr_t)&stack_dummy; // Stack grows downward

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
