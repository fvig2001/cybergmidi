#define MAX_VELOCITY 127

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

struct noteShift {
  uint8_t paddleNote;
  uint8_t assignedNote;
};

// --- HEX TO MIDI NOTE MAP ---
struct MidiMessage {
  const char* hex;
  uint8_t note;
};

struct HexToProgram {
  const char* hex;
  uint8_t program;
};

struct HexToControl {
  const char* hex;
  uint8_t cc;
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