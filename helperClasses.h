#pragma once
#include <vector>
#include <deque>
#define MAX_VELOCITY 127
#define SEMITONESPEROCTAVE 12

// --- MIDI CONSTANTS ---
#define ACCOMPANIMENT_CHANNEL 5 // backing for omnichord tracks
#define BASS_CHANNEL 4
#define GUITAR_BUTTON_CHANNEL 3
#define GUITAR_CHANNEL 2
#define KEYBOARD_CHANNEL 1
#define DRUM_CHANNEL 10

extern bool debug;

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

const std::vector<uint8_t> omniChordOrigNotes = { 65, 67, 69, 71, 72, 74, 76, 77, 79, 81, 83, 84 };
std::vector<uint8_t> omniChordNewNotes;
#define LOWEST_MIDI_OCTAVE_NOTE_START 29
#define LOWEST_MIDI_OCTAVE_NOTE_END 48
#define HIGHEST_MIDI_OCTAVE_NOTE_START 77
#define HIGHEST_MIDI_OCTAVE_NOTE_END 96

const std::vector<std::vector<uint8_t>> midiOctaveGroups = {
  {17, 19, 21, 23, 24, 26, 28, 29, 31, 33, 35, 36}, // -4
  {29, 31, 33, 35, 36, 38, 40, 41, 43, 45, 47, 48}, // -3
  {41, 43, 45, 47, 48, 50, 52, 53, 55, 57, 59, 60}, // -2
  {53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72}, // -1
  {65, 67, 69, 71, 72, 74, 76, 77, 79, 81, 83, 84}, //  0
  {77, 79, 81, 83, 84, 86, 88, 89, 91, 93, 95, 96},  // +1
  {89, 91, 93, 95, 96, 98, 100, 101, 103, 105, 107, 108}, // +2
  {101, 103, 105, 107, 108, 110, 112, 113, 115, 117, 119, 120} //+3

};

class TranspositionDetector {

  uint8_t lowest = omniChordOrigNotes[0];
  uint8_t highest = omniChordOrigNotes[omniChordOrigNotes.size() - 1];
  int8_t offset = 0;
  uint8_t prevBestGroup = 3;  // default to center (offset 0)
  std::deque<uint8_t> noteHistory;
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

  void noteOn(uint8_t note) {
    Serial.printf("Note is %d\n", note);
    noteHistory.push_back(note);
    if (noteHistory.size() > SEMITONESPEROCTAVE) {
      noteHistory.pop_front();
    }

    std::vector<uint8_t> matchCounts(midiOctaveGroups.size(), 0);

    for (uint8_t n : noteHistory) {
      for (uint8_t i = 0; i < midiOctaveGroups.size(); i++) {
        if (std::find(midiOctaveGroups[i].begin(), midiOctaveGroups[i].end(), n) != midiOctaveGroups[i].end()) {
          matchCounts[i]++;
        }
      }
    }

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

    Serial.printf("Detected offset: %d, group: %d\n", offset, bestGroup);
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
  std::vector<uint8_t> getCompleteChordNotes() const {
    std::vector<uint8_t> complete;
    uint8_t offset = 0;
    complete.push_back(rootNote);
    for (uint8_t i = 0; i < notes.size(); i++) {
      complete.push_back(notes[i] + rootNote - offset);
    }
    return complete;
  }
  //skips 5th and 11th as needed
  std::vector<uint8_t> getCompleteChordNotesNo5No11() const {
    std::vector<uint8_t> complete;
    complete.push_back(rootNote);
    bool skip;
    for (uint8_t i = 0; i < notes.size(); i++) {
      skip = false;
      if (notes.size() > 2 && i == 1 && notes[i] == 7) //do not add 5th which is on 1
      {
        skip = true;
      }
      //else if (notes.size() > 3 && i != notes.size() - 1) //assume 3rd and last not only
      //{
        //skip = true; //only get last
      //}
      else if (notes.size() >= 6 && notes[i] == 17 ) // skip 11th on 13th
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
  //get only 3 notes
  std::vector<uint8_t> getCompleteChordNotesNo5() const {
    std::vector<uint8_t> complete;
    complete.push_back(rootNote);
    bool skip;
    for (uint8_t i = 0; i < notes.size(); i++) {
      skip = false;
      if (notes.size() > 2 && i == 1 && notes[i] == 7) //do not add 5th which is on 1
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
    
    isIgnored = false;
    customPattern = 0;
  };
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
