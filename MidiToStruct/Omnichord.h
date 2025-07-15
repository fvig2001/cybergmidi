#pragma once
#include <vector>
#include "MidiToStruct.h"
#define MAX_VELOCITY 127
#define ACCOMPANIMENT_CHANNEL 5 // backing for omnichord tracks
#define BASS_CHANNEL 4
#define GUITAR_BUTTON_CHANNEL 3
#define GUITAR_CHANNEL 2
#define KEYBOARD_CHANNEL 1
#define DRUM_CHANNEL 10

void generateRock1();
void generateRock2();
void generateBossanova();
void generateFunk();
void generateCountry();
void generateDisco();
void generateSlowRock();
void generateSwing();
void generateWaltz();
void generateHiphop();
void getOmniChordBacking(int newBackingType, std::vector<EncodedNote>& drumOut, std::vector<EncodedNote>& bassOut, std::vector<EncodedNote>& accompanimentOut);
class SequencerNote
{
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
    SequencerNote(uint8_t newNote, int newholdTime, uint16_t newoffset, uint8_t newchannel, uint8_t newvelocity = MAX_VELOCITY, int8_t oct = 0)
    {
        note = newNote;
        holdTime = newholdTime;
        offset = newoffset;
        channel = newchannel;
        velocity = newvelocity;
        relativeOctave = oct;
    }
};
