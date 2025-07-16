
// MidiToStruct.cpp : Defines the exported functions for the DLL.
//

#include "pch.h"
#include "framework.h"
#include "MidiToStruct.h"
#include "Omnichord.h"


#include <set>
#include <map>
#include <iostream>
#include <vector>
#include <map>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include "MidiFile.h" //midifile library 3rd party, put in midifile-master folder
#include <string>

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

/*


MIDITOSTRUCT_API int writeMidiFromEncoded(EncodedNote* encodedNotes, int* noteSize, char* filename, int bpm = 128)
{
    if (!encodedNotes || !noteSize || filename == nullptr || *noteSize <= 0)
        return 0;

    std::sort(encodedNotes, encodedNotes + *noteSize,
        [](const EncodedNote& a, const EncodedNote& b) {
            return a.noteOrder < b.noteOrder;
        });
    const int tpq = 192;

    smf::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(tpq);

    // Track 0 reserved for tempo/meta
    midiFile.addTrack();

    std::map<int, std::vector<EncodedNote>> notesByOrder;
    std::set<int> uniqueChannels;

    for (int i = 0; i < *noteSize; ++i) {
        EncodedNote& note = encodedNotes[i];
        notesByOrder[note.noteOrder].push_back(note);
        uniqueChannels.insert(note.channel);
    }

    std::map<int, int> channelToTrack;
    int trackIndex = 1; // 0 is tempo/meta

    for (auto ch : uniqueChannels) {
        midiFile.addTrack();
        channelToTrack[ch] = trackIndex++;
    }

    // Insert tempo event at tick 0 on track 0
    smf::MidiEvent tempoEvent;
    tempoEvent.tick = 0;
    tempoEvent.makeTempo(bpm);
    midiFile[0].push_back(tempoEvent);

    // Per-channel current tick counters
    std::map<int, int> channelCurrentTicks;
    for (auto ch : uniqueChannels) {
        channelCurrentTicks[ch] = 0;
    }

    // Process notes order by order
    for (auto& orderPair : notesByOrder) {
        const std::vector<EncodedNote>& notes = orderPair.second;

        // Store max duration per channel in this order
        std::map<int, int> maxDurationPerChannel;

        // Step 1: write note events, all notes in this order start at the current channel tick
        for (const EncodedNote& note : notes) {
            int ch = note.channel;
            int duration = note.lengthTicks;

            int startTick = channelCurrentTicks[ch];  // All notes of same order start at channel's current tick

            if (note.midiNote == 255) {
                // Rest - no events, but remember duration for advancing channel tick later
                if (duration > maxDurationPerChannel[ch])
                    maxDurationPerChannel[ch] = duration;
                continue;
            }

            // Note On
            smf::MidiEvent noteOn;
            noteOn.tick = startTick;
            noteOn.setP0(0x90 | ch);
            noteOn.setP1(note.midiNote);
            noteOn.setP2(note.velocity);
            midiFile[channelToTrack[ch]].push_back(noteOn);

            // Note Off
            smf::MidiEvent noteOff;
            noteOff.tick = startTick + duration;
            noteOff.setP0(0x80 | ch);
            noteOff.setP1(note.midiNote);
            noteOff.setP2(0);
            midiFile[channelToTrack[ch]].push_back(noteOff);

            if (duration > maxDurationPerChannel[ch])
                maxDurationPerChannel[ch] = duration;
        }

        // Step 2: advance each channel's current tick by the max duration of notes/rests in this order
        for (auto& chDur : maxDurationPerChannel) {
            int ch = chDur.first;
            int dur = chDur.second;
            channelCurrentTicks[ch] += dur;
        }
    }

    midiFile.sortTracks();

    return midiFile.write(filename) ? 1 : 0;
}

*/

