#pragma once
#ifdef __cplusplus
extern "C" {
#endif
	// The following ifdef block is the standard way of creating macros which make exporting
	// from a DLL simpler. All files within this DLL are compiled with the MIDITOSTRUCT_EXPORTS
	// symbol defined on the command line. This symbol should not be defined on any project
	// that uses this DLL. This way any other project whose source files include this file see
	// MIDITOSTRUCT_API functions as being imported from a DLL, whereas this DLL sees symbols
	// defined with this macro as being exported.

	typedef struct s_EncodedNote {
		int midiNote;    // MIDI note number, 255 for rest
		int noteOrder;  // Sequential group index
		int length;     // Length in 1/64th or 1/256th notes (units)
		int lengthTicks;     // Length in ticks (for debug)
		int velocity;    // velocity
		int relativeOctave; //octave relative to root note
		int channel;
	} EncodedNote;

#ifdef MIDITOSTRUCT_EXPORTS
#define MIDITOSTRUCT_API __declspec(dllexport)
#else
#define MIDITOSTRUCT_API __declspec(dllimport)
#endif
	/*
	// This class is exported from the dll
	class MIDITOSTRUCT_API CMidiToStruct {
	public:
		CMidiToStruct(void);
		// TODO: add your methods here.
	};

	extern MIDITOSTRUCT_API int nMidiToStruct;

	MIDITOSTRUCT_API int fnMidiToStruct(void);
	*/
	//for writing struct to midi for playback. Should use actual notes for playback
	MIDITOSTRUCT_API int writeMidiFromEncoded(EncodedNote* standardNotes, int* noteSize, char* filename, int bpm);
	//for encoding midi to struct for the guitar. Outputs in actual notes and place holder notes
	MIDITOSTRUCT_API int ConvertMidiToStruct(char* fMidiFile, EncodedNote** standardNotes, EncodedNote** placeHolderNotes, int* cgdSize, int channel);

	int GetOmniChordToStruct(int backingNo, EncodedNote** placeHolderNotesBass, EncodedNote** placeHolderNotesAccompaniment, EncodedNote** placeHolderNotesDrums, int* cgdSizeBass, int* cgdSizeAccompaniment, int* cgdSizeDrums);
	//free up the data
	MIDITOSTRUCT_API int FreeStruct(EncodedNote* data);


#ifdef __cplusplus
}
#endif