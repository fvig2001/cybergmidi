#pragma once
#define BTMW 0
#define BTRA 4
#define BTOW 2
#define BTMR 1
#define BTOR 3
#define BTCR 5

#include "helperClasses.h"
int GetPresetNote(int root, int interval)
{
  int val = 0;
  int octave = (interval/7) * SEMITONESPEROCTAVE;
  interval = interval % 7;
  switch(interval)
  {
    case 1: //2nd
      val = 2;
      break;
    case 2: //3rd
      val = 4;
      break;
    case 3: //4th
      val = 5;
      break;
    case 4: //5th
      val = 7;
      break;
    case 5: //6th
      val = 9;
      break;
    case 6: //7th
      val = 11;
      break;
    default:
      val = 0; //identity octave
  }
  return val + root + octave;
}
Chord GetPianoChords(uint8_t index)
{
  switch (index)
  {
    case majorChordType:  return Chord({ 4, 7 });               // Major
    case minorChordType:  return Chord({ 3, 7 });               // Minor
    case diminishedChordType:  return Chord({ 3, 6 });               // Diminished
    case augmentedChordType:  return Chord({ 4, 8 });               // Augmented
    case major7thChordType:  return Chord({ 4, 7, 11 });           // Major 7th
    case minor7thChordType:  return Chord({ 3, 7, 10 });           // Minor 7th
    case dominant7thChordType:  return Chord({ 4, 7, 10 });           // Dominant 7th
    case minor7Flat5ChordType:  return Chord({ 3, 6, 10 });           // Minor 7♭5
    case major6thChordType:  return Chord({ 4, 7, 9 });            // Major 6th
    case minor6thChordType:  return Chord({ 3, 7, 9 });            // Minor 6th
    case suspended2ChordType: return Chord({ 2, 7 });               // Suspended 2
    case suspended4ChordType: return Chord({ 5, 7 });               // Suspended 4
    case add9ChordType: return Chord({ 4, 7, 2 });            // Add 9
    case add11ChordType: return Chord({ 4, 7, 11 });           // Add 11 (same as major7th)
    case ninthChordType: return Chord({ 4, 7, 10, 14 });       // 9th
    case eleventhChordType: return Chord({ 4, 7, 10, 14, 17 });   // 11th
    case thirteenthChordType: return Chord({ 4, 7, 10, 14, 17, 21 });// 13th

     // ✅ Useful to Add:
    case majorAdd9ChordType:       return Chord({ 4, 7, 14 });
    case dominant7b9ChordType:     return Chord({ 4, 7, 10, 13 });
    case dominant7sharp9ChordType: return Chord({ 4, 7, 10, 15 });
    case dominant7b5ChordType:     return Chord({ 4, 6, 10 });
    case dominant7sharp5ChordType: return Chord({ 4, 8, 10 });
    case minorMajor7ChordType:     return Chord({ 3, 7, 11 });
    case major7b5ChordType:        return Chord({ 4, 6, 11 });
    case dim7ChordType:            return Chord({ 3, 6, 9 });   // fully diminished 7th
    case powerChordType:           return Chord({ 0, 7 });      // root + 5 only
    case dominant13b9ChordType:    return Chord({ 4, 7, 10, 13, 21 }); // 1 3 5 b7 b9 13
    case dominant7sus4ChordType:   return Chord({ 5, 7, 10 });         // 1 4 5 b7 //WEird issue
    case quartalChordType:         return Chord({ 5, 10 });           // 1 + perfect 4th + another 4th
    default:                       return Chord({ 1, 2});         // dense cluster: 1 b9 9

  }
}

char * getFakeNotesKB(bool isOn, int *size)
{
  const int noteCount = 3;
  const int bytesPerNote = 3;
  *size = noteCount * bytesPerNote;
  char* fakeNotes = (char*)malloc(*size);
   for (int i = 0; i < noteCount; ++i) 
  {
    fakeNotes[i * 3 + 0] = isOn ? 0x90 : 0x80;
  }

  fakeNotes[1] = 0x60; fakeNotes[2] = 0x7F;
  fakeNotes[4] = 0x64; fakeNotes[5] = 0x7F;
  fakeNotes[7] = 0x67; fakeNotes[8] = 0x7F;
  return fakeNotes;
}
char * getFakeNotesGuitar(bool isOn, int *size)
{
  const int noteCount = 11;
  *size = noteCount;
  char* fakeNotes = (char*)malloc(*size);
  fakeNotes[0] = 0xaa;
  fakeNotes[1] = 0x55;
  fakeNotes[2] = 0x00;
  fakeNotes[3] = 0x0a;
  fakeNotes[4] = 0x22;
  fakeNotes[5] = 0x01;
  
  if (isOn)
  {
    fakeNotes[6] = 0x04;
  }
  else
  {
    fakeNotes[6] = 0x00;
  }
  
  fakeNotes[6] = 0x00;
  fakeNotes[7] = 0x00;
  fakeNotes[8] = 0x00;
  fakeNotes[9] = 0x00;
  fakeNotes[10] = 0x00;
  return fakeNotes;
}

