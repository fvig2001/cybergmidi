#pragma once
#include <vector>
#include <deque>
#define MAX_PRESET 6
#define MAX_VELOCITY 127
#define SEMITONESPEROCTAVE 12

// --- MIDI CONSTANTS ---
#define ACCOMPANIMENT_CHANNEL 5 // backing for omnichord tracks
#define BASS_CHANNEL 4
#define GUITAR_BUTTON_CHANNEL 3
#define GUITAR_CHANNEL 2
#define KEYBOARD_CHANNEL 1
#define DRUM_CHANNEL 10

#define OMNICHORD_OCTAVE_RANGE 8
#define OMNICHORD_ROOT_START 48
extern uint8_t preset;
extern std::vector<int8_t> guitarTranspose;
extern bool debug;
extern bool debug2;
void DebugPrintf(const char* format, ...) {
  char buffer[128];  // Adjust size as needed
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  String message(buffer);
  if (debug2)
  {
    String msg = "DB00," + message + "\r\n";
    Serial.write(msg.c_str());
  }    

}


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
  thirteenthChordType,

  // ✅ Useful to Add:
  majorAdd9ChordType,
  dominant7b9ChordType,
  dominant7sharp9ChordType,
  dominant7b5ChordType,
  dominant7sharp5ChordType,
  minorMajor7ChordType,
  major7b5ChordType,
  dim7ChordType,
  powerChordType,

  //optional/advanced
  dominant13b9ChordType,
  dominant7sus4ChordType,
  quartalChordType,
  clusterChordType,

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

const std::vector<uint8_t> omniChordOrigNotes = { 65, 67, 69, 71, 72, 74, 76, 77, 79, 81, 83, 84 };
std::vector<uint8_t> omniChordNewNotes;
#define LOWEST_MIDI_OCTAVE_NOTE_START 29
#define LOWEST_MIDI_OCTAVE_NOTE_END 48
#define HIGHEST_MIDI_OCTAVE_NOTE_START 77
#define HIGHEST_MIDI_OCTAVE_NOTE_END 96

class TranspositionDetector {

  uint8_t lowest = omniChordOrigNotes[0];
  uint8_t highest = omniChordOrigNotes[omniChordOrigNotes.size() - 1];
  int8_t offset = 0;
  uint8_t prevBestGroup = 3;  // default to center (offset 0)
  std::deque<uint8_t> noteHistory;
  
public:
  int octaveShift = 0;
  bool groupFindCount(uint8_t midiNote, uint8_t groupNum)
  {
    switch (groupNum) 
    {
      case 0:
        if (midiNote == 17 || midiNote == 19 || midiNote == 21 || midiNote == 23 ||
            midiNote == 24 || midiNote == 26 || midiNote == 28 || midiNote == 29 ||
            midiNote == 31 || midiNote == 33 || midiNote == 35 || midiNote == 36) {
          return true;
        }
        break;
      case 1:
        if (midiNote == 29 || midiNote == 31 || midiNote == 33 || midiNote == 35 ||
            midiNote == 36 || midiNote == 38 || midiNote == 40 || midiNote == 41 ||
            midiNote == 43 || midiNote == 45 || midiNote == 47 || midiNote == 48) {
          return true;
        }
        break;
      case 2:
        if (midiNote == 41 || midiNote == 43 || midiNote == 45 || midiNote == 47 ||
            midiNote == 48 || midiNote == 50 || midiNote == 52 || midiNote == 53 ||
            midiNote == 55 || midiNote == 57 || midiNote == 59 || midiNote == 60) {
          return true;
        }
        break;
      case 3:
        if (midiNote == 53 || midiNote == 55 || midiNote == 57 || midiNote == 59 ||
            midiNote == 60 || midiNote == 62 || midiNote == 64 || midiNote == 65 ||
            midiNote == 67 || midiNote == 69 || midiNote == 71 || midiNote == 72) {
          return true;
        }
        break;
      case 4:
        if (midiNote == 65 || midiNote == 67 || midiNote == 69 || midiNote == 71 ||
            midiNote == 72 || midiNote == 74 || midiNote == 76 || midiNote == 77 ||
            midiNote == 79 || midiNote == 81 || midiNote == 83 || midiNote == 84) {
          return true;
        }
        break;
      case 5:
        if (midiNote == 77 || midiNote == 79 || midiNote == 81 || midiNote == 83 ||
            midiNote == 84 || midiNote == 86 || midiNote == 88 || midiNote == 89 ||
            midiNote == 91 || midiNote == 93 || midiNote == 95 || midiNote == 96) {
          return true;
        }
        break;
      case 6:
        if (midiNote == 89 || midiNote == 91 || midiNote == 93 || midiNote == 95 ||
            midiNote == 96 || midiNote == 98 || midiNote ==100 || midiNote ==101 ||
            midiNote ==103 || midiNote ==105 || midiNote ==107 || midiNote ==108) {
          return true;
        }
        break;
      case 7:
        if (midiNote ==101 || midiNote ==103 || midiNote ==105 || midiNote ==107 ||
            midiNote ==108 || midiNote ==110 || midiNote ==112 || midiNote ==113 ||
            midiNote ==115 || midiNote ==117 || midiNote ==119 || midiNote ==120) {
          return true;
        }
      break;
    }
    return false;
  }
  void findGroup (uint8_t midiNote, std::vector<uint8_t> &matchCounts)
  {
    for (uint8_t n : noteHistory) {
      for (uint8_t i = 0; i < OMNICHORD_OCTAVE_RANGE; i++) {
        matchCounts[i] += groupFindCount(n, i)?1:0 ;
      }
    }
  }
  
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