MIDITOSTRUCT_API int writeMidiFromEncoded(EncodedNote* encodedNotes, int* noteSize, char* filename, int bpm = 128)
{
    if (!encodedNotes || !noteSize || filename == nullptr || *noteSize <= 0)
        return 0;

    const int tpq = 192;

    smf::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(tpq);

    // Track 0 for tempo/meta
    midiFile.addTrack();

    std::map<int, std::map<int, std::vector<EncodedNote>>> notesByChannelAndOrder;
    std::set<int> channels;

    // Group notes by channel and then by noteOrder
    for (int i = 0; i < *noteSize; ++i) {
        const EncodedNote& note = encodedNotes[i];
        notesByChannelAndOrder[note.channel][note.noteOrder].push_back(note);
        channels.insert(note.channel);
    }

    // Allocate one track per channel
    std::map<int, int> channelToTrack;
    int trackIndex = 1;
    for (int ch : channels) {
        midiFile.addTrack();
        channelToTrack[ch] = trackIndex++;
    }

    // Insert tempo into meta track (track 0)
    smf::MidiEvent tempoEvent;
    tempoEvent.tick = 0;
    tempoEvent.makeTempo(bpm);
    midiFile[0].push_back(tempoEvent);

    // Encode each channel independently
    for (const auto& channelPair : notesByChannelAndOrder) {
        int originalChannel = channelPair.first;

        const auto& ordersMap = channelPair.second;
        int currentTick = 0;

        // Map channel 9 to MIDI channel 10 (index 9)
        int midiChannel = (originalChannel == 8) ? 9 : originalChannel;
        //int midiChannel = originalChannel;

        for (const auto& orderPair : ordersMap) {
            const std::vector<EncodedNote>& notes = orderPair.second;

            int maxDuration = 0;

            for (const auto& note : notes) {
                int duration = note.lengthTicks;
                if (duration > maxDuration)
                    maxDuration = duration;

                if (note.midiNote == 255)
                    continue;

                // Note On
                smf::MidiEvent noteOn;
                noteOn.tick = currentTick;
                noteOn.setP0(0x90 | midiChannel);  // mapped channel
                noteOn.setP1(note.midiNote);
                noteOn.setP2(note.velocity);
                midiFile[channelToTrack[originalChannel]].push_back(noteOn);

                // Note Off
                smf::MidiEvent noteOff;
                noteOff.tick = currentTick + duration;
                noteOff.setP0(0x80 | midiChannel);  // mapped channel
                noteOff.setP1(note.midiNote);
                noteOff.setP2(0);
                midiFile[channelToTrack[originalChannel]].push_back(noteOff);
            }

            currentTick += maxDuration;
        }
    }

    midiFile.sortTracks();
    return midiFile.write(filename) ? 1 : 0;
}


MIDITOSTRUCT_API int ConvertMidiToStruct(char* fMidi, EncodedNote** cgd, EncodedNote** cgdPlaceholder, int* cgdSize, int channel)
{
    //OutputDebugStringA("Hit native breakpoint point\n");

    if (cgdSize == NULL)
    {
        return -1;
    }
    
    bool addRests = false;
    MidiFile midi;
    if (!midi.read(fMidi)) {
        return -1;
    }
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
            int restLength = snappedRestTicks / TICKLEN;
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
            int lengthUnits = noteTicks / TICKLEN;
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
            (*cgd)[i] = encoded[i];
        }
    }
    if (cgdPlaceholder != NULL)
    {
        //replaces notes with simple numbers from 0 - x. Meant for substitution later
        encoded = changeNotesToSymbol(encoded);
        *cgdPlaceholder = (EncodedNote*)malloc(sizeof(EncodedNote) * (*cgdSize));
        for (int i = 0; i < encoded.size(); i++)
        {
            (*cgdPlaceholder)[i] = encoded[i];
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
        *cgdSizeBass = bassOut.size();
        *placeHolderNotesBass = (EncodedNote*)malloc(sizeof(EncodedNote) * (*cgdSizeBass));

        for (int i = 0; i < bassOut.size(); i++)
        {
            (*placeHolderNotesBass)[i] = bassOut[i];
        }

    }
    if (placeHolderNotesDrums != NULL)
    {
        *cgdSizeDrums = drumOut.size();
        (*placeHolderNotesDrums) = (EncodedNote*)malloc(sizeof(EncodedNote) * (*cgdSizeDrums));

        for (int i = 0; i < drumOut.size(); i++)
        {
            (*placeHolderNotesDrums)[i] = drumOut[i];
        }

    }
    if (placeHolderNotesAccompaniment != NULL)
    {
        *cgdSizeAccompaniment = static_cast<int>(accompanimentOut.size());
        *placeHolderNotesAccompaniment = (EncodedNote*)malloc(sizeof(EncodedNote) * (*cgdSizeAccompaniment));
        for (int i = 0; i < accompanimentOut.size(); i++)
        {
            (*placeHolderNotesAccompaniment)[i] = accompanimentOut[i];
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