const std::vector<uint8_t>& GetAllChordsGuitar(uint8_t chordTypeIndex, uint8_t key)
{
  static const std::vector<uint8_t> empty = {};

  switch (chordTypeIndex)
  {
    case majorChordType: // majorChordGuitar
      switch (key)
      {
        case 0:  static const std::vector<uint8_t> c0 = { 48, 52, 55, 60, 64 }; return c0;
        case 1:  static const std::vector<uint8_t> c1 = { 49, 56, 61, 65, 68 }; return c1;
        case 2:  static const std::vector<uint8_t> c2 = { 50, 57, 62, 66 }; return c2;
        case 3:  static const std::vector<uint8_t> c3 = { 51, 58, 63, 67, 70 }; return c3;
        case 4:  static const std::vector<uint8_t> c4 = { 40, 47, 52, 56, 59, 64 }; return c4;
        case 5:  static const std::vector<uint8_t> c5 = { 41, 48, 53, 57, 60, 65 }; return c5;
        case 6:  static const std::vector<uint8_t> c6 = { 42, 49, 54, 58, 61, 66 }; return c6;
        case 7:  static const std::vector<uint8_t> c7 = { 43, 47, 50, 55, 59, 67 }; return c7;
        case 8:  static const std::vector<uint8_t> c8 = { 44, 51, 56, 60, 63, 68 }; return c8;
        case 9:  static const std::vector<uint8_t> c9 = { 45, 52, 57, 61, 64 }; return c9;
        case 10: static const std::vector<uint8_t> c10 = { 46, 50, 58, 65, 70 }; return c10;
        default: static const std::vector<uint8_t> c11 = { 47, 54, 59, 63, 66 }; return c11;
      }

    case minorChordType: // minorChordGuitar
      switch (key)
      {
        case 0:  static const std::vector<uint8_t> c0 = { 48, 51, 60, 63, 67 }; return c0;
        case 1:  static const std::vector<uint8_t> c1 = { 49, 54, 61, 64, 68 }; return c1;
        case 2:  static const std::vector<uint8_t> c2 = { 50, 57, 62, 65 }; return c2;
        case 3:  static const std::vector<uint8_t> c3 = { 51, 58, 63, 66, 70 }; return c3;
        case 4:  static const std::vector<uint8_t> c4 = { 40, 47, 52, 55, 59, 64 }; return c4;
        case 5:  static const std::vector<uint8_t> c5 = { 41, 48, 53, 56, 60 }; return c5;
        case 6:  static const std::vector<uint8_t> c6 = { 42, 49, 54, 57, 61 }; return c6;
        case 7:  static const std::vector<uint8_t> c7 = { 43, 50, 55, 58, 62 }; return c7;
        case 8:  static const std::vector<uint8_t> c8 = { 44, 51, 56, 59, 63 }; return c8;
        case 9:  static const std::vector<uint8_t> c9 = { 45, 52, 57, 60, 64 }; return c9;
        case 10: static const std::vector<uint8_t> c10 = { 46, 53, 58, 61, 65 }; return c10;
        default: static const std::vector<uint8_t> c11 = { 47, 54, 59, 62, 66 }; return c11;
      }
    case diminishedChordType:
      switch (key)
      {
        case 0:  static const std::vector<uint8_t> c0 = { 48, 51, 54, 63}; return c0;
        case 1:  static const std::vector<uint8_t> c1 = { 49, 52, 55, 64}; return c1;
        case 2:  static const std::vector<uint8_t> c2 = { 50, 53, 56, 65}; return c2;
        case 3:  static const std::vector<uint8_t> c3 = { 51, 54, 57, 66}; return c3;
        case 4:  static const std::vector<uint8_t> c4 = { 40, 43, 46, 55}; return c4;
        case 5:  static const std::vector<uint8_t> c5 = { 41, 44, 47, 56}; return c5;
        case 6:  static const std::vector<uint8_t> c6 = { 42, 45, 48, 57}; return c6;
        case 7:  static const std::vector<uint8_t> c7 = { 43, 46, 49, 58}; return c7;
        case 8:  static const std::vector<uint8_t> c8 = { 44, 47, 50, 59}; return c8;
        case 9:  static const std::vector<uint8_t> c9 = { 45, 48, 51, 60}; return c9;
        case 10: static const std::vector<uint8_t> c10 = { 46, 49, 52, 61}; return c10;
        default: static const std::vector<uint8_t> c11 = { 47, 50, 53, 62}; return c11;
      }

    case augmentedChordType:
          switch (key)
      {
        case 0:  static const std::vector<uint8_t> c0 = { 48, 52, 56, 60, 64 }; return c0;
        case 1:  static const std::vector<uint8_t> c1 = { 49, 53, 57, 61, 65 }; return c1;
        case 2:  static const std::vector<uint8_t> c2 = { 50, 54, 58, 62, 66 }; return c2;
        case 3:  static const std::vector<uint8_t> c3 = { 51, 55, 59, 63, 67 }; return c3;
        case 4:  static const std::vector<uint8_t> c4 = { 40, 44, 48, 52, 56 }; return c4;
        case 5:  static const std::vector<uint8_t> c5 = { 41, 45, 49, 53, 57 }; return c5;
        case 6:  static const std::vector<uint8_t> c6 = { 42, 46, 50, 54, 58 }; return c6;
        case 7:  static const std::vector<uint8_t> c7 = { 43, 47, 51, 55, 59 }; return c7;
        case 8:  static const std::vector<uint8_t> c8 = { 44, 48, 52, 56, 60 }; return c8;
        case 9:  static const std::vector<uint8_t> c9 = { 45, 49, 53, 57, 61 }; return c9;
        case 10: static const std::vector<uint8_t> c10 = { 46, 50, 54, 58, 62 }; return c10;
        default: static const std::vector<uint8_t> c11 = { 47, 51, 55, 59, 63 }; return c11;
      }
    case major7thChordType:
      switch (key)
      {
        case 0:  static const std::vector<uint8_t> c0 = { 48, 52, 55, 71, 64 }; return c0;
        case 1:  static const std::vector<uint8_t> c1 = { 49, 53, 56, 72, 65 }; return c1;
        case 2:  static const std::vector<uint8_t> c2 = { 50, 54, 57, 73, 66 }; return c2;
        case 3:  static const std::vector<uint8_t> c3 = { 51, 55, 58, 74, 67 }; return c3;
        case 4:  static const std::vector<uint8_t> c4 = { 40, 44, 47, 63, 56 }; return c4;
        case 5:  static const std::vector<uint8_t> c5 = { 41, 45, 48, 64, 57 }; return c5;
        case 6:  static const std::vector<uint8_t> c6 = { 42, 46, 49, 65, 58 }; return c6;
        case 7:  static const std::vector<uint8_t> c7 = { 43, 47, 50, 66, 59 }; return c7;
        case 8:  static const std::vector<uint8_t> c8 = { 44, 48, 51, 67, 60 }; return c8;
        case 9:  static const std::vector<uint8_t> c9 = { 45, 49, 52, 68, 61 }; return c9;
        case 10: static const std::vector<uint8_t> c10 = { 46, 50, 53, 69, 62 }; return c10;
        default: static const std::vector<uint8_t> c11 = { 47, 51, 54, 70, 63 }; return c11;
      }

    case minor7thChordType:
      switch (key)
      {
        case 0:  static const std::vector<uint8_t> c0 = { 48, 55, 58, 63, 67 }; return c0;
        case 1:  static const std::vector<uint8_t> c1 = { 49, 56, 59, 64, 68 }; return c1;
        case 2:  static const std::vector<uint8_t> c2 = { 50, 57, 60, 65, 69 }; return c2;
        case 3:  static const std::vector<uint8_t> c3 = { 51, 58, 61, 66, 70 }; return c3;
        case 4:  static const std::vector<uint8_t> c4 = { 40, 47, 50, 55, 59 }; return c4;
        case 5:  static const std::vector<uint8_t> c5 = { 41, 48, 51, 56, 60 }; return c5;
        case 6:  static const std::vector<uint8_t> c6 = { 42, 49, 52, 57, 61 }; return c6;
        case 7:  static const std::vector<uint8_t> c7 = { 43, 50, 53, 58, 62 }; return c7;
        case 8:  static const std::vector<uint8_t> c8 = { 44, 51, 54, 59, 63 }; return c8;
        case 9:  static const std::vector<uint8_t> c9 = { 45, 52, 55, 60, 64 }; return c9;
        case 10: static const std::vector<uint8_t> c10 = { 46, 53, 56, 61, 65 }; return c10;
        default: static const std::vector<uint8_t> c11 = { 47, 54, 57, 62, 66 }; return c11;
      }

      case dominant7thChordType:
        switch (key)
        {
          case 0:  static const std::vector<uint8_t> c0 = { 48, 55, 58, 64, 67 }; return c0;
          case 1:  static const std::vector<uint8_t> c1 = { 49, 56, 59, 65, 68 }; return c1;
          case 2:  static const std::vector<uint8_t> c2 = { 50, 57, 60, 66 }; return c2;
          case 3:  static const std::vector<uint8_t> c3 = { 51, 58, 61, 67, 68 }; return c3;
          case 4:  static const std::vector<uint8_t> c4 = { 40, 47, 52, 56, 62, 64 }; return c4;
          case 5:  static const std::vector<uint8_t> c5 = { 41, 48, 51, 57, 60 }; return c5;
          case 6:  static const std::vector<uint8_t> c6 = { 42, 46, 49, 52 }; return c6;
          case 7:  static const std::vector<uint8_t> c7 = { 43, 47, 50, 55, 59, 65 }; return c7;
          case 8:  static const std::vector<uint8_t> c8 = { 56, 63, 66, 72, 75 }; return c8;
          case 9:  static const std::vector<uint8_t> c9 = { 45, 52, 55, 61, 64 }; return c9;
          case 10: static const std::vector<uint8_t> c10 = { 46, 53, 56, 62, 65 }; return c10;
          default: static const std::vector<uint8_t> c11 = { 47, 51, 57, 59, 66 }; return c11;
        }

      case minor7Flat5ChordType:
        switch (key)
        {
          case 0:  static const std::vector<uint8_t> c0  = { 48, 51, 58, 60, 66 }; return c0;
          case 1:  static const std::vector<uint8_t> c1  = { 49, 52, 59, 61, 67 }; return c1;
          case 2:  static const std::vector<uint8_t> c2  = { 50, 53, 60, 62, 68 }; return c2;
          case 3:  static const std::vector<uint8_t> c3  = { 51, 54, 61, 63, 69 }; return c3;
          case 4:  static const std::vector<uint8_t> c4  = { 40, 43, 50, 52, 58 }; return c4;
          case 5:  static const std::vector<uint8_t> c5  = { 41, 44, 51, 53, 59 }; return c5;
          case 6:  static const std::vector<uint8_t> c6  = { 42, 45, 52, 54, 60 }; return c6;
          case 7:  static const std::vector<uint8_t> c7  = { 43, 46, 53, 55, 61 }; return c7;
          case 8:  static const std::vector<uint8_t> c8  = { 44, 47, 54, 56, 62 }; return c8;
          case 9:  static const std::vector<uint8_t> c9  = { 45, 48, 55, 57, 63 }; return c9;
          case 10: static const std::vector<uint8_t> c10 = { 46, 49, 56, 58, 64 }; return c10;
          default: static const std::vector<uint8_t> c11 = { 47, 50, 57, 59, 65 }; return c11;
        }

      case major6thChordType:
        switch (key)
        {
          case 0:  static const std::vector<uint8_t> c0  = { 48, 52, 57, 60, 64 }; return c0;
          case 1:  static const std::vector<uint8_t> c1  = { 49, 53, 58, 61, 65 }; return c1;
          case 2:  static const std::vector<uint8_t> c2  = { 50, 54, 59, 62, 66 }; return c2;
          case 3:  static const std::vector<uint8_t> c3  = { 51, 55, 60, 63, 67 }; return c3;
          case 4:  static const std::vector<uint8_t> c4  = { 40, 44, 49, 52, 56 }; return c4;
          case 5:  static const std::vector<uint8_t> c5  = { 41, 45, 50, 53, 57 }; return c5;
          case 6:  static const std::vector<uint8_t> c6  = { 42, 46, 51, 54, 58 }; return c6;
          case 7:  static const std::vector<uint8_t> c7  = { 43, 47, 52, 55, 59 }; return c7;
          case 8:  static const std::vector<uint8_t> c8  = { 44, 48, 53, 56, 60 }; return c8;
          case 9:  static const std::vector<uint8_t> c9  = { 45, 49, 54, 57, 61 }; return c9;
          case 10: static const std::vector<uint8_t> c10 = { 46, 50, 55, 58, 62 }; return c10;
          default: static const std::vector<uint8_t> c11 = { 47, 51, 56, 59, 63 }; return c11;
        }

      case minor6thChordType:
        switch (key)
        {
          case 0:  static const std::vector<uint8_t> c0  = { 48, 51, 57, 60, 67 }; return c0;
          case 1:  static const std::vector<uint8_t> c1  = { 49, 52, 58, 61, 68 }; return c1;
          case 2:  static const std::vector<uint8_t> c2  = { 50, 53, 59, 62, 69 }; return c2;
          case 3:  static const std::vector<uint8_t> c3  = { 51, 54, 60, 63, 70 }; return c3;
          case 4:  static const std::vector<uint8_t> c4  = { 40, 44, 49, 52, 59 }; return c4;
          case 5:  static const std::vector<uint8_t> c5  = { 41, 45, 50, 53, 60 }; return c5;
          case 6:  static const std::vector<uint8_t> c6  = { 42, 46, 51, 54, 61 }; return c6;
          case 7:  static const std::vector<uint8_t> c7  = { 43, 47, 52, 55, 62 }; return c7;
          case 8:  static const std::vector<uint8_t> c8  = { 44, 48, 53, 56, 63 }; return c8;
          case 9:  static const std::vector<uint8_t> c9  = { 45, 49, 54, 57, 64 }; return c9;
          case 10: static const std::vector<uint8_t> c10 = { 46, 50, 55, 58, 65 }; return c10;
          default: static const std::vector<uint8_t> c11 = { 47, 51, 56, 59, 66 }; return c11;
        }

      case suspended2ChordType:
        switch (key)
        {
          case 0:  static const std::vector<uint8_t> c0  = { 48, 50, 55, 62, 67 }; return c0;
          case 1:  static const std::vector<uint8_t> c1  = { 49, 51, 56, 63, 68 }; return c1;
          case 2:  static const std::vector<uint8_t> c2  = { 50, 52, 57, 64, 69 }; return c2;
          case 3:  static const std::vector<uint8_t> c3  = { 51, 53, 58, 65, 70 }; return c3;
          case 4:  static const std::vector<uint8_t> c4  = { 40, 42, 47, 54, 59 }; return c4;
          case 5:  static const std::vector<uint8_t> c5  = { 41, 43, 48, 55, 60 }; return c5;
          case 6:  static const std::vector<uint8_t> c6  = { 42, 44, 49, 56, 61 }; return c6;
          case 7:  static const std::vector<uint8_t> c7  = { 43, 45, 50, 57, 62 }; return c7;
          case 8:  static const std::vector<uint8_t> c8  = { 44, 46, 51, 58, 63 }; return c8;
          case 9:  static const std::vector<uint8_t> c9  = { 45, 47, 52, 59, 64 }; return c9;
          case 10: static const std::vector<uint8_t> c10 = { 46, 48, 53, 60, 65 }; return c10;
          default: static const std::vector<uint8_t> c11 = { 47, 49, 54, 61, 66 }; return c11;
        }

    case suspended4ChordType:
      switch (key)
      {
        case 0:  static const std::vector<uint8_t> c0  = { 48, 53, 55, 60, 65 }; return c0;
        case 1:  static const std::vector<uint8_t> c1  = { 49, 54, 56, 61, 66 }; return c1;
        case 2:  static const std::vector<uint8_t> c2  = { 50, 55, 57, 62, 67 }; return c2;
        case 3:  static const std::vector<uint8_t> c3  = { 51, 56, 58, 63, 68 }; return c3;
        case 4:  static const std::vector<uint8_t> c4  = { 40, 45, 47, 52, 57 }; return c4;
        case 5:  static const std::vector<uint8_t> c5  = { 41, 46, 48, 53, 58 }; return c5;
        case 6:  static const std::vector<uint8_t> c6  = { 42, 47, 49, 54, 59 }; return c6;
        case 7:  static const std::vector<uint8_t> c7  = { 43, 48, 50, 55, 60 }; return c7;
        case 8:  static const std::vector<uint8_t> c8  = { 44, 49, 51, 56, 61 }; return c8;
        case 9:  static const std::vector<uint8_t> c9  = { 45, 50, 52, 57, 62 }; return c9;
        case 10: static const std::vector<uint8_t> c10 = { 46, 51, 53, 58, 63 }; return c10;
        default: static const std::vector<uint8_t> c11 = { 47, 52, 54, 59, 64 }; return c11;
      }

      case add9ChordType:
        switch (key)
        {
          case 0:  static const std::vector<uint8_t> c0  = { 48, 52, 55, 62, 64 }; return c0;
          case 1:  static const std::vector<uint8_t> c1  = { 49, 53, 56, 63, 65 }; return c1;
          case 2:  static const std::vector<uint8_t> c2  = { 50, 54, 57, 64, 66 }; return c2;
          case 3:  static const std::vector<uint8_t> c3  = { 51, 55, 58, 65, 67 }; return c3;
          case 4:  static const std::vector<uint8_t> c4  = { 40, 44, 47, 54, 56 }; return c4;
          case 5:  static const std::vector<uint8_t> c5  = { 41, 45, 48, 55, 57 }; return c5;
          case 6:  static const std::vector<uint8_t> c6  = { 42, 46, 49, 56, 58 }; return c6;
          case 7:  static const std::vector<uint8_t> c7  = { 43, 47, 50, 57, 59 }; return c7;
          case 8:  static const std::vector<uint8_t> c8  = { 44, 48, 51, 58, 60 }; return c8;
          case 9:  static const std::vector<uint8_t> c9  = { 45, 49, 52, 59, 61 }; return c9;
          case 10: static const std::vector<uint8_t> c10 = { 46, 50, 53, 60, 62 }; return c10;
          default: static const std::vector<uint8_t> c11 = { 47, 51, 54, 61, 63 }; return c11;
        }

      case add11ChordType:
        switch (key)
        {
          case 0:  static const std::vector<uint8_t> c0  = { 48, 52, 55, 60, 65 }; return c0;
          case 1:  static const std::vector<uint8_t> c1  = { 49, 53, 56, 61, 66 }; return c1;
          case 2:  static const std::vector<uint8_t> c2  = { 50, 54, 57, 62, 67 }; return c2;
          case 3:  static const std::vector<uint8_t> c3  = { 51, 55, 58, 63, 68 }; return c3;
          case 4:  static const std::vector<uint8_t> c4  = { 40, 44, 47, 52, 57 }; return c4;
          case 5:  static const std::vector<uint8_t> c5  = { 41, 45, 48, 53, 58 }; return c5;
          case 6:  static const std::vector<uint8_t> c6  = { 42, 46, 49, 54, 59 }; return c6;
          case 7:  static const std::vector<uint8_t> c7  = { 43, 47, 50, 55, 60 }; return c7;
          case 8:  static const std::vector<uint8_t> c8  = { 44, 48, 51, 56, 61 }; return c8;
          case 9:  static const std::vector<uint8_t> c9  = { 45, 49, 52, 57, 62 }; return c9;
          case 10: static const std::vector<uint8_t> c10 = { 46, 50, 53, 58, 63 }; return c10;
          default: static const std::vector<uint8_t> c11 = { 47, 51, 54, 59, 64 }; return c11;
        }

      case ninthChordType:
        switch (key)
        {
          case 0:  static const std::vector<uint8_t> c0  = { 48, 55, 58, 64, 70, 74 }; return c0;
          case 1:  static const std::vector<uint8_t> c1  = { 49, 56, 59, 65, 71, 75 }; return c1;
          case 2:  static const std::vector<uint8_t> c2  = { 50, 57, 60, 66, 72, 76 }; return c2;
          case 3:  static const std::vector<uint8_t> c3  = { 51, 58, 61, 67, 73, 77 }; return c3;
          case 4:  static const std::vector<uint8_t> c4  = { 40, 47, 50, 56, 62, 66 }; return c4;
          case 5:  static const std::vector<uint8_t> c5  = { 41, 48, 51, 57, 63, 67 }; return c5;
          case 6:  static const std::vector<uint8_t> c6  = { 42, 49, 52, 58, 64, 68 }; return c6;
          case 7:  static const std::vector<uint8_t> c7  = { 43, 50, 53, 59, 65, 69 }; return c7;
          case 8:  static const std::vector<uint8_t> c8  = { 44, 51, 54, 60, 66, 70 }; return c8;
          case 9:  static const std::vector<uint8_t> c9  = { 45, 52, 55, 61, 67, 71 }; return c9;
          case 10: static const std::vector<uint8_t> c10 = { 46, 53, 56, 62, 68, 72 }; return c10;
          default: static const std::vector<uint8_t> c11 = { 47, 54, 57, 63, 69, 73 }; return c11;
        }

      case eleventhChordType:
        switch (key)
        {
          case 0:  static const std::vector<uint8_t> c0  = { 43, 53, 58, 60, 64 }; return c0;
          case 1:  static const std::vector<uint8_t> c1  = { 44, 54, 59, 61, 65 }; return c1;
          case 2:  static const std::vector<uint8_t> c2  = { 45, 55, 60, 62, 66 }; return c2;
          case 3:  static const std::vector<uint8_t> c3  = { 46, 56, 61, 63, 67 }; return c3;
          case 4:  static const std::vector<uint8_t> c4  = { 47, 57, 62, 64, 68 }; return c4;
          case 5:  static const std::vector<uint8_t> c5  = { 48, 58, 63, 65, 69 }; return c5;
          case 6:  static const std::vector<uint8_t> c6  = { 49, 59, 64, 66, 70 }; return c6;
          case 7:  static const std::vector<uint8_t> c7  = { 50, 60, 65, 67, 71 }; return c7;
          case 8:  static const std::vector<uint8_t> c8  = { 51, 61, 66, 68, 72 }; return c8;
          case 9:  static const std::vector<uint8_t> c9  = { 52, 62, 67, 69, 73 }; return c9;
          case 10: static const std::vector<uint8_t> c10 = { 53, 63, 68, 70, 74 }; return c10;
          default: static const std::vector<uint8_t> c11 = { 54, 64, 69, 71, 75 }; return c11;
        }

      case thirteenthChordType:
        switch (key)
        {
          case 0:  static const std::vector<uint8_t> c0  = { 48, 52, 58, 62, 69 }; return c0;
          case 1:  static const std::vector<uint8_t> c1  = { 49, 53, 59, 63, 70 }; return c1;
          case 2:  static const std::vector<uint8_t> c2  = { 50, 54, 60, 64, 71 }; return c2;
          case 3:  static const std::vector<uint8_t> c3  = { 51, 55, 61, 65, 72 }; return c3;
          case 4:  static const std::vector<uint8_t> c4  = { 40, 44, 50, 54, 61 }; return c4;
          case 5:  static const std::vector<uint8_t> c5  = { 41, 45, 51, 55, 62 }; return c5;
          case 6:  static const std::vector<uint8_t> c6  = { 42, 46, 52, 56, 63 }; return c6;
          case 7:  static const std::vector<uint8_t> c7  = { 43, 47, 53, 57, 64 }; return c7;
          case 8:  static const std::vector<uint8_t> c8  = { 44, 48, 54, 58, 65 }; return c8;
          case 9:  static const std::vector<uint8_t> c9  = { 45, 49, 55, 59, 66 }; return c9;
          case 10: static const std::vector<uint8_t> c10 = { 46, 50, 56, 60, 67 }; return c10;
          default: static const std::vector<uint8_t> c11 = { 47, 51, 57, 61, 68 }; return c11;
        }
      case majorAdd9ChordType:
        switch (key) 
        {
          case 0:  static const std::vector<uint8_t> c0 = {48, 52, 55, 59, 62}; return c0;
          case 1:  static const std::vector<uint8_t> c1 = {49, 53, 56, 60, 63}; return c1;
          case 2:  static const std::vector<uint8_t> c2 = {50, 54, 57, 61, 64}; return c2;
          case 3:  static const std::vector<uint8_t> c3 = {51, 55, 58, 62, 65}; return c3;
          case 4:  static const std::vector<uint8_t> c4 = {40, 44, 47, 51, 54}; return c4;
          case 5:  static const std::vector<uint8_t> c5 = {41, 45, 48, 52, 55}; return c5;
          case 6:  static const std::vector<uint8_t> c6 = {42, 46, 49, 53, 56}; return c6;
          case 7:  static const std::vector<uint8_t> c7 = {43, 47, 50, 54, 57}; return c7;
          case 8:  static const std::vector<uint8_t> c8 = {44, 48, 51, 55, 58}; return c8;
          case 9:  static const std::vector<uint8_t> c9 = {45, 49, 52, 56, 59}; return c9;
          case 10: static const std::vector<uint8_t> c10 = {46, 50, 53, 57, 60}; return c10;
          default: static const std::vector<uint8_t> c11 = {47, 51, 54, 58, 61}; return c11;
        }
      case dominant7b9ChordType:
        switch (key) 
        {
          case 0:  static const std::vector<uint8_t> c0 = {48, 52, 55, 58, 61}; return c0;
          case 1:  static const std::vector<uint8_t> c1 = {49, 53, 56, 59, 62}; return c1;
          case 2:  static const std::vector<uint8_t> c2 = {50, 54, 57, 60, 63}; return c2;
          case 3:  static const std::vector<uint8_t> c3 = {51, 55, 58, 61, 64}; return c3;
          case 4:  static const std::vector<uint8_t> c4 = {40, 44, 47, 50, 53}; return c4;
          case 5:  static const std::vector<uint8_t> c5 = {41, 45, 48, 51, 54}; return c5;
          case 6:  static const std::vector<uint8_t> c6 = {42, 46, 49, 52, 55}; return c6;
          case 7:  static const std::vector<uint8_t> c7 = {43, 47, 50, 53, 56}; return c7;
          case 8:  static const std::vector<uint8_t> c8 = {44, 48, 51, 54, 57}; return c8;
          case 9:  static const std::vector<uint8_t> c9 = {45, 49, 52, 55, 58}; return c9;
          case 10: static const std::vector<uint8_t> c10 = {46, 50, 53, 56, 59}; return c10;
          default: static const std::vector<uint8_t> c11 = {47, 51, 54, 57, 60}; return c11;
        }
      case dominant7sharp9ChordType:
        switch (key) {
          case 0:  static const std::vector<uint8_t> c0 = {48, 52, 55, 58, 63}; return c0;
          case 1:  static const std::vector<uint8_t> c1 = {49, 53, 56, 59, 64}; return c1;
          case 2:  static const std::vector<uint8_t> c2 = {50, 54, 57, 60, 65}; return c2;
          case 3:  static const std::vector<uint8_t> c3 = {51, 55, 58, 61, 66}; return c3;
          case 4:  static const std::vector<uint8_t> c4 = {40, 44, 47, 50, 55}; return c4;
          case 5:  static const std::vector<uint8_t> c5 = {41, 45, 48, 51, 56}; return c5;
          case 6:  static const std::vector<uint8_t> c6 = {42, 46, 49, 52, 57}; return c6;
          case 7:  static const std::vector<uint8_t> c7 = {43, 47, 50, 53, 58}; return c7;
          case 8:  static const std::vector<uint8_t> c8 = {44, 48, 51, 54, 59}; return c8;
          case 9:  static const std::vector<uint8_t> c9 = {45, 49, 52, 55, 60}; return c9;
          case 10: static const std::vector<uint8_t> c10 = {46, 50, 53, 56, 61}; return c10;
          default: static const std::vector<uint8_t> c11 = {47, 51, 54, 57, 62}; return c11;
        }
      case dominant7b5ChordType:
        switch (key) 
        {
          case 0:  static const std::vector<uint8_t> c0 = {48, 52, 55, 59}; return c0;
          case 1:  static const std::vector<uint8_t> c1 = {49, 53, 56, 60}; return c1;
          case 2:  static const std::vector<uint8_t> c2 = {50, 54, 57, 61}; return c2;
          case 3:  static const std::vector<uint8_t> c3 = {51, 55, 58, 62}; return c3;
          case 4:  static const std::vector<uint8_t> c4 = {40, 44, 47, 51}; return c4;
          case 5:  static const std::vector<uint8_t> c5 = {41, 45, 48, 52}; return c5;
          case 6:  static const std::vector<uint8_t> c6 = {42, 46, 49, 53}; return c6;
          case 7:  static const std::vector<uint8_t> c7 = {43, 47, 50, 54}; return c7;
          case 8:  static const std::vector<uint8_t> c8 = {44, 48, 51, 55}; return c8;
          case 9:  static const std::vector<uint8_t> c9 = {45, 49, 52, 56}; return c9;
          case 10: static const std::vector<uint8_t> c10 = {46, 50, 53, 57}; return c10;
          default: static const std::vector<uint8_t> c11 = {47, 51, 54, 58}; return c11;
        }
      case dominant7sharp5ChordType:
        switch (key) 
        {
          case 0:  static const std::vector<uint8_t> c0 = {48, 52, 56, 58}; return c0;
          case 1:  static const std::vector<uint8_t> c1 = {49, 53, 57, 59}; return c1;
          case 2:  static const std::vector<uint8_t> c2 = {50, 54, 58, 60}; return c2;
          case 3:  static const std::vector<uint8_t> c3 = {51, 55, 59, 61}; return c3;
          case 4:  static const std::vector<uint8_t> c4 = {40, 44, 48, 50}; return c4;
          case 5:  static const std::vector<uint8_t> c5 = {41, 45, 49, 51}; return c5;
          case 6:  static const std::vector<uint8_t> c6 = {42, 46, 50, 52}; return c6;
          case 7:  static const std::vector<uint8_t> c7 = {43, 47, 51, 53}; return c7;
          case 8:  static const std::vector<uint8_t> c8 = {44, 48, 52, 54}; return c8;
          case 9:  static const std::vector<uint8_t> c9 = {45, 49, 53, 55}; return c9;
          case 10: static const std::vector<uint8_t> c10 = {46, 50, 54, 56}; return c10;
          default: static const std::vector<uint8_t> c11 = {47, 51, 55, 57}; return c11;
        }
      case minorMajor7ChordType:
        switch (key) 
        {
          case 0:  static const std::vector<uint8_t> c0 = {48, 51, 55, 59}; return c0;
          case 1:  static const std::vector<uint8_t> c1 = {49, 52, 56, 60}; return c1;
          case 2:  static const std::vector<uint8_t> c2 = {50, 53, 57, 61}; return c2;
          case 3:  static const std::vector<uint8_t> c3 = {51, 54, 58, 62}; return c3;
          case 4:  static const std::vector<uint8_t> c4 = {40, 43, 47, 51}; return c4;
          case 5:  static const std::vector<uint8_t> c5 = {41, 44, 48, 52}; return c5;
          case 6:  static const std::vector<uint8_t> c6 = {42, 45, 49, 53}; return c6;
          case 7:  static const std::vector<uint8_t> c7 = {43, 46, 50, 54}; return c7;
          case 8:  static const std::vector<uint8_t> c8 = {44, 47, 51, 55}; return c8;
          case 9:  static const std::vector<uint8_t> c9 = {45, 48, 52, 56}; return c9;
          case 10: static const std::vector<uint8_t> c10 = {46, 49, 53, 57}; return c10;
          default: static const std::vector<uint8_t> c11 = {47, 50, 54, 58}; return c11;
        }

      case major7b5ChordType:
        switch (key)
        {
          case 0:  static const std::vector<uint8_t> c0  = {48, 52, 54, 59}; return c0; // C E G# B
          case 1:  static const std::vector<uint8_t> c1  = {49, 53, 55, 60}; return c1;
          case 2:  static const std::vector<uint8_t> c2  = {50, 54, 56, 61}; return c2;
          case 3:  static const std::vector<uint8_t> c3  = {51, 55, 57, 62}; return c3;
          case 4:  static const std::vector<uint8_t> c4  = {40, 44, 46, 51}; return c4;
          case 5:  static const std::vector<uint8_t> c5  = {41, 45, 47, 52}; return c5;
          case 6:  static const std::vector<uint8_t> c6  = {42, 46, 48, 53}; return c6;
          case 7:  static const std::vector<uint8_t> c7  = {43, 47, 49, 54}; return c7;
          case 8:  static const std::vector<uint8_t> c8  = {44, 48, 50, 55}; return c8;
          case 9:  static const std::vector<uint8_t> c9  = {45, 49, 51, 56}; return c9;
          case 10: static const std::vector<uint8_t> c10 = {46, 50, 52, 57}; return c10;
          default: static const std::vector<uint8_t> c11 = {47, 51, 53, 58}; return c11;
        }

      case dim7ChordType:
        switch (key)
        {
          case 0:  static const std::vector<uint8_t> c0  = {48, 51, 54, 57}; return c0; // C Eb Gb A
          case 1:  static const std::vector<uint8_t> c1  = {49, 52, 55, 58}; return c1;
          case 2:  static const std::vector<uint8_t> c2  = {50, 53, 56, 59}; return c2;
          case 3:  static const std::vector<uint8_t> c3  = {51, 54, 57, 60}; return c3;
          case 4:  static const std::vector<uint8_t> c4  = {40, 43, 46, 49}; return c4;
          case 5:  static const std::vector<uint8_t> c5  = {41, 44, 47, 50}; return c5;
          case 6:  static const std::vector<uint8_t> c6  = {42, 45, 48, 51}; return c6;
          case 7:  static const std::vector<uint8_t> c7  = {43, 46, 49, 52}; return c7;
          case 8:  static const std::vector<uint8_t> c8  = {44, 47, 50, 53}; return c8;
          case 9:  static const std::vector<uint8_t> c9  = {45, 48, 51, 54}; return c9;
          case 10: static const std::vector<uint8_t> c10 = {46, 49, 52, 55}; return c10;
          default: static const std::vector<uint8_t> c11 = {47, 50, 53, 56}; return c11;
        }
  case powerChordType:
    switch (key)
    {
      case 0:  static const std::vector<uint8_t> c0  = {48, 55}; return c0; // C5
      case 1:  static const std::vector<uint8_t> c1  = {49, 56}; return c1;
      case 2:  static const std::vector<uint8_t> c2  = {50, 57}; return c2;
      case 3:  static const std::vector<uint8_t> c3  = {51, 58}; return c3;
      case 4:  static const std::vector<uint8_t> c4  = {40, 47}; return c4;
      case 5:  static const std::vector<uint8_t> c5  = {41, 48}; return c5;
      case 6:  static const std::vector<uint8_t> c6  = {42, 49}; return c6;
      case 7:  static const std::vector<uint8_t> c7  = {43, 50}; return c7;
      case 8:  static const std::vector<uint8_t> c8  = {44, 51}; return c8;
      case 9:  static const std::vector<uint8_t> c9  = {45, 52}; return c9;
      case 10: static const std::vector<uint8_t> c10 = {46, 53}; return c10;
      default: static const std::vector<uint8_t> c11 = {47, 54}; return c11;
    }
  case dominant13b9ChordType:
    switch (key)
    {
      case 0:  static const std::vector<uint8_t> c0  = {48, 52, 58, 61, 65, 69}; return c0; // C E♭ F# Bb A
      case 1:  static const std::vector<uint8_t> c1  = {49, 53, 59, 62, 66, 70}; return c1;
      case 2:  static const std::vector<uint8_t> c2  = {50, 54, 60, 63, 67, 71}; return c2;
      case 3:  static const std::vector<uint8_t> c3  = {51, 55, 61, 64, 68, 72}; return c3;
      case 4:  static const std::vector<uint8_t> c4  = {40, 44, 50, 52, 56, 60}; return c4;
      case 5:  static const std::vector<uint8_t> c5  = {41, 45, 51, 53, 57, 61}; return c5;
      case 6:  static const std::vector<uint8_t> c6  = {42, 46, 52, 54, 58, 62}; return c6;
      case 7:  static const std::vector<uint8_t> c7  = {43, 47, 53, 55, 59, 63}; return c7;
      case 8:  static const std::vector<uint8_t> c8  = {44, 48, 54, 56, 60, 64}; return c8;
      case 9:  static const std::vector<uint8_t> c9  = {45, 49, 55, 57, 61, 65}; return c9;
      case 10: static const std::vector<uint8_t> c10 = {46, 50, 56, 58, 62, 66}; return c10;
      default: static const std::vector<uint8_t> c11 = {47, 51, 57, 59, 63, 67}; return c11;
    }
  case dominant7sus4ChordType:
    switch (key)
    {
      case 0:  static const std::vector<uint8_t> c0  = {48, 53, 55, 58}; return c0; // C F G Bb
      case 1:  static const std::vector<uint8_t> c1  = {49, 54, 56, 59}; return c1;
      case 2:  static const std::vector<uint8_t> c2  = {50, 55, 57, 60}; return c2;
      case 3:  static const std::vector<uint8_t> c3  = {51, 56, 58, 61}; return c3;
      case 4:  static const std::vector<uint8_t> c4  = {40, 45, 47, 50}; return c4;
      case 5:  static const std::vector<uint8_t> c5  = {41, 46, 48, 51}; return c5;
      case 6:  static const std::vector<uint8_t> c6  = {42, 47, 49, 52}; return c6;
      case 7:  static const std::vector<uint8_t> c7  = {43, 48, 50, 53}; return c7;
      case 8:  static const std::vector<uint8_t> c8  = {44, 49, 51, 54}; return c8;
      case 9:  static const std::vector<uint8_t> c9  = {45, 50, 52, 55}; return c9;
      case 10: static const std::vector<uint8_t> c10 = {46, 51, 53, 56}; return c10;
      default: static const std::vector<uint8_t> c11 = {47, 52, 54, 57}; return c11;
    }

  case quartalChordType:
    switch (key)
    {
      case 0:  static const std::vector<uint8_t> c0  = {48, 53, 58, 63, 68}; return c0; // C F Bb
      case 1:  static const std::vector<uint8_t> c1  = {49, 54, 59, 64, 69}; return c1;
      case 2:  static const std::vector<uint8_t> c2  = {50, 55, 60, 65, 70}; return c2;
      case 3:  static const std::vector<uint8_t> c3  = {51, 56, 61, 66, 71}; return c3;
      case 4:  static const std::vector<uint8_t> c4  = {40, 45, 50, 55, 60}; return c4;
      case 5:  static const std::vector<uint8_t> c5  = {41, 46, 51, 56, 61}; return c5;
      case 6:  static const std::vector<uint8_t> c6  = {42, 47, 52, 57, 62}; return c6;
      case 7:  static const std::vector<uint8_t> c7  = {43, 48, 53, 58, 63}; return c7;
      case 8:  static const std::vector<uint8_t> c8  = {44, 49, 54, 59, 64}; return c8;
      case 9:  static const std::vector<uint8_t> c9  = {45, 50, 55, 60, 65}; return c9;
      case 10: static const std::vector<uint8_t> c10 = {46, 51, 56, 61, 66}; return c10;
      default: static const std::vector<uint8_t> c11 = {47, 52, 57, 62, 67}; return c11;
    }
  //case clusterChordType:
  default:
    switch (key)
    {
      case 0:  static const std::vector<uint8_t> c0  = {48, 49, 50}; return c0; // C, C#, D
      case 1:  static const std::vector<uint8_t> c1  = {49, 50, 51}; return c1;
      case 2:  static const std::vector<uint8_t> c2  = {50, 51, 52}; return c2;
      case 3:  static const std::vector<uint8_t> c3  = {51, 52, 53}; return c3;
      case 4:  static const std::vector<uint8_t> c4  = {40, 41, 42}; return c4;
      case 5:  static const std::vector<uint8_t> c5  = {41, 42, 43}; return c5;
      case 6:  static const std::vector<uint8_t> c6  = {42, 43, 44}; return c6;
      case 7:  static const std::vector<uint8_t> c7  = {43, 44, 45}; return c7;
      case 8:  static const std::vector<uint8_t> c8  = {44, 45, 46}; return c8;
      case 9:  static const std::vector<uint8_t> c9  = {45, 46, 47}; return c9;
      case 10: static const std::vector<uint8_t> c10 = {46, 47, 48}; return c10;
      default: static const std::vector<uint8_t> c11 = {47, 48, 49}; return c11;
    }
  }
}

