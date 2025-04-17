#include <usb_midi.h>
#include <vector>
//#define USE_AND
// --- PIN DEFINITIONS ---
const int NOTE_OFF_PIN = 10;     // Digital input for turning off all notes
const int CC_TRIGGER_PIN = 11;   // Digital input for triggering CC message

// --- MIDI CONSTANTS ---
const int GUITAR_CHANNEL = 2;
const int KEYBOARD_CHANNEL = 1;

// --- STATE TRACKING ---
bool prevNoteOffState = HIGH;
bool prevCCState = HIGH;
std::vector<uint8_t> lastGuitarNotes;
#define MAX_BUFFER_SIZE 150
uint8_t bufferLen2 = 0;
char dataBuffer1[MAX_BUFFER_SIZE + 1];  // +1 for null terminator
char dataBuffer2[MAX_BUFFER_SIZE + 1];  // +1 for null terminator
uint8_t bufferLen1 = 0;

// --- HEX TO MIDI NOTE MAP ---
struct MidiMessage {
  const char* hex;
  uint8_t note;
};

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
  {"aa55000a22010000000000", 0}, //note off
  {"aa55000a22010000000400", 24},  // C1
  {"aa55000a22010000000200", 36},  // C2
  {"aa55000a22010000000100", 48},  // C3
  {"aa55000a22010000002000", 26},  // D1
  {"aa55000a22010000001000", 38},  // D2
  {"aa55000a22010000000800", 50},  // D3
  {"aa55000a22010000010000", 28},  // E1
  {"aa55000a22010000008000", 40},  // E2
  {"aa55000a22010000004000", 52},  // E3
  {"aa55000a22010000080000", 29},  // F1
  {"aa55000a22010000040000", 41},  // F2
  {"aa55000a22010000020000", 53},  // F3
  {"aa55000a22010000400000", 31},  // G1
  {"aa55000a22010000200000", 43},  // G2
  {"aa55000a22010000100000", 55},  // G3
  {"aa55000a22010002000000", 33},  // A1
  {"aa55000a22010001000000", 45},  // A2
  {"aa55000a22010000800000", 57},  // A3
  {"aa55000a22010010000000", 35},  // B1
  {"aa55000a22010008000000", 47},  // B2
  {"aa55000a22010004000000", 59},  // B3
  {"aa55000a22010080000000", 60},  // C4
  {"aa55000a22010040000000", 72},  // C5
  {"aa55000a22010020000000", 84},  // C6
  {"aa55000a22010400000000", 62},  // D4
  {"aa55000a22010200000000", 74},  // D5
  {"aa55000a22010100000000", 86}   // D6
};

struct HexToProgram {
  const char* hex;
  uint8_t program;
};

HexToProgram hexToProgram[] = {
  {"aa550003220200d8", 0}, // Rocker Up
  {"aa5500032202ffd9", 1}, // Rocker Down
};

struct HexToControl {
  const char* hex;
  uint8_t cc;
};

HexToControl hexToControl[] = {
  {"f5550003201401", 2},  // Right
  {"f5550003201402", 1},  // Left
  {"f5550003201000", 4},  // Circle not Lit
  {"f5550003201001", 3},  // Circle Lit
  {"f5550003201400", 0} //ignore
};




void setup() {
  pinMode(NOTE_OFF_PIN, INPUT_PULLUP);
  pinMode(CC_TRIGGER_PIN, INPUT_PULLUP);

  Serial1.begin(250000); // Guitar
  Serial2.begin(250000); // Keyboard
  Serial.begin(115200);  // USB debug monitor

  usbMIDI.begin();
  while (!Serial && millis() < 3000);  // Wait for Serial Monitor
  Serial.println("Teensy MIDI Debug Start");
}

void sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity = 127) {
  usbMIDI.sendNoteOn(note, velocity, channel);
  Serial.printf("Note ON: ch=%d note=%d vel=%d\n", channel, note, velocity);
}

void sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity = 0) {
  usbMIDI.sendNoteOff(note, velocity, channel);
  Serial.printf("Note OFF: ch=%d note=%d vel=%d\n", channel, note, velocity);
}

void sendCC(uint8_t channel, uint8_t cc, uint8_t value) {
  usbMIDI.sendControlChange(cc, value, channel);
  Serial.printf("CC: ch=%d cc=%d val=%d\n", channel, cc, value);
}

void sendProgram(uint8_t channel, uint8_t program) {
  usbMIDI.sendProgramChange(program, channel);
  Serial.printf("Program: ch=%d program=%d\n", channel, program);
}

void trimBuffer(String& buffer) {
  if (buffer.length() > MAX_BUFFER_SIZE) {
    buffer.remove(0, buffer.length() - MAX_BUFFER_SIZE);
  }
}

void checkSerialGuitar(HardwareSerial& serialPort, char* buffer, uint8_t& bufferLen, uint8_t channel) {
  size_t last_len = 0;
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
                sendProgram(channel, msg.program);
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
          if (msg.note != 0) 
          {
            sendNoteOn(channel, msg.note);
            lastGuitarNotes.push_back(msg.note);
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
        if (bufferLen >= len && strncmp(buffer, msg.hex, len) == 0) {
          if (msg.note != 0)
          {
            sendNoteOn(channel, msg.note);
            lastGuitarNotes.push_back(msg.note);
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
  if (!ignoringIdlePing)
  {
    //Serial.printf("Raw buffer (KB): %s\n", buffer);
  }

  // --- Step 2: If we're ignoring idle ping, skip until we see a valid start ---
  if (ignoringIdlePing) {
    while (bufferLen >= 2) {
      if ((buffer[0] == '8' && buffer[1] == '0') ||
          (buffer[0] == '9' && buffer[1] == '0') ||
          (bufferLen >= 4 && strncmp(buffer, "f555", 4) == 0)) {
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
            sendCC(channel, msg.cc, 127);
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

  bool ccState = digitalRead(CC_TRIGGER_PIN);
  if (ccState != prevCCState && ccState == LOW) {
    Serial.println("CC_TRIGGER_PIN pressed → Send CC");
    sendCC(GUITAR_CHANNEL, 0, 127);
  }
  prevCCState = ccState;

  usbMIDI.read();
  delayMicroseconds(100); 
}
