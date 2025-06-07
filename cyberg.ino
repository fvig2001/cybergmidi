#include <iostream>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include "MidiFile.h" //midifile library 3rd party, put in midifile-master folder
#include <string>
//#define BASE128 
#ifndef BASE128
#define BASELEN 12
#else
#define BASELEN 3
#endif
#define REST_NOTE  255
using namespace std;
using namespace smf;

struct EncodedNote {
    uint8_t midiNote;    // MIDI note number, 255 for rest
    uint16_t noteOrder;  // Sequential group index
    uint32_t length;     // Length in 1/64th or 1/256th notes (units)
    int lengthTicks;     // Length in ticks (for debug)
    int velocity;    // velocity
};

struct NoteEvent {
    uint8_t note;
    int startTick;
    int endTick;
    double startSeconds;
    int velocity;    // velocity
};
#ifndef BASE128
int offsetCorrect(int start, int end, bool isRest)
{
    int offset = 0;
    int diff = (end - start);
    bool found = false;
    
    if (!found)
    {
        switch (diff)
        {
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
    }
    //cout << "offset " << offset << "\n";
    //cout << "diff " << diff << "\n";
    //assumes rests corrected correctly since notes are adjusted correctly

    return offset;
}
#else
int offsetCorrect(int start, int end, bool isRest)
{
    int offset = 0;
    int diff = (end - start);
    bool found = false;

    if (!found)
    {
        switch (diff)
        {
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


        //old
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
        default:
            offset = 0;
        }
    }
    //cout << "offset " << offset << "\n";
    //cout << "diff " << diff << "\n";
    //assumes rests corrected correctly since notes are adjusted correctly

    return offset;
}
#endif
// Helper function to snap ticks to nearest multiple of snapSize
int snapLengthTicks(int ticks, int snapSize) {
    return ticks;
}

void writeMidiFromEncoded(const vector<EncodedNote>& encodedNotes, const string& filename, int bpm, int tpq = 192) {
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
    for (const auto& note : encodedNotes) {
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
        noteOn.setP0(0x90);              // Note On, channel 0
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

        //currentTick += durationTicks;  // Advance to the next note or rest
        lastTime = durationTicks;
    }

    // Sort all events before writing to prevent negative delta errors
    midiFile.sortTracks();

    if (midiFile.write(filename)) {
        cout << "MIDI file written: " << filename << endl;
    }
    else {
        cerr << "Failed to write MIDI file." << endl;
    }
}

int convertDataToMidi(vector<EncodedNote> encoded, std::string filename, int bpm)
{
    writeMidiFromEncoded(encoded, filename.c_str(), bpm);
    return 0;
}



void printData(vector<EncodedNote> encoded)
{
    // Output encoded notes and rests
    for (const auto& note : encoded) {
        cout << "Note: " << (note.midiNote == REST_NOTE ? "Rest" : to_string(note.midiNote))
            << ", Order: " << note.noteOrder
#ifndef BASE128
            << ", Length (1/64): " << note.length
#else
            << ", Length (1/256): " << note.length
#endif
            << ", LengthTicks: " << note.lengthTicks
            << ", Velocity: " << note.velocity << endl;
    }
}
//basically convert for my purpose
vector<EncodedNote> changeNotesToSymbol(vector<EncodedNote> encoded)
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

void generateSerial(vector<EncodedNote> encoded, int instrument, int slot)
{
    std::string clr = "";
    std::string write = "";
    vector<EncodedNote> processed = changeNotesToSymbol(encoded);
    switch (instrument)
    {
    case 0:
        clr = "GCLR";
        write = "GADD";

        break;
    case 1:
        clr = "BCLR";
        write = "BADD";
        break;
    default:
        clr = "DCLR";
        write = "DADD";
        processed = encoded;
        break;
    }
    cout << clr << "," << to_string(slot) << std::endl;
    int lastNoteOrder = 0;
    int curTicks = 1; //start
    int lastLen = -1;
    for (int i = 0; i < encoded.size(); i++)
    {
        int note;
        switch (instrument)
        {
        case 0:
            clr = "BCLR";
            write = "BADD";
            break;
        default:
            clr = "DCLR";
            write = "DADD";
            break;
        }
        //convert note order to ticks by adding last tick to move the offset/start note time
        //1 is start
        if (lastNoteOrder != encoded[i].noteOrder)
        {
            curTicks += lastLen;
            lastNoteOrder = encoded[i].noteOrder;
        }
        lastLen = encoded[i].lengthTicks;
        //if (encoded[i].midiNote != 255)
        {
            cout << write << "," << to_string(encoded[i].midiNote) << "," << to_string(slot) << "," << to_string(curTicks) << "," << to_string(encoded[i].lengthTicks) << "," << to_string(encoded[i].velocity) << "\n";
        }
    }

}
//note = newNote;
//holdTime = newholdTime;
//offset = newoffset;
//channel = newchannel;
//velocity = newvelocity;

bool convertToCyberG(vector<EncodedNote> encoded)
{
    vector<EncodedNote> adjusted;
    adjusted = changeNotesToSymbol(encoded);
    //given adjusted, we will be printing as code to use
    //SequencerNote note(midiNote, 65535, currentOffset, GUITAR_CHANNEL);
    int scale = 4; //1/256 -> Cyber G still uses 1/64
    int curOffset = 1;
    int curOrder = -1;
    int lastTime = -1;
    for (int i = 0; i < adjusted.size(); i++)
    {
        if (curOrder < adjusted[i].noteOrder)
        {
            //move time
            if (lastTime != -1)
            {
                curOffset += lastTime;
            }
            curOrder = adjusted[i].noteOrder;
        }
        //get order

        cout << "SequencerNote note(" << adjusted[i].midiNote << ", " << adjusted[i].length / scale << ", " << curOffset << ", GUITAR_CHANNEL);\n";
        cout << "SequencerNotes.push_back(note);\n";
        lastTime = adjusted[i].length / scale;
    }
    

    return true;
}
int main(int argc, char** argv) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <midi-file> <instrument 0-2> <slot number>" << endl;
        cerr << "Guitar = 0, Bass = 1, Drums = 2. Guitar has 0-2 slots, Drums has 5, bass has 0." << endl;
        return 1;
    }
    bool addRests = false;
    MidiFile midi;
    if (!midi.read(argv[1])) {
        cerr << "Failed to read MIDI file." << endl;
        return 1;
    }
    int instrument = atoi(argv[2]);
    int slot = atoi(argv[3]);
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
    /*
    for (const auto& note : notes) {
        cout << "Note: " << (int)note.note
            << ", Start: " << note.startTick
            << ", End: " << note.endTick 
            << ", Velocity: " << note.velocity
            << endl;

    }
    */
    i = 0;
    while (i < notes.size()) {
        
        int groupStartTick = notes[i].startTick;

        // DEBUG: Print current state
        //cout << "Processing group at tick " << groupStartTick
            //<< ", lastGroupEndTick = " << lastGroupEndTick << endl;

        // Handle rest between last group's end and this group's start
        if (lastGroupEndTick >= 0 && groupStartTick > lastGroupEndTick) {
            int restTicks = groupStartTick - lastGroupEndTick;
            int snappedRestTicks = snapLengthTicks(restTicks, ticksPerUnit);
            //snappedRestTicks = snappedRestTicks + offsetCorrect(lastGroupEndTick, groupStartTick, true);
            snappedRestTicks = snappedRestTicks + offsetCorrect(lastGroupEndTick, groupStartTick, true);

            //uint32_t restLength = snappedRestTicks / ticksPerUnit;
            uint32_t restLength = snappedRestTicks / BASELEN;
            if (restLength > 0) {
                encoded.push_back({ REST_NOTE, noteOrder++, restLength, snappedRestTicks,0 });
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
            int snappedNoteTicks = snapLengthTicks(noteTicks, ticksPerUnit);
            int velocity = notes[k].velocity;
            //uint32_t lengthUnits = snappedNoteTicks / ticksPerUnit;
            uint32_t lengthUnits = noteTicks / BASELEN;
            encoded.push_back({ notes[k].note, noteOrder, lengthUnits, noteTicks, velocity });

            if (notes[k].endTick > groupEndTick) {
                groupEndTick = notes[k].endTick;
            }
        }

        // CRITICAL: Update lastGroupEndTick for the next iteration
        lastGroupEndTick = groupEndTick;
        noteOrder++;
        i = j;
    }
    //printData(encoded); //for drums as we need actual notes
    //cout << "Adjusted:\n";
    //printData(encoded); 
    //printData(changeNotesToSymbol(encoded)); //for bass/guitar
    //writeMidiFromEncoded(encoded, "test.mid", 129);
    generateSerial(encoded, instrument, slot);
    return 0;
}