const char* hexToOctave(int index, size_t& length) {
  switch (index) {
    case 0: { //off
      static constexpr char data[] = { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x12, 0x01 };
      length = sizeof(data);
      return data;
    }
    case 1: {
      static constexpr char data[] = { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x12, 0x02 };
      length = sizeof(data);
      return data;
    }
    case 2: {
      static constexpr char data[] = { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x12, 0x03 };
      length = sizeof(data);
      return data;
    }
    case 3: {
      static constexpr char data[] = { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x12, 0x04 };
      length = sizeof(data);
      return data;
    }
    case 4: {
      static constexpr char data[] = { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x12, 0x05 };
      length = sizeof(data);
      return data;
    }    
    default: {
      static constexpr char data[] = { 0xF5, 0x55, 0x00, 0x03, 0x00, 0x12, 0x06 };
      length = sizeof(data);
      return data;
    }
  }
}
uint8_t getCyberGCapo(uint8_t val)
{
  switch (val)
  {
    case 0x35:
      return 0;
    case 0x34:
      return 1;
    case 0x33:
      return 2;
    case 0x32:
      return 3;
    case 0x3D:
      return 4;
    case 0x3C:
      return 5;
    case 0x3B:
      return 6;
    case 0x3A:
      return 7;
    case 0x39:
      return 8;      
    case 0x38:
      return 9;            
    case 0x37:
      return 10;                  
    default: //0x36
      return 11;
  }
  
}
const MidiMessage hexToNote(uint8_t index)
{
  switch (index)
  {
    case 0:  return { "aa55000a22010000000400", 0 };
    case 1:  return { "aa55000a22010000000200", 1 };
    case 2:  return { "aa55000a22010000000100", 2 };
    case 3:  return { "aa55000a22010000002000", 3 };
    case 4:  return { "aa55000a22010000001000", 4 };
    case 5:  return { "aa55000a22010000000800", 5 };
    case 6:  return { "aa55000a22010000010000", 6 };
    case 7:  return { "aa55000a22010000008000", 7 };
    case 8:  return { "aa55000a22010000004000", 8 };
    case 9:  return { "aa55000a22010000080000", 9 };
    case 10: return { "aa55000a22010000040000", 10 };
    case 11: return { "aa55000a22010000020000", 11 };
    case 12: return { "aa55000a22010000400000", 12 };
    case 13: return { "aa55000a22010000200000", 13 };
    case 14: return { "aa55000a22010000100000", 14 };
    case 15: return { "aa55000a22010002000000", 15 };
    case 16: return { "aa55000a22010001000000", 16 };
    case 17: return { "aa55000a22010000800000", 17 };
    case 18: return { "aa55000a22010010000000", 18 };
    case 19: return { "aa55000a22010008000000", 19 };
    case 20: return { "aa55000a22010004000000", 20 };
    case 21: return { "aa55000a22010080000000", 21 };
    case 22: return { "aa55000a22010040000000", 22 };
    case 23: return { "aa55000a22010020000000", 23 };
    case 24: return { "aa55000a22010400000000", 24 };
    case 25: return { "aa55000a22010200000000", 25 };
    case 26: return { "aa55000a22010100000000", 26 };
    default: return { "aa55000a22010000000000", 255 }; // note off
  }
}
const HexToProgram hexToProgram(uint8_t index)
{
  switch (index)
  {
    case 0:  return { "aa550003220200d8", 3 };
    default: return { "aa5500032202ffd9", 9 }; // note off
  }
}
const int hexToProgramBytes(uint8_t index, uint8_t *ptr, uint8_t &len) {
  // Each message is 8 bytes
  
  if (lastb1 > (lastb1+2))
  {
    lastb0++;
  }
  lastb1 +=2;
  Serial.printf("Last %x %x\n", lastb0, lastb1);

  //const uint8_t msg0[] = { 0xAA, 0x55, 0x00, 0x03, 0x22, 0x02, 0x00, 0xD8,lastb0,lastb1,0x00,0x00 }; //up
  //const uint8_t msg1[] = { 0xAA, 0x55, 0x00, 0x03, 0x22, 0x02, 0xFF, 0xD9,lastb0,lastb1,0x00,0x00 }; //down
  const uint8_t msg1[] = { 0xaa, 0x55, 0x00, 0x0a, 0x22, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,lastb0,lastb1,0x00,0x00 }; //no button
  const uint8_t msg2[] = { 0xaa, 0x55, 0x00, 0x0a, 0x22, 0x01, 0x00, 0x20, 0x00, 0x00, 0x00,lastb0,lastb1,0x00,0x00 }; //octave up
  const uint8_t msg3[] = { 0xaa, 0x55, 0x00, 0x0a, 0x22, 0x01, 0x00, 0x00, 0x00, 0x40, 0x00,lastb0,lastb1,0x00,0x00 }; //00
  const uint8_t msg4[] = { 0xaa, 0x55, 0x00, 0x0a, 0x22, 0x01, 0x00, 0x80, 0x00, 0x00, 0x00 ,lastb0,lastb1,0x00,0x00 }; //octave down

                                                                                       
  switch (index) {
    case 0: 
      len = sizeof(msg1);
      ptr = (uint8_t *)malloc(len);
      for (int i = 0; i < len; i++)
      {
        ptr[i] = msg1[i];
      }
      return 1;
    case 1: 
          len = sizeof(msg2)/sizeof(uint8_t);
      ptr = (uint8_t *)malloc(sizeof(msg2));
      for (int i = 0; i < len; i++)
      {
        ptr[i] = msg2[i];
      }
      return 1;
    case 2: len = sizeof(msg3)/sizeof(uint8_t);
      ptr = (uint8_t *)malloc(sizeof(msg3));
      for (int i = 0; i < len; i++)
      {
        ptr[i] = msg3[i];
      }
      return 1;
    default: len = sizeof(msg4)/sizeof(uint8_t);
      ptr = (uint8_t *)malloc(sizeof(msg4));
      for (int i = 0; i < len; i++)
      {
        ptr[i] = msg4[i];
      }
      return 1;

  }
}

