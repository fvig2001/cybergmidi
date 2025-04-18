#include <usb_midi.h>
#include <vector>
extern "C" char* sbrk(int incr); // Get current heap end

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
    uint8_t getRootNote() const {
      return rootNote;
    }
    void setRootNote(uint8_t newNote)
    {
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

private:
    std::vector<uint8_t> notes;  // A vector of intervals relative to the root note
};

class AssignedPattern
{
  
  public:
    AssignedPattern(bool isBasic, Chord toAssign, uint8_t rootNote, bool ignored)
    {
      isSimple = isBasic;
      assignedChord = toAssign;
      assignedChord.setRootNote(rootNote);
      isIgnored = ignored;
    }
    bool isIgnored;
    bool isSimple;
    Chord assignedChord;
    Chord getChords() const
    {
      return assignedChord;
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


//#define USE_AND
// --- PIN DEFINITIONS ---
const int NOTE_OFF_PIN = 10;     // Digital input for turning off all notes
const int START_TRIGGER_PIN = 11;   // Digital input for triggering CC message

const int BUTTON_1_PIN = 4;
const int BUTTON_2_PIN = 5;
const int BUTTON_3_PIN = 6;

bool prevButton1State = HIGH;
bool prevButton2State = HIGH;
bool prevButton3State = HIGH;

// --- MIDI CONSTANTS ---
const int GUITAR_CHANNEL = 2;
const int KEYBOARD_CHANNEL = 1;

// --- STATE TRACKING ---
bool prevNoteOffState = LOW;
bool prevCCState = LOW;
std::vector<uint8_t> lastGuitarNotes;
#define MAX_BUFFER_SIZE 150
uint8_t bufferLen2 = 0;
char dataBuffer1[MAX_BUFFER_SIZE + 1];  // +1 for null terminator
char dataBuffer2[MAX_BUFFER_SIZE + 1];  // +1 for null terminator
uint8_t bufferLen1 = 0;
bool lastSimple = false;

// --- HEX TO MIDI NOTE MAP ---
struct MidiMessage {
  const char* hex;
  uint8_t note;
};
int transpose = 0;
int preset = 0;
int curProgram = 0;
int playMode = 0;
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
    Chord add6Chord({4, 7, 9});
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
        add6Chord,
        ninthChord,
        eleventhChord,
        thirteenthChord
    };
std::vector<AssignedPattern> assignedFretPatterns;
void prepareChords()
{
  uint8_t rootNotes[] = {48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72, 74, 75, 77, 79, 81, 83, 84};

    // Populate the assignedFretPatterns with the chords for each root note
    for (int i = 0; i < 12; ++i) {
        assignedFretPatterns.push_back(AssignedPattern(true, majorChord, rootNotes[i], false));
        assignedFretPatterns.push_back(AssignedPattern(true, minorChord, rootNotes[i], false));
        assignedFretPatterns.push_back(AssignedPattern(true, dominant7thChord, rootNotes[i], false));
    }
    //other ignored
    for (int i = 0; i < 2; ++i) 
    {
        assignedFretPatterns.push_back(AssignedPattern(true, majorChord, rootNotes[i], true));
        assignedFretPatterns.push_back(AssignedPattern(true, minorChord, rootNotes[i], true));
        assignedFretPatterns.push_back(AssignedPattern(true, dominant7thChord, rootNotes[i], true));
    }
    for (const AssignedPattern& pattern : assignedFretPatterns) 
    {
      Serial.print("Pattern Simple: ");
      Serial.print(pattern.getSimple() ? "Yes" : "No");
      Serial.print(", Root Note: ");
      Serial.print(pattern.assignedChord.getRootNote());
      Serial.print(" Chord Notes: ");
      for (uint8_t note : pattern.assignedChord.getChordNotes()) 
      {
          Serial.print(note);
          Serial.print(" ");
      }
      Serial.println();
    }
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

    // Add 6 - [4, 7, 9]

    add6Chord.printChordInfo();  // C Add6 chord (relative intervals)

    // 9th (9) - [4, 7, 10, 14]

    ninthChord.printChordInfo();  // C 9th chord (relative intervals)

    // 11th (11) - [4, 7, 10, 14, 17]

    eleventhChord.printChordInfo();  // C 11th chord (relative intervals)

    // 13th (13) - [4, 7, 10, 14, 17, 21]

    thirteenthChord.printChordInfo();  // C 13th chord (relative intervals)

}

void setup() {
  pinMode(NOTE_OFF_PIN, INPUT_PULLUP);
  pinMode(START_TRIGGER_PIN, INPUT_PULLUP);
  pinMode(BUTTON_1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_2_PIN, INPUT_PULLUP);
  pinMode(BUTTON_3_PIN, INPUT_PULLUP);
  Serial.println("HM-10 Bluetooth Initialized");
  Serial1.begin(250000); // Guitar
  Serial2.begin(250000); // Keyboard
    //HM10_BLUETOOTH.begin(HM10_BAUD); //BT
  Serial.begin(115200);  // USB debug monitor

  usbMIDI.begin();
  while (!Serial && millis() < 3000);  // Wait for Serial Monitor
  Serial.println("Teensy MIDI Debug Start");
  printChords();  
  prepareChords();
}

void sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity = 127) {
  usbMIDI.sendNoteOn(note+transpose, velocity, channel);
  Serial.printf("Note ON: ch=%d note=%d vel=%d\n", channel, note, velocity);
}

void sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity = 0) {
  usbMIDI.sendNoteOff(note+transpose, velocity, channel);
  Serial.printf("Note OFF: ch=%d note=%d vel=%d\n", channel, note, velocity);
}
void sendStart() 
{
  usbMIDI.sendRealTime(0xFA);
  Serial.printf("Start!\n");
}

void sendStop() 
{
  usbMIDI.sendRealTime(0xFC);
  Serial.printf("Stop\n");
}

void sendContinue() 
{
  usbMIDI.sendRealTime(0xFB);
  Serial.printf("Continue\n");
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
  Serial.printf("Program: ch=%d program=%d\n", channel, curProgram);
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
  Serial.printf("Transpose: ch=%d transpose=%d\n", channel, transpose);
}

void trimBuffer(String& buffer) {
  if (buffer.length() > MAX_BUFFER_SIZE) {
    buffer.remove(0, buffer.length() - MAX_BUFFER_SIZE);
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

  // --- Step 5: Main parser loop ---
  while (true) 
  {
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
#ifdef USE_AND              
      for (const MidiMessage& msg : hexToNote) 
      {
        size_t len = strlen(msg.hex);  // length in hex characters
        last_len = len;

        if (bufferLen >= len && hexStringAndMatches(buffer, msg.hex, len)) 
        {
          if (msg.note != 255) 
          {
            Serial.printf("Get simple %d\n",assignedFretPatterns[msg.note].getSimple())
            if (assignedFretPatterns[msg.note].getSimple())
            {
              lastSimple = true;
              rootNote = assignedFretPatterns[msg.note].getRootNote();
              Serial.printf("Root note is %d\n", rootNote);
              sendNoteOn(channel, rootNote);
              lastGuitarNotes.push_back(rootNote);
              for (uint8_t note : assignedFretPatterns[msg.note].getChords().getChordNotes()) 
              {
                sendNoteOn(channel, rootNote + note);
                lastGuitarNotes.push_back(rootNote+note);
              }
            }
            else
            {
              lastSimple = false;
              Serial.printf("Not simple!");
            }
             
          }
            
          matched = true;
          processed = true;
        } 
        else 
        {
          // Clear all currently active guitar notes
          while (!lastGuitarNotes.empty()) {
            sendNoteOff(channel, lastGuitarNotes.back());
            lastGuitarNotes.pop_back();
            matched = true;
            processed = true;
            break;
          }
        }
      }
    }

    // Move buffer forward only if we matched something
    if (matched) {
      memmove(buffer, buffer + last_len, bufferLen - last_len + 1);
      bufferLen -= last_len;
    }
#else
    for (const MidiMessage& msg : hexToNote) {
      size_t len = strlen(msg.hex);
      last_len = len;
      if (bufferLen >= len && strncmp(buffer, msg.hex, len) == 0) 
      {

        if (msg.note != 255) 
        {
          Serial.printf("Get simple %d\n",assignedFretPatterns[msg.note].getSimple());
          if (assignedFretPatterns[msg.note].getSimple())
          {
            lastSimple = true;
            rootNote = assignedFretPatterns[msg.note].getChords().getRootNote();
            Serial.printf("Root note is %d\n", rootNote);
            sendNoteOn(channel, rootNote);
            lastGuitarNotes.push_back(rootNote);
            for (uint8_t note : assignedFretPatterns[msg.note].getChords().getChordNotes()) 
            {
              sendNoteOn(channel, rootNote + note);
              lastGuitarNotes.push_back(rootNote+note);
            }
          }
          else
          {
            lastSimple = false;
            Serial.printf("Not simple!");
          }
            
        }
        else
        {
          while (lastGuitarNotes.size() > 0)
          {
            sendNoteOff(channel, lastGuitarNotes.back());
            lastGuitarNotes.pop_back();
          }
        }
        memmove(buffer, buffer + len, bufferLen - len + 1);
        bufferLen -= len;
        matched = true;
        processed = true; //did something
        break;
      }
    }
#endif
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

bool ignoringIdlePing = false;

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
    memmove(buffer, buffer + 12, bufferLen - 11);
    bufferLen -= 12;
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
  while (true) {
    bool processed = false;

    if (bufferLen >= 6 && (strncmp(buffer, "80", 2) == 0 || strncmp(buffer, "90", 2) == 0)) 
    {
      char temp[3]; temp[2] = '\0';
      temp[0] = buffer[0]; temp[1] = buffer[1]; uint8_t status = strtol(temp, NULL, 16);
      temp[0] = buffer[2]; temp[1] = buffer[3]; uint8_t note   = strtol(temp, NULL, 16);
      temp[0] = buffer[4]; temp[1] = buffer[5]; uint8_t vel    = strtol(temp, NULL, 16);

      if ((status & 0xF0) == 0x90) sendNoteOn(channel, note, vel);
      else                         sendNoteOff(channel, note, vel);

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
              Serial.printf("Reset transpose to 0\n");              
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



void loop() {
  checkSerialGuitar(Serial1, dataBuffer1, bufferLen1, GUITAR_CHANNEL);
  checkSerialKB(Serial2, dataBuffer2, bufferLen2, KEYBOARD_CHANNEL);

  bool noteOffState = digitalRead(NOTE_OFF_PIN);
  if (noteOffState != prevNoteOffState && noteOffState == LOW) {
    Serial.println("NOTE_OFF_PIN pressed → All notes off");
    for (int n = 0; n < 127; n++) {
      sendNoteOff(GUITAR_CHANNEL, n);
      sendNoteOff(KEYBOARD_CHANNEL, n);
    }
  }
  prevNoteOffState = noteOffState;

  bool ccState = digitalRead(START_TRIGGER_PIN);
  if (ccState != prevCCState && ccState == LOW) {
    if (playMode == 1) //started
    {
      Serial.println("START_TRIGGER_PIN pressed → Send Stop");
      sendStop();
      playMode = 0;
    }
    else
    {
      Serial.println("START_TRIGGER_PIN pressed → Send Start");
      sendStart();
      playMode = 1;
    }

  }
  prevCCState = ccState;

 // --- Button Handling ---
  bool button1State = digitalRead(BUTTON_1_PIN);
  if (button1State != prevButton1State && button1State == LOW) {
    Serial.println("Button 1 Pressed");
    preset = 0;

  }
  prevButton1State = button1State;

  bool button2State = digitalRead(BUTTON_2_PIN);
  if (button2State != prevButton2State && button2State == LOW) {
    Serial.println("Button 2 Pressed");
    preset +=1;
    if (preset >= 3)
    {
      preset = 0;
    }
    //add preset handling here
    Serial.printf("Preset is now %d\n", preset);
  }
  prevButton2State = button2State;

  bool button3State = digitalRead(BUTTON_3_PIN);
  if (button3State != prevButton3State && button3State == LOW) {
    Serial.println("Button 3 Pressed");
    preset = 2;
  }
  prevButton3State = button3State;
    // --- HM10 Bluetooth handling ---
  // while (HM10_BLUETOOTH.available()) {
  //   char c = HM10_BLUETOOTH.read();
  //   Serial.print("[BT] ");
  //   Serial.println(c); // You can customize this later (e.g., trigger MIDI)
  // }
  usbMIDI.read();
  delayMicroseconds(10); 
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