  void noteOn(uint8_t note) {
    noteHistory.push_back(note);
    if (noteHistory.size() > SEMITONESPEROCTAVE) {
      noteHistory.pop_front();
    }

    std::vector<uint8_t> matchCounts(OMNICHORD_OCTAVE_RANGE, 0);
    findGroup(note, matchCounts);

    // Find best matching group(s)
    uint8_t bestGroup = prevBestGroup;
    uint8_t maxMatches = 0;

    for (uint8_t i = 0; i < matchCounts.size(); ++i) {
      if (matchCounts[i] > maxMatches) {
        maxMatches = matchCounts[i];
        bestGroup = i;
      } else if (matchCounts[i] == maxMatches && i == prevBestGroup) {
        // Tie breaker: prefer the previous best group
        bestGroup = i;
      }
    }

    // Optional: require minimum number of matches before changing
    if (matchCounts[bestGroup] < 3) {
      bestGroup = prevBestGroup;  // Don't change if not stable
    }

    offset = bestGroup - 4;
    prevBestGroup = bestGroup;

    //Serial.printf("Detected offset: %d, group: %d\n", offset, bestGroup);
  }

  uint8_t getBestNote(uint8_t note) {
    //given a transpose
    //uint8_t newNote = note - transpose;
    uint8_t newNote = note - offset * SEMITONESPEROCTAVE;
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
  std::vector<uint8_t> getCompleteChordNotes(bool useTranspose = false) const {
    std::vector<uint8_t> complete;
    int offset = 0;
    if (useTranspose)
    {
      offset = guitarTranspose[preset];
    }
    //uint8_t offset = 0;
    complete.push_back(rootNote + offset);
    
    for (uint8_t i = 0; i < notes.size(); i++) {
      if (notes[i] != 0) //skip power note repeated note
      {
        complete.push_back(notes[i] + rootNote + offset);
      }
    }
    return complete;
  }

//get only 3 notes
std::vector<uint8_t> getCompleteChordNotes3(bool useTranspose = false) const {
    std::vector<uint8_t> complete;
    int offset = 0;
    if (useTranspose)
    {
      offset = guitarTranspose[preset];
    }
    complete.push_back(rootNote + offset);  // Always include root

    int third = -1;
    int color = -1;
    
    // Pass 1: find the 3rd (either major 3rd = 4 or minor 3rd = 3)
    for (uint8_t interval : notes) {
        if (interval == 3 || interval == 4) {
            third = interval;
            break;
        }
    }

    // Pass 2: find the next most important color tone
    // Priority: 7 > 9 > 13 > altered 5ths > 11 > add tones
    for (uint8_t interval : notes) {
        if (interval == 10 || interval == 11) {  // minor or major 7th
            color = interval;
            break;
        }
    }
    if (color == -1) {
        for (uint8_t interval : notes) {
            if (interval == 14 || interval == 13) {  // 9th
                color = interval;
                break;
            }
        }
    }
    if (color == -1) {
        for (uint8_t interval : notes) {
            if (interval == 21 || interval == 20) {  // 13th
                color = interval;
                break;
            }
        }
    }
    if (color == -1) {
        for (uint8_t interval : notes) {
            if (interval == 8 || interval == 6) {  // ♯5 or ♭5
                color = interval;
                break;
            }
        }
    }
    if (color == -1) {
        for (uint8_t interval : notes) {
            if (interval == 5 || interval == 2) {  // 11 or add9
                color = interval;
                break;
            }
        }
    }

    // Add chosen tones if found
    if (third != -1) {
        complete.push_back((rootNote + third + offset) % 128);
    }

    if (color != -1 && color != third) {
        complete.push_back((rootNote + color + offset) % 128);
    }
    
    bool fixed = false;

    //fix for power chord
    if (complete.size() == 1 && notes.size() == 2)
    {
      fixed = true;
      complete.push_back(rootNote + notes[1] + offset);
    }
    //if notes is exactly 3, use the original
    else if (complete.size() < 3 && notes.size() == 2)
    {
      fixed = true;
      complete.push_back(rootNote + notes[1] + offset);
      complete[1] = rootNote + notes[0] + offset;
    }
    if (!fixed && complete.size() < 3)
    {
      // First, try to add any remaining *non-5th* intervals
      for (uint8_t interval : notes) {
          if (interval == 7) continue;  // Skip perfect 5th for now

          uint8_t absNote = (rootNote + interval + offset) % 128;
          if (std::find(complete.begin(), complete.end(), absNote) == complete.end()) {
              complete.push_back(absNote);
              if (complete.size() >= 3) break;
          }
      }

      // If still not enough, then and only then add the 5th
      if (complete.size() < 3) 
      {
        for (uint8_t interval : notes) 
        {
          if (interval == 7) 
          {
            uint8_t absNote = (rootNote + interval + offset) % 128;
            if (std::find(complete.begin(), complete.end(), absNote) == complete.end()) {
              complete.push_back(absNote);
              break;
            }
          }
        }
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

class SequencerNote {
public:
  uint16_t holdTime;
  uint16_t offset;
  int8_t relativeOctave;
  uint8_t note;
  uint8_t channel;
  uint8_t velocity;
  SequencerNote()
  {
    relativeOctave = 0;
    note = 0;
    holdTime = 0;
    offset = 0;
    channel = -1;
    velocity = MAX_VELOCITY;
  }
  SequencerNote(uint8_t newNote, int newholdTime, uint16_t newoffset, uint8_t newchannel, uint8_t newvelocity = MAX_VELOCITY, int8_t oct = 0) {
    note = newNote;
    holdTime = newholdTime;
    offset = newoffset;
    channel = newchannel;
    velocity = newvelocity;
    relativeOctave = oct;
  }
};


class AssignedPattern 
{

public:
  AssignedPattern()
  {
    //assignedChord = 0;
    assignedChord.setRootNote(0);
    
    customPattern = 0;
  };
  AssignedPattern(uint8_t strumStyle, Chord toAssign, std::vector<uint8_t> newGuitarChord, uint8_t rootNote, uint8_t pattern) {
    //patternAssigned = 0;
    assignedChord = toAssign;
    assignedChord.setRootNote(rootNote);
    assignedGuitarChord = newGuitarChord;
    customPattern = pattern;  //actual auto/manual pattern for neck press
  }
  int customPattern; //strum pattern
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
};

typedef struct _noteShift {
  uint8_t paddleNote;
  uint8_t assignedNote;
} noteShift;

// --- HEX TO MIDI NOTE MAP ---
typedef struct _MidiMessage {
  const char* hex;
  uint8_t note;
} MidiMessage;

typedef struct _HexToProgram {
  const char* hex;
  uint8_t program;
} HexToProgram;

typedef struct _HexToControl {
  const char* hex;
  uint8_t cc;
} HexToControl;

class neckAssignment {
public:
  Note key;
  ChordType chordType;
  neckAssignment() {
    key = NO_NOTE;
    chordType = majorChordType;
  }
};

typedef struct _noteOffset 
{
  std::vector<int8_t> noteOffsets; //offsets relative to expected note
} noteOffset;

bool isHexStringEqualToBytes(const char* str, size_t strLen, const char* numValue, size_t numValueLen) {
  // Each byte should be represented by 2 hex characters
  if (strLen != numValueLen * 2) {
    return false;
  }

  for (size_t i = 0; i < numValueLen; ++i) {
    char hex[3] = { str[i * 2], str[i * 2 + 1], '\0' };  // 2 hex chars + null terminator
    char* end;
    long parsedByte = strtol(hex, &end, 16);  // convert hex string to integer

    if (*end != '\0' || parsedByte < 0 || parsedByte > 255) {
      return false;  // invalid hex or out of range
    }

    if ((uint8_t)parsedByte != numValue[i]) {
      return false;
    }
  }

  return true;
}