const HexToControl hexToControl(uint8_t index)
{
  switch (index)
  {
    case 0:  return { "f5550003201401", 2 };  // right
    case 1:  return { "f5550003201402", 1 };  // Left
    case 2:  return { "f5550003201403", 5 };  // Both
    case 3:  return { "f5550003201000", 4 };  // Circle not Lit
    case 4:  return { "f5550003201001", 3 };  // Circle Lit
    default:  return { "f5550003201400", 0 };  //ignore
  }
}

const char* chordTypeToString(ChordType chord) {
    switch (chord) {
        case majorChordType: return "majorChordType";
        case minorChordType: return "minorChordType";
        case diminishedChordType: return "diminishedChordType";
        case augmentedChordType: return "augmentedChordType";
        case major7thChordType: return "major7thChordType";
        case minor7thChordType: return "minor7thChordType";
        case dominant7thChordType: return "dominant7thChordType";
        case minor7Flat5ChordType: return "minor7Flat5ChordType";
        case major6thChordType: return "major6thChordType";
        case minor6thChordType: return "minor6thChordType";
        case suspended2ChordType: return "suspended2ChordType";
        case suspended4ChordType: return "suspended4ChordType";
        case add9ChordType: return "add9ChordType";
        case add11ChordType: return "add11ChordType";
        case ninthChordType: return "ninthChordType";
        case eleventhChordType: return "eleventhChordType";
        case thirteenthChordType: return "thirteenthChordType";
        case majorAdd9ChordType: return "majorAdd9ChordType";
        case dominant7b9ChordType: return "dominant7b9ChordType";
        case dominant7sharp9ChordType: return "dominant7sharp9ChordType";
        case dominant7b5ChordType: return "dominant7b5ChordType";
        case dominant7sharp5ChordType: return "dominant7sharp5ChordType";
        case minorMajor7ChordType: return "minorMajor7ChordType";
        case major7b5ChordType: return "major7b5ChordType";
        case dim7ChordType: return "dim7ChordType";
        case powerChordType: return "powerChordType";
        case dominant13b9ChordType: return "dominant13b9ChordType";
        case dominant7sus4ChordType: return "dominant7sus4ChordType";
        case quartalChordType: return "quartalChordType";
        case clusterChordType: return "clusterChordType";
        default: return "UnknownChordType";
    }
}
