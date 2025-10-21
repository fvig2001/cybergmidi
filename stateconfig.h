#pragma once
#include "helperClasses.h"

//defines
#define MIN_SWITCH_PASS_TIME 3000
#define MUTE_NOTE_AFTER_SWITCH_TIME 1000
#define FAKENOTETIME 2500
#define NOTE_PASS_TIMEOUT 60000
#define MIN_BACKING_TYPE 0
#define MAX_BACKING_TYPE 10
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

#define PROGRAM_ACOUSTIC_GUITAR 24
#define PROGRAM_ACOUSTIC_BASS 32
#define PROGRAM_HARPSICORD 6
#define PROGRAM_DRUMS 0

#define MAX_CUSTOM_PATTERNS 3
#define MAX_BASS_PATTERNS 1
#define MAX_ACCOMPANIMENT_PATTERNS 1
#define MAX_STRUM_SEPARATION 100
#define MIN_STRUM_SEPARATION 0
#define MIN_IGNORED_GUITAR 21
#define MAX_IGNORED_GUITAR 26
#define BPM_COMPUTE_BUTTON 25
#define DRUM_STARTSTOP_BUTTON 26
#define DRUM_FILL_BUTTON 24
#define SUSTAIN_BUTTON 22
#define OCTAVE_DOWN_BUTTON 21
#define OCTAVE_UP_BUTTON 23
#define MIDI_BUTTON_CHANNEL_NOTE_OFFSET 0
#define BT_PRESS_ACTIVATE_COUNT 5

#define DRUM_INTRO_ID 0
#define DRUM_LOOP_ID 1
#define DRUM_LOOPHALF_ID 2
#define DRUM_FILL_ID 3
#define DRUM_END_ID 4
#define MAX_RELATIVE_OFFSET 5
#define MIN_RELATIVE_OFFSET -5

#define DEVICE_TYPE 0 //0 = Cyber G, 1 = MidiPlus Band

#define MAX_NOTE 127
#define REST_NOTE 255

#define MAX_OFFSET 65535
#define MAX_HOLDTIME 768

//#define USE_AND
// --- PIN DEFINITIONS ---
#define NOTE_OFF_PIN 9       // Digital input for turning off all notes
#define START_TRIGGER_PIN 11  // Digital input for triggering CC message, logo change
#define BUTTON_1_PIN 4  //unused due to hardware issues (device turns on)
#define BUTTON_2_PIN 5
#define BUTTON_3_PIN 6  //unused due to hardware issues (device hangs)
#define VOLUME_MSB 23 //TX5, 20->22
#define VOLUME_LSB 22 //RX5, 21->23
#define MAX_STRUM_STYLE 3
// Other constants
#define ACTUAL_NECKBUTTONS 27
//#define NECKBUTTONS 21
#define PROGRAM_CHANGE 0xC0
#define PITCH_BEND 0xE0
#define NOTE_ON 0x90
#define NOTE_OFF 0x80
#define START_MIDI 0xFA
#define CONTINUE_MIDI 0xFB
#define STOP_MIDI 0xFC
#define CLOCK_MIDI 0xF8
#define NOTBT 0
#define BTCLASSIC 1
#define BTBLE 2
//state
char lastb0 = 0;
char lastb1 = 0;
uint8_t cyberGOctave = -1; 
uint8_t cyberGCapo = -1; //problem you can't tell if it's 0 or 12
bool deviceFaked = false;
bool BLEConnected = false;
bool BTClassicConnected = false;
bool BLEEnabled = true;
bool BTClassicEnabled = true;
bool BLEDiscoveryEnabled = true;
bool BTClassicDiscoveryEnabled = true;
int volumeOffset = 0;
bool lastFakeGuitar = false;
bool toMuteAfterSwitching = false;
int toMuteAfterSwitchingTime = 0;
uint8_t lastKBPressed = 255;
bool buttonPressedChanged = false; //flag for kb know that fret button pressed was changed
bool isSustain = false; //actual sustain state
bool isSustainPressed = false; //sustain button is still pressed
bool bassStart = false;
bool accompanimentStart = false;
DrumState drumState = DrumStopped;
DrumState drumNextState = DrumNone;
bool muteWasRecentlyPressed = false;
int muteHoldTime = 0;
bool logoWasRecentlyPressed = false;
int logoHoldTime = 0;
int lastFakeNotePlayTime = millis();
int lastNotePressTime = millis();
int lastFakeNoteSendTime = millis();
bool isStrumUp = false; // used for alternating strumming order
//int strum; 
//int lastStrum;
bool passThroughSerial = true;
bool fullPassThroughSerial = true;
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
int lastAccompanimentNeckButtonPressed = -1;
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
bool externalUseFixedChannel = true;
uint8_t backingState = 1; //0 = stock, 1-10 - uses omnichord 108 backing
//uint16_t deviceBPM = 128;
std::vector<char> bufferList;
std::vector<char> bufferList2;
std::vector<bool> isSimpleChordMode;
std::vector<bool> enableAllNotesOnChords;
std::vector<bool> enableButtonMidi;
std::vector<uint8_t> omniChordModeGuitar; //omnichord modes
std::vector<bool> muteWhenLetGo;


std::vector<bool> ignoreSameChord;
std::vector<uint16_t> presetBPM;
bool isPhantomNotePlaying;
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
std::vector<bool> accompanimentEnabled;
std::vector<bool> properOmniChord5ths;

uint16_t AccompanimentPatternID;
uint16_t DrumPatternID[5];
uint16_t BassPatternID;
uint16_t GuitarPatternID0;
uint16_t GuitarPatternID1;
uint16_t GuitarPatternID2;
