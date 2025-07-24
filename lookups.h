#pragma once
#include "helperClasses.h"
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
    default:         return Chord({ 1, 2});         // dense cluster: 1 b9 9

  }
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
        case 4:  static const std::vector<uint8_t> c4 = { 52, 56, 60, 64, 68 }; return c4;
        case 5:  static const std::vector<uint8_t> c5 = { 53, 57, 61, 65, 69 }; return c5;
        case 6:  static const std::vector<uint8_t> c6 = { 54, 58, 62, 66, 70 }; return c6;
        case 7:  static const std::vector<uint8_t> c7 = { 55, 59, 63, 67, 71 }; return c7;
        case 8:  static const std::vector<uint8_t> c8 = { 56, 60, 64, 68, 72 }; return c8;
        case 9:  static const std::vector<uint8_t> c9 = { 57, 61, 65, 69, 73 }; return c9;
        case 10: static const std::vector<uint8_t> c10 = { 58, 62, 66, 70, 74 }; return c10;
        default: static const std::vector<uint8_t> c11 = { 59, 63, 67, 71, 75 }; return c11;
      }
    case major7thChordType:
      switch (key)
      {
        case 0:  static const std::vector<uint8_t> c0 = { 48, 52, 55, 71, 64 }; return c0;
        case 1:  static const std::vector<uint8_t> c1 = { 49, 53, 56, 72, 65 }; return c1;
        case 2:  static const std::vector<uint8_t> c2 = { 50, 54, 57, 73, 66 }; return c2;
        case 3:  static const std::vector<uint8_t> c3 = { 51, 55, 58, 74, 67 }; return c3;
        case 4:  static const std::vector<uint8_t> c4 = { 52, 56, 59, 75, 68 }; return c4;
        case 5:  static const std::vector<uint8_t> c5 = { 53, 57, 60, 76, 69 }; return c5;
        case 6:  static const std::vector<uint8_t> c6 = { 54, 58, 61, 77, 70 }; return c6;
        case 7:  static const std::vector<uint8_t> c7 = { 55, 59, 62, 78, 71 }; return c7;
        case 8:  static const std::vector<uint8_t> c8 = { 56, 60, 63, 79, 72 }; return c8;
        case 9:  static const std::vector<uint8_t> c9 = { 57, 61, 64, 80, 73 }; return c9;
        case 10: static const std::vector<uint8_t> c10 = { 58, 62, 65, 81, 74 }; return c10;
        default: static const std::vector<uint8_t> c11 = { 59, 63, 66, 82, 75 }; return c11;
      }

    case minor7thChordType:
      switch (key)
      {
        case 0:  static const std::vector<uint8_t> c0 = { 48, 55, 58, 63, 67 }; return c0;
        case 1:  static const std::vector<uint8_t> c1 = { 49, 56, 59, 64, 68 }; return c1;
        case 2:  static const std::vector<uint8_t> c2 = { 50, 57, 60, 65, 69 }; return c2;
        case 3:  static const std::vector<uint8_t> c3 = { 51, 58, 61, 66, 70 }; return c3;
        case 4:  static const std::vector<uint8_t> c4 = { 52, 59, 62, 67, 71 }; return c4;
        case 5:  static const std::vector<uint8_t> c5 = { 53, 60, 63, 68, 72 }; return c5;
        case 6:  static const std::vector<uint8_t> c6 = { 54, 61, 64, 69, 73 }; return c6;
        case 7:  static const std::vector<uint8_t> c7 = { 55, 62, 65, 70, 74 }; return c7;
        case 8:  static const std::vector<uint8_t> c8 = { 56, 63, 66, 71, 75 }; return c8;
        case 9:  static const std::vector<uint8_t> c9 = { 57, 64, 67, 72, 76 }; return c9;
        case 10: static const std::vector<uint8_t> c10 = { 58, 65, 68, 73, 77 }; return c10;
        default: static const std::vector<uint8_t> c11 = { 59, 66, 69, 74, 78 }; return c11;
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
          case 4:  static const std::vector<uint8_t> c4  = { 52, 55, 62, 64, 70 }; return c4;
          case 5:  static const std::vector<uint8_t> c5  = { 53, 56, 63, 65, 71 }; return c5;
          case 6:  static const std::vector<uint8_t> c6  = { 54, 57, 64, 66, 72 }; return c6;
          case 7:  static const std::vector<uint8_t> c7  = { 55, 58, 65, 67, 73 }; return c7;
          case 8:  static const std::vector<uint8_t> c8  = { 56, 59, 66, 68, 74 }; return c8;
          case 9:  static const std::vector<uint8_t> c9  = { 57, 60, 67, 69, 75 }; return c9;
          case 10: static const std::vector<uint8_t> c10 = { 58, 61, 68, 70, 76 }; return c10;
          default: static const std::vector<uint8_t> c11 = { 59, 62, 69, 71, 77 }; return c11;
        }

      case major6thChordType:
        switch (key)
        {
          case 0:  static const std::vector<uint8_t> c0  = { 48, 52, 57, 60, 64 }; return c0;
          case 1:  static const std::vector<uint8_t> c1  = { 49, 53, 58, 61, 65 }; return c1;
          case 2:  static const std::vector<uint8_t> c2  = { 50, 54, 59, 62, 66 }; return c2;
          case 3:  static const std::vector<uint8_t> c3  = { 51, 55, 60, 63, 67 }; return c3;
          case 4:  static const std::vector<uint8_t> c4  = { 52, 56, 61, 64, 68 }; return c4;
          case 5:  static const std::vector<uint8_t> c5  = { 53, 57, 62, 65, 69 }; return c5;
          case 6:  static const std::vector<uint8_t> c6  = { 54, 58, 63, 66, 70 }; return c6;
          case 7:  static const std::vector<uint8_t> c7  = { 55, 59, 64, 67, 71 }; return c7;
          case 8:  static const std::vector<uint8_t> c8  = { 56, 60, 65, 68, 72 }; return c8;
          case 9:  static const std::vector<uint8_t> c9  = { 57, 61, 66, 69, 73 }; return c9;
          case 10: static const std::vector<uint8_t> c10 = { 58, 62, 67, 70, 74 }; return c10;
          default: static const std::vector<uint8_t> c11 = { 59, 63, 68, 71, 75 }; return c11;
        }

      case minor6thChordType:
        switch (key)
        {
          case 0:  static const std::vector<uint8_t> c0  = { 48, 51, 57, 60, 67 }; return c0;
          case 1:  static const std::vector<uint8_t> c1  = { 49, 52, 58, 61, 68 }; return c1;
          case 2:  static const std::vector<uint8_t> c2  = { 50, 53, 59, 62, 69 }; return c2;
          case 3:  static const std::vector<uint8_t> c3  = { 51, 54, 60, 63, 70 }; return c3;
          case 4:  static const std::vector<uint8_t> c4  = { 52, 55, 61, 64, 71 }; return c4;
          case 5:  static const std::vector<uint8_t> c5  = { 53, 56, 62, 65, 72 }; return c5;
          case 6:  static const std::vector<uint8_t> c6  = { 54, 57, 63, 66, 73 }; return c6;
          case 7:  static const std::vector<uint8_t> c7  = { 55, 58, 64, 67, 74 }; return c7;
          case 8:  static const std::vector<uint8_t> c8  = { 56, 59, 65, 68, 75 }; return c8;
          case 9:  static const std::vector<uint8_t> c9  = { 57, 60, 66, 69, 76 }; return c9;
          case 10: static const std::vector<uint8_t> c10 = { 58, 61, 67, 70, 77 }; return c10;
          default: static const std::vector<uint8_t> c11 = { 59, 62, 68, 71, 78 }; return c11;
        }

      case suspended2ChordType:
        switch (key)
        {
          case 0:  static const std::vector<uint8_t> c0  = { 48, 50, 55, 62, 67 }; return c0;
          case 1:  static const std::vector<uint8_t> c1  = { 49, 51, 56, 63, 68 }; return c1;
          case 2:  static const std::vector<uint8_t> c2  = { 50, 52, 57, 64, 69 }; return c2;
          case 3:  static const std::vector<uint8_t> c3  = { 51, 53, 58, 65, 70 }; return c3;
          case 4:  static const std::vector<uint8_t> c4  = { 52, 54, 59, 66, 71 }; return c4;
          case 5:  static const std::vector<uint8_t> c5  = { 53, 55, 60, 67, 72 }; return c5;
          case 6:  static const std::vector<uint8_t> c6  = { 54, 56, 61, 68, 73 }; return c6;
          case 7:  static const std::vector<uint8_t> c7  = { 55, 57, 62, 69, 74 }; return c7;
          case 8:  static const std::vector<uint8_t> c8  = { 56, 58, 63, 70, 75 }; return c8;
          case 9:  static const std::vector<uint8_t> c9  = { 57, 59, 64, 71, 76 }; return c9;
          case 10: static const std::vector<uint8_t> c10 = { 58, 60, 65, 72, 77 }; return c10;
          default: static const std::vector<uint8_t> c11 = { 59, 61, 66, 73, 78 }; return c11;
        }

    case suspended4ChordType:
      switch (key)
      {
        case 0:  static const std::vector<uint8_t> c0  = { 48, 53, 55, 60, 65 }; return c0;
        case 1:  static const std::vector<uint8_t> c1  = { 49, 56, 61, 66, 68 }; return c1;
        case 2:  static const std::vector<uint8_t> c2  = { 50, 57, 62, 67 };     return c2;
        case 3:  static const std::vector<uint8_t> c3  = { 51, 58, 63, 68, 70 }; return c3;
        case 4:  static const std::vector<uint8_t> c4  = { 40, 47, 52, 57, 59, 64 }; return c4;
        case 5:  static const std::vector<uint8_t> c5  = { 53, 60, 65, 70, 72 }; return c5;
        case 6:  static const std::vector<uint8_t> c6  = { 54, 61, 66, 71, 73 }; return c6;
        case 7:  static const std::vector<uint8_t> c7  = { 43, 50, 55, 60, 67 }; return c7;
        case 8:  static const std::vector<uint8_t> c8  = { 56, 63, 68, 73, 75 }; return c8;
        case 9:  static const std::vector<uint8_t> c9  = { 45, 52, 57, 62, 64 }; return c9;
        case 10: static const std::vector<uint8_t> c10 = { 46, 53, 58, 63, 65 }; return c10;
        default: static const std::vector<uint8_t> c11 = { 47, 54, 59, 64 };     return c11;
      }

      case add9ChordType:
        switch (key)
        {
          case 0:  static const std::vector<uint8_t> c0  = { 48, 52, 55, 62, 64 }; return c0;
          case 1:  static const std::vector<uint8_t> c1  = { 49, 53, 56, 63, 65 }; return c1;
          case 2:  static const std::vector<uint8_t> c2  = { 50, 54, 57, 64, 66 }; return c2;
          case 3:  static const std::vector<uint8_t> c3  = { 51, 55, 58, 65, 67 }; return c3;
          case 4:  static const std::vector<uint8_t> c4  = { 52, 56, 59, 66, 68 }; return c4;
          case 5:  static const std::vector<uint8_t> c5  = { 53, 57, 60, 67, 69 }; return c5;
          case 6:  static const std::vector<uint8_t> c6  = { 54, 58, 61, 68, 70 }; return c6;
          case 7:  static const std::vector<uint8_t> c7  = { 55, 59, 62, 69, 71 }; return c7;
          case 8:  static const std::vector<uint8_t> c8  = { 56, 60, 63, 70, 72 }; return c8;
          case 9:  static const std::vector<uint8_t> c9  = { 57, 61, 64, 71, 73 }; return c9;
          case 10: static const std::vector<uint8_t> c10 = { 58, 62, 65, 72, 74 }; return c10;
          default: static const std::vector<uint8_t> c11 = { 59, 63, 66, 73, 75 }; return c11;
        }

      case add11ChordType:
        switch (key)
        {
          case 0:  static const std::vector<uint8_t> c0  = { 48, 52, 55, 60, 65 }; return c0;
          case 1:  static const std::vector<uint8_t> c1  = { 50, 54, 57, 62, 67 }; return c1;
          case 2:  static const std::vector<uint8_t> c2  = { 51, 55, 58, 63, 68 }; return c2;
          case 3:  static const std::vector<uint8_t> c3  = { 52, 56, 59, 64, 69 }; return c3;
          case 4:  static const std::vector<uint8_t> c4  = { 53, 57, 60, 65, 70 }; return c4;
          case 5:  static const std::vector<uint8_t> c5  = { 54, 58, 61, 66, 71 }; return c5;
          case 6:  static const std::vector<uint8_t> c6  = { 55, 59, 62, 67, 72 }; return c6;
          case 7:  static const std::vector<uint8_t> c7  = { 56, 60, 63, 68, 73 }; return c7;
          case 8:  static const std::vector<uint8_t> c8  = { 57, 61, 64, 69, 74 }; return c8;
          case 9:  static const std::vector<uint8_t> c9  = { 58, 62, 65, 70, 75 }; return c9;
          case 10: static const std::vector<uint8_t> c10 = { 59, 63, 66, 71, 76 }; return c10;
          default: static const std::vector<uint8_t> c11 = { 60, 64, 67, 72, 77 }; return c11;
        }

      case ninthChordType:
        switch (key)
        {
          case 0:  static const std::vector<uint8_t> c0  = { 48, 55, 58, 64, 70, 74 }; return c0;
          case 1:  static const std::vector<uint8_t> c1  = { 49, 56, 59, 65, 71, 75 }; return c1;
          case 2:  static const std::vector<uint8_t> c2  = { 50, 57, 60, 66, 72, 76 }; return c2;
          case 3:  static const std::vector<uint8_t> c3  = { 51, 58, 61, 67, 73, 77 }; return c3;
          case 4:  static const std::vector<uint8_t> c4  = { 52, 59, 62, 68, 74, 78 }; return c4;
          case 5:  static const std::vector<uint8_t> c5  = { 53, 60, 63, 69, 75, 79 }; return c5;
          case 6:  static const std::vector<uint8_t> c6  = { 54, 61, 64, 70, 76, 80 }; return c6;
          case 7:  static const std::vector<uint8_t> c7  = { 55, 62, 65, 71, 77, 81 }; return c7;
          case 8:  static const std::vector<uint8_t> c8  = { 56, 63, 66, 72, 78, 82 }; return c8;
          case 9:  static const std::vector<uint8_t> c9  = { 57, 64, 67, 73, 79, 83 }; return c9;
          case 10: static const std::vector<uint8_t> c10 = { 58, 65, 68, 74, 80, 84 }; return c10;
          default: static const std::vector<uint8_t> c11 = { 59, 66, 69, 75, 81, 85 }; return c11;
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
          case 4:  static const std::vector<uint8_t> c4 = {52, 56, 59, 63, 66}; return c4;
          case 5:  static const std::vector<uint8_t> c5 = {53, 57, 60, 64, 67}; return c5;
          case 6:  static const std::vector<uint8_t> c6 = {54, 58, 61, 65, 68}; return c6;
          case 7:  static const std::vector<uint8_t> c7 = {55, 59, 62, 66, 69}; return c7;
          case 8:  static const std::vector<uint8_t> c8 = {56, 60, 63, 67, 70}; return c8;
          case 9:  static const std::vector<uint8_t> c9 = {57, 61, 64, 68, 71}; return c9;
          case 10: static const std::vector<uint8_t> c10 = {58, 62, 65, 69, 72}; return c10;
          default: static const std::vector<uint8_t> c11 = {59, 63, 66, 70, 73}; return c11;
        }
      case dominant7b9ChordType:
        switch (key) 
        {
          case 0:  static const std::vector<uint8_t> c0 = {48, 52, 55, 58, 63}; return c0;
          case 1:  static const std::vector<uint8_t> c1 = {49, 53, 56, 59, 64}; return c1;
          case 2:  static const std::vector<uint8_t> c2 = {50, 54, 57, 60, 65}; return c2;
          case 3:  static const std::vector<uint8_t> c3 = {51, 55, 58, 61, 66}; return c3;
          case 4:  static const std::vector<uint8_t> c4 = {52, 56, 59, 62, 67}; return c4;
          case 5:  static const std::vector<uint8_t> c5 = {53, 57, 60, 63, 68}; return c5;
          case 6:  static const std::vector<uint8_t> c6 = {54, 58, 61, 64, 69}; return c6;
          case 7:  static const std::vector<uint8_t> c7 = {55, 59, 62, 65, 70}; return c7;
          case 8:  static const std::vector<uint8_t> c8 = {56, 60, 63, 66, 71}; return c8;
          case 9:  static const std::vector<uint8_t> c9 = {57, 61, 64, 67, 72}; return c9;
          case 10: static const std::vector<uint8_t> c10 = {58, 62, 65, 68, 73}; return c10;
          default: static const std::vector<uint8_t> c11 = {59, 63, 66, 69, 74}; return c11;
        }
      case dominant7sharp9ChordType:
        switch (key) {
          case 0:  static const std::vector<uint8_t> c0 = {48, 52, 55, 58, 65}; return c0;
          case 1:  static const std::vector<uint8_t> c1 = {49, 53, 56, 59, 66}; return c1;
          case 2:  static const std::vector<uint8_t> c2 = {50, 54, 57, 60, 67}; return c2;
          case 3:  static const std::vector<uint8_t> c3 = {51, 55, 58, 61, 68}; return c3;
          case 4:  static const std::vector<uint8_t> c4 = {52, 56, 59, 62, 69}; return c4;
          case 5:  static const std::vector<uint8_t> c5 = {53, 57, 60, 63, 70}; return c5;
          case 6:  static const std::vector<uint8_t> c6 = {54, 58, 61, 64, 71}; return c6;
          case 7:  static const std::vector<uint8_t> c7 = {55, 59, 62, 65, 72}; return c7;
          case 8:  static const std::vector<uint8_t> c8 = {56, 60, 63, 66, 73}; return c8;
          case 9:  static const std::vector<uint8_t> c9 = {57, 61, 64, 67, 74}; return c9;
          case 10: static const std::vector<uint8_t> c10 = {58, 62, 65, 68, 75}; return c10;
          default: static const std::vector<uint8_t> c11 = {59, 63, 66, 69, 76}; return c11;
        }
      case dominant7b5ChordType:
        switch (key) 
        {
          case 0:  static const std::vector<uint8_t> c0 = {48, 52, 55, 59}; return c0;
          case 1:  static const std::vector<uint8_t> c1 = {49, 53, 56, 60}; return c1;
          case 2:  static const std::vector<uint8_t> c2 = {50, 54, 57, 61}; return c2;
          case 3:  static const std::vector<uint8_t> c3 = {51, 55, 58, 62}; return c3;
          case 4:  static const std::vector<uint8_t> c4 = {52, 56, 59, 63}; return c4;
          case 5:  static const std::vector<uint8_t> c5 = {53, 57, 60, 64}; return c5;
          case 6:  static const std::vector<uint8_t> c6 = {54, 58, 61, 65}; return c6;
          case 7:  static const std::vector<uint8_t> c7 = {55, 59, 62, 66}; return c7;
          case 8:  static const std::vector<uint8_t> c8 = {56, 60, 63, 67}; return c8;
          case 9:  static const std::vector<uint8_t> c9 = {57, 61, 64, 68}; return c9;
          case 10: static const std::vector<uint8_t> c10 = {58, 62, 65, 69}; return c10;
          default: static const std::vector<uint8_t> c11 = {59, 63, 66, 70}; return c11;
        }
      case dominant7sharp5ChordType:
        switch (key) 
        {
          case 0:  static const std::vector<uint8_t> c0 = {48, 52, 56, 58}; return c0;
          case 1:  static const std::vector<uint8_t> c1 = {49, 53, 57, 59}; return c1;
          case 2:  static const std::vector<uint8_t> c2 = {50, 54, 58, 60}; return c2;
          case 3:  static const std::vector<uint8_t> c3 = {51, 55, 59, 61}; return c3;
          case 4:  static const std::vector<uint8_t> c4 = {52, 56, 60, 62}; return c4;
          case 5:  static const std::vector<uint8_t> c5 = {53, 57, 61, 63}; return c5;
          case 6:  static const std::vector<uint8_t> c6 = {54, 58, 62, 64}; return c6;
          case 7:  static const std::vector<uint8_t> c7 = {55, 59, 63, 65}; return c7;
          case 8:  static const std::vector<uint8_t> c8 = {56, 60, 64, 66}; return c8;
          case 9:  static const std::vector<uint8_t> c9 = {57, 61, 65, 67}; return c9;
          case 10: static const std::vector<uint8_t> c10 = {58, 62, 66, 68}; return c10;
          default: static const std::vector<uint8_t> c11 = {59, 63, 67, 69}; return c11;
        }
      case minorMajor7ChordType:
        switch (key) 
        {
          case 0:  static const std::vector<uint8_t> c0 = {48, 51, 55, 59}; return c0;
          case 1:  static const std::vector<uint8_t> c1 = {49, 52, 56, 60}; return c1;
          case 2:  static const std::vector<uint8_t> c2 = {50, 53, 57, 61}; return c2;
          case 3:  static const std::vector<uint8_t> c3 = {51, 54, 58, 62}; return c3;
          case 4:  static const std::vector<uint8_t> c4 = {52, 55, 59, 63}; return c4;
          case 5:  static const std::vector<uint8_t> c5 = {53, 56, 60, 64}; return c5;
          case 6:  static const std::vector<uint8_t> c6 = {54, 57, 61, 65}; return c6;
          case 7:  static const std::vector<uint8_t> c7 = {55, 58, 62, 66}; return c7;
          case 8:  static const std::vector<uint8_t> c8 = {56, 59, 63, 67}; return c8;
          case 9:  static const std::vector<uint8_t> c9 = {57, 60, 64, 68}; return c9;
          case 10: static const std::vector<uint8_t> c10 = {58, 61, 65, 69}; return c10;
          default: static const std::vector<uint8_t> c11 = {59, 62, 66, 70}; return c11;
        }

      case major7b5ChordType:
    switch (key)
    {
      case 0:  static const std::vector<uint8_t> c0  = {48, 52, 54, 59}; return c0; // C E G# B
      case 1:  static const std::vector<uint8_t> c1  = {49, 53, 55, 60}; return c1;
      case 2:  static const std::vector<uint8_t> c2  = {50, 54, 56, 61}; return c2;
      case 3:  static const std::vector<uint8_t> c3  = {51, 55, 57, 62}; return c3;
      case 4:  static const std::vector<uint8_t> c4  = {52, 56, 58, 63}; return c4;
      case 5:  static const std::vector<uint8_t> c5  = {53, 57, 59, 64}; return c5;
      case 6:  static const std::vector<uint8_t> c6  = {54, 58, 60, 65}; return c6;
      case 7:  static const std::vector<uint8_t> c7  = {55, 59, 61, 66}; return c7;
      case 8:  static const std::vector<uint8_t> c8  = {56, 60, 62, 67}; return c8;
      case 9:  static const std::vector<uint8_t> c9  = {57, 61, 63, 68}; return c9;
      case 10: static const std::vector<uint8_t> c10 = {58, 62, 64, 69}; return c10;
      default: static const std::vector<uint8_t> c11 = {59, 63, 65, 70}; return c11;
    }

  case dim7ChordType:
    switch (key)
    {
      case 0:  static const std::vector<uint8_t> c0  = {48, 51, 54, 57}; return c0; // C Eb Gb A
      case 1:  static const std::vector<uint8_t> c1  = {49, 52, 55, 58}; return c1;
      case 2:  static const std::vector<uint8_t> c2  = {50, 53, 56, 59}; return c2;
      case 3:  static const std::vector<uint8_t> c3  = {51, 54, 57, 60}; return c3;
      case 4:  static const std::vector<uint8_t> c4  = {52, 55, 58, 61}; return c4;
      case 5:  static const std::vector<uint8_t> c5  = {53, 56, 59, 62}; return c5;
      case 6:  static const std::vector<uint8_t> c6  = {54, 57, 60, 63}; return c6;
      case 7:  static const std::vector<uint8_t> c7  = {55, 58, 61, 64}; return c7;
      case 8:  static const std::vector<uint8_t> c8  = {56, 59, 62, 65}; return c8;
      case 9:  static const std::vector<uint8_t> c9  = {57, 60, 63, 66}; return c9;
      case 10: static const std::vector<uint8_t> c10 = {58, 61, 64, 67}; return c10;
      default: static const std::vector<uint8_t> c11 = {59, 62, 65, 68}; return c11;
    }
  case powerChordType:
    switch (key)
    {
      case 0:  static const std::vector<uint8_t> c0  = {48, 55}; return c0; // C5
      case 1:  static const std::vector<uint8_t> c1  = {49, 56}; return c1;
      case 2:  static const std::vector<uint8_t> c2  = {50, 57}; return c2;
      case 3:  static const std::vector<uint8_t> c3  = {51, 58}; return c3;
      case 4:  static const std::vector<uint8_t> c4  = {52, 59}; return c4;
      case 5:  static const std::vector<uint8_t> c5  = {53, 60}; return c5;
      case 6:  static const std::vector<uint8_t> c6  = {54, 61}; return c6;
      case 7:  static const std::vector<uint8_t> c7  = {55, 62}; return c7;
      case 8:  static const std::vector<uint8_t> c8  = {56, 63}; return c8;
      case 9:  static const std::vector<uint8_t> c9  = {57, 64}; return c9;
      case 10: static const std::vector<uint8_t> c10 = {58, 65}; return c10;
      default: static const std::vector<uint8_t> c11 = {59, 66}; return c11;
    }
  case dominant13b9ChordType:
    switch (key)
    {
      case 0:  static const std::vector<uint8_t> c0  = {48, 52, 58, 61, 65, 69}; return c0; // C E♭ F# Bb A
      case 1:  static const std::vector<uint8_t> c1  = {49, 53, 59, 62, 66, 70}; return c1;
      case 2:  static const std::vector<uint8_t> c2  = {50, 54, 60, 63, 67, 71}; return c2;
      case 3:  static const std::vector<uint8_t> c3  = {51, 55, 61, 64, 68, 72}; return c3;
      case 4:  static const std::vector<uint8_t> c4  = {52, 56, 62, 65, 69, 73}; return c4;
      case 5:  static const std::vector<uint8_t> c5  = {53, 57, 63, 66, 70, 74}; return c5;
      case 6:  static const std::vector<uint8_t> c6  = {54, 58, 64, 67, 71, 75}; return c6;
      case 7:  static const std::vector<uint8_t> c7  = {55, 59, 65, 68, 72, 76}; return c7;
      case 8:  static const std::vector<uint8_t> c8  = {56, 60, 66, 69, 73, 77}; return c8;
      case 9:  static const std::vector<uint8_t> c9  = {57, 61, 67, 70, 74, 78}; return c9;
      case 10: static const std::vector<uint8_t> c10 = {58, 62, 68, 71, 75, 79}; return c10;
      default: static const std::vector<uint8_t> c11 = {59, 63, 69, 72, 76, 80}; return c11;
    }
  case dominant7sus4ChordType:
    switch (key)
    {
      case 0:  static const std::vector<uint8_t> c0  = {48, 53, 55, 58}; return c0; // C F G Bb
      case 1:  static const std::vector<uint8_t> c1  = {49, 54, 56, 59}; return c1;
      case 2:  static const std::vector<uint8_t> c2  = {50, 55, 57, 60}; return c2;
      case 3:  static const std::vector<uint8_t> c3  = {51, 56, 58, 61}; return c3;
      case 4:  static const std::vector<uint8_t> c4  = {52, 57, 59, 62}; return c4;
      case 5:  static const std::vector<uint8_t> c5  = {53, 58, 60, 63}; return c5;
      case 6:  static const std::vector<uint8_t> c6  = {54, 59, 61, 64}; return c6;
      case 7:  static const std::vector<uint8_t> c7  = {55, 60, 62, 65}; return c7;
      case 8:  static const std::vector<uint8_t> c8  = {56, 61, 63, 66}; return c8;
      case 9:  static const std::vector<uint8_t> c9  = {57, 62, 64, 67}; return c9;
      case 10: static const std::vector<uint8_t> c10 = {58, 63, 65, 68}; return c10;
      default: static const std::vector<uint8_t> c11 = {59, 64, 66, 69}; return c11;
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
      case 4:  static const std::vector<uint8_t> c4  = {52, 53, 54}; return c4;
      case 5:  static const std::vector<uint8_t> c5  = {53, 54, 55}; return c5;
      case 6:  static const std::vector<uint8_t> c6  = {54, 55, 56}; return c6;
      case 7:  static const std::vector<uint8_t> c7  = {55, 56, 57}; return c7;
      case 8:  static const std::vector<uint8_t> c8  = {56, 57, 58}; return c8;
      case 9:  static const std::vector<uint8_t> c9  = {57, 58, 59}; return c9;
      case 10: static const std::vector<uint8_t> c10 = {58, 59, 60}; return c10;
      default: static const std::vector<uint8_t> c11 = {59, 60, 61}; return c11;
    }
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