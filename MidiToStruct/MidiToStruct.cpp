
// MidiToStruct.cpp : Defines the exported functions for the DLL.
//

#include "pch.h"
#include "framework.h"
#include "MidiToStruct.h"
#include "Omnichord.h"

#include <iostream>
#include <vector>
#include <map>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include "MidiFile.h" //midifile library 3rd party, put in midifile-master folder
#include <string>
#define BASELEN 12
using namespace std;
using namespace smf;
#define REST_NOTE  255
// This is an example of an exported variable
MIDITOSTRUCT_API int nMidiToStruct=0;

// This is an example of an exported function.
MIDITOSTRUCT_API int fnMidiToStruct(void)
{
    return 0;
}
struct NoteEvent {
    uint8_t note;
    int startTick;
    int endTick;
    double startSeconds;
    int velocity;    // velocity
};
int offsetCorrect(int start, int end, bool isRest)
{
    int offset = 0;
    int diff = (end - start);

    switch (diff)
    {

        //case 72:
        //case 24:
    case 232:
        offset = 8;
        break;
        //half note
    case 336:
        //half note dote
    case 528:
        //half note double dot
    case 624:
        offset = 48;
        break;
        //quarter dot
    case 256:
        //Quarter
    case 160:
    case 304:
        offset = 32;
        break;
        //whole double dot
    case 1296:
        //whole full + dot
    case 1104:
    case 720:
        offset = 48;
        break;
    case 310:
    case 286:
    case 382://?
    case 376:
    case 184:
        //16th
    case 46:
    case 70: //16th dot
    case 82: //16th double dot
        //Eigth dot
    case 142:
    case 166: //doubl dot
    case 94: //eighth
        offset = 2;
        break;
        //new
    //unknown
    case 118:
    case 190:
        //64th note
    case 10:
        //64th note dot
    case 19:
        //64th note dot dot
    case 16:
        //32nd note
    case 22:
        //32nd note dot
    case 40:
        //32nd note dot dot
    case 34:
        offset = 2;
        break;

    default:
        offset = 0;
    }

    //cout << "offset " << offset << "\n";
    //cout << "diff " << diff << "\n";
    //assumes rests corrected correctly since notes are adjusted correctly

    return offset;
}

/*
vector<EncodedNote> changeNotesToSymbol(vector<EncodedNote> encoded, map<int, int> & swapSymbols, vector<int> & offsets)
{
    vector<EncodedNote> adjusted;
    vector<uint8_t> notes;
    bool found = false;
    for (int i = 0; i < encoded.size(); i++)
    {
        found = false;
        for (int j = 0; j < notes.size(); j++)
        {
            if (encoded[i].midiNote == REST_NOTE)
            {
                //if rest, we do not substitute
                found = true;
                break;
            }
            if (notes[j] == encoded[i].midiNote)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            notes.push_back(encoded[i].midiNote);
        }
    }

    for (int i = 0; i < encoded.size(); i++)
    {
        EncodedNote n = encoded[i];
        found = false;
        for (int j = 0; j < notes.size(); j++)
        {
            if (notes[j] == encoded[i].midiNote)
            {
                n.midiNote = j;
                break;
            }
        }
        adjusted.push_back(n);
    }
    return adjusted;
}
*/

vector<EncodedNote> changeNotesToSymbol(vector<EncodedNote> encoded)
{
    vector<EncodedNote> adjusted;

    // Notes in C Major 9 chord (C3, E3, G3, B3, D4)
    // Symbol order: C=0, E=1, G=2, B=3, D=4
    //vector<int> chordNotes = { 60, 64, 67, 71, 74, 77, 81 }; // C3, E3, G3, B3, D4, F4, A4
    vector<int> chordNotes = { 60, 64, 67, 71, 74, 77, 81 }; // C3, E3, G3, B3, D4, F4, A4
    for (int i = 0; i < chordNotes.size(); i++)
    {
        chordNotes[i] -= 12;
    }
    int transpose = 1; //force to be higher
    for (const EncodedNote& n : encoded)
    {
        EncodedNote newNote = n;

        if (n.midiNote == 255) // REST
        {
            newNote.midiNote = 255;
            newNote.relativeOctave = 0;
        }
        else
        {
            bool found = false;

            for (int i = 0; i < chordNotes.size(); i++)
            {
                int noteDiff = n.midiNote - chordNotes[i];
                if (noteDiff % 12 == 0)
                {
                    newNote.midiNote = i; // 0 to 4
                    newNote.relativeOctave = noteDiff / 12;
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                // Skip or handle error if note is not in chord
                continue;
            }
        }

        adjusted.push_back(newNote);
    }

    return adjusted;
}


MIDITOSTRUCT_API int writeMidiFromEncoded(EncodedNote *encodedNotes, int *noteSize, char * filename, int bpm = 128) 
{
    int tpq = 192;
    if (encodedNotes == NULL || noteSize == NULL || filename == NULL)
    {
        return 0;
    }
    MidiFile midiFile;
    midiFile.addTrack(1);  // One track
    midiFile.setTicksPerQuarterNote(tpq);

    int currentTick = 0;
    int ticksPerUnit = BASELEN;

    // Insert tempo at tick 0
    MidiEvent tempoEvent;
    tempoEvent.tick = 0;
    tempoEvent.makeTempo(bpm);
    midiFile[0].push_back(tempoEvent);
    int curOrder = -1;
    int lastTime = -1;
    for (int i = 0; i < *noteSize; i++) 
    {
        EncodedNote note = encodedNotes[i];
        if (curOrder < note.noteOrder)
        {
            if (lastTime != -1)
            {
                curOrder = note.noteOrder;
                currentTick += lastTime;  // Advance to the next note or rest
            }
        }
        int durationTicks = note.length * ticksPerUnit;

        if (note.midiNote == 255) {
            // Rest: move forward in time without outputting anything
            //currentTick += durationTicks;
            lastTime = durationTicks;
            continue;
        }

        // Create Note On event
        MidiEvent noteOn;
        noteOn.tick = currentTick;
        
        noteOn.setP0(0x90 | note.channel);              // Note On, channel 0
        noteOn.setP1(note.midiNote);     // Note number
        noteOn.setVelocity(note.velocity);
        noteOn.setP2(100);               // Velocity
        midiFile[0].push_back(noteOn);

        // Create Note Off event
        MidiEvent noteOff;
        noteOff.tick = currentTick + durationTicks;
        noteOff.setP0(0x80);             // Note Off, channel 0
        noteOff.setP1(note.midiNote);
        noteOff.setP2(0);
        midiFile[0].push_back(noteOff);
        lastTime = durationTicks;
    }

    // Sort all events before writing to prevent negative delta errors
    midiFile.sortTracks();

    if (midiFile.write(filename)) {
        return 1;
    }
    else {
        return 0;
    }
}




MIDITOSTRUCT_API int ConvertMidiToStruct(char* fMidi, EncodedNote** cgd, EncodedNote** cgdPlaceholder, int* cgdSize, int channel)
{
    if (cgdSize == NULL)
    {
        return -1;
    }
    bool addRests = false;
    MidiFile midi;

    midi.doTimeAnalysis();
    midi.linkNotePairs();
    midi.joinTracks();
    int tpq = midi.getTicksPerQuarterNote();
    //cout << "tpq = " << tpq << "\n";
    int track = 0;

    vector<NoteEvent> notes;

    // Extract note events (note-ons with linked note-offs)
    for (int i = 0; i < midi[track].size(); ++i) {
        MidiEvent& ev = midi[track][i];
        if (!ev.isNoteOn() || ev.getVelocity() == 0) continue;
        if (!ev.isLinked()) continue;

        notes.push_back({
            static_cast<uint8_t>(ev.getKeyNumber()),
            ev.tick,
            ev.getLinkedEvent()->tick,
            ev.seconds,
            ev.getVelocity()
            });

    }

    if (notes.empty()) {
        cout << "No notes found in the MIDI file." << endl;
        return 0;
    }

    // Sort notes by start tick
    sort(notes.begin(), notes.end(), [](const NoteEvent& a, const NoteEvent& b) {
        return a.startTick < b.startTick;
        });

    vector<EncodedNote> encoded;
    uint16_t noteOrder = 0;


    int lastGroupEndTick = -1;
    size_t i = 0;

    // 1/64 note unit in ticks (tpq / 16)
    const int ticksPerUnit = tpq / 16; // e.g. 192 / 16 = 16 ticks per 1/64 note
    for (i = 0; i < notes.size(); i++)
    {
        notes[i].endTick += offsetCorrect(notes[i].startTick, notes[i].endTick, false);
    }

    i = 0;
    while (i < notes.size()) {

        int groupStartTick = notes[i].startTick;

        // DEBUG: Print current state
        //cout << "Processing group at tick " << groupStartTick
            //<< ", lastGroupEndTick = " << lastGroupEndTick << endl;

        // Handle rest between last group's end and this group's start
        if (lastGroupEndTick >= 0 && groupStartTick > lastGroupEndTick) {
            int restTicks = groupStartTick - lastGroupEndTick;
            int snappedRestTicks = restTicks;
            //snappedRestTicks = snappedRestTicks + offsetCorrect(lastGroupEndTick, groupStartTick, true);
            snappedRestTicks = snappedRestTicks + offsetCorrect(lastGroupEndTick, groupStartTick, true);

            //uint32_t restLength = snappedRestTicks / ticksPerUnit;
            int restLength = snappedRestTicks / BASELEN;
            if (restLength > 0) {
                encoded.push_back({ REST_NOTE, noteOrder++, restLength, snappedRestTicks,127,0, channel });
            }
        }

        // Find all notes in this group (same start time)
        size_t j = i;
        while (j < notes.size() && notes[j].startTick == groupStartTick) {
            j++;
        }

        // Determine the longest note in this group
        int groupEndTick = groupStartTick; // Initialize to minimum possible
        for (size_t k = i; k < j; k++) {
            int noteTicks = notes[k].endTick - notes[k].startTick;
            int snappedNoteTicks = noteTicks;
            int velocity = notes[k].velocity;
            //uint32_t lengthUnits = snappedNoteTicks / ticksPerUnit;
            int lengthUnits = noteTicks / BASELEN;
            encoded.push_back({ notes[k].note, noteOrder, lengthUnits, noteTicks, velocity });

            if (notes[k].endTick > groupEndTick) {
                groupEndTick = notes[k].endTick;
            }
        }

        // CRITICAL: Update lastGroupEndTick for the next iteration
        lastGroupEndTick = groupEndTick;
        noteOrder++;
        i = j;
        if (encoded.size() < 1)
        {
            return 0;
        }
    }
    //erase the last note as it's unneeded
    int targetOrder = encoded.back().noteOrder;

    // Remove all elements with that noteOrder
    encoded.erase(
        std::remove_if(
            encoded.begin(),
            encoded.end(),
            [targetOrder](const EncodedNote& note) {
                return note.noteOrder == targetOrder;
            }
        ),
        encoded.end()
    );
    *cgdSize = (int) encoded.size();
    if (cgd != NULL)
    {
        *cgd = (EncodedNote*)malloc(sizeof(EncodedNote) * (*cgdSize));
        for (int i = 0; i < encoded.size(); i++)
        {
            *cgd[i] = encoded[i];
        }
    }
    if (cgdPlaceholder != NULL)
    {
        //replaces notes with simple numbers from 0 - x. Meant for substitution later
        encoded = changeNotesToSymbol(encoded);
        *cgdPlaceholder = (EncodedNote*)malloc(sizeof(EncodedNote) * (*cgdSize));
        for (int i = 0; i < encoded.size(); i++)
        {
            *cgdPlaceholder[i] = encoded[i];
        }
    }
    return 1;
}
int GetOmniChordToStruct(int backingNo, EncodedNote** placeHolderNotesBass, EncodedNote** placeHolderNotesAccompaniment, EncodedNote** placeHolderNotesDrums, int* cgdSizeBass, int* cgdSizeAccompaniment, int* cgdSizeDrums)
{
    std::vector<EncodedNote> drumOut;
    std::vector<EncodedNote> bassOut;
    std::vector<EncodedNote> accompanimentOut;
    getOmniChordBacking(backingNo, drumOut, bassOut, accompanimentOut);
    if (placeHolderNotesBass != NULL)
    {
        *placeHolderNotesBass = (EncodedNote*)malloc(sizeof(bassOut) * (*cgdSizeBass));

        for (int i = 0; i < bassOut.size(); i++)
        {
            *placeHolderNotesBass[i] = bassOut[i];
        }
    }
    if (placeHolderNotesDrums != NULL)
    {
        *placeHolderNotesDrums = (EncodedNote*)malloc(sizeof(drumOut) * (*cgdSizeDrums));

        for (int i = 0; i < drumOut.size(); i++)
        {
            *placeHolderNotesDrums[i] = drumOut[i];
        }
    }
    if (placeHolderNotesAccompaniment != NULL)
    {
        *placeHolderNotesAccompaniment = (EncodedNote*)malloc(sizeof(accompanimentOut) * (*cgdSizeAccompaniment));
        for (int i = 0; i < accompanimentOut.size(); i++)
        {
            *placeHolderNotesAccompaniment[i] = accompanimentOut[i];
        }
    }
    return 1;
}
//needed to free memory correctly
MIDITOSTRUCT_API int FreeStruct(EncodedNote* cgd)
{
    if (cgd == NULL)
    {
        return -1;
    }
    free(cgd);
    return 0;
}
/*
// This is the constructor of a class that has been exported.
CMidiToStruct::CMidiToStruct()
{
    return;
}

*/