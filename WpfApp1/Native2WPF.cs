
using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using static CyberG.NativeLoader;

namespace CyberG
{
    public struct midiPattern
    {
        public int midiNote;         // MIDI note number, 255 for rest
        public int offset;           // when it should be played
        public int length;           // Length in 1/64th or 1/256th notes (units)
        public int lengthTicks;      // Length in ticks (for debug)
        public int velocity;         // velocity
        public int relativeOctave;   // octave relative to root note
        public int channel;          // midi channel
    }
    internal class Native2WPF
    {
        public const int TPM = 12;
        public const bool ALLOW_LOOP = true;
        public const int BASS_CHANNEL = 1;
        public const int DRUMS_CHANNEL = 9;
        public const int BACKING_CHANNEL = 0;
        public static List<midiPattern> converted = new List<midiPattern>();
        public static List<midiPattern> convertedPlaceholder = new List<midiPattern>();
        public static List<midiPattern> omniConverted = new List<midiPattern>();
        
        //convert from Cyber G to midi-ish format
        public static void convertStandardToEncoded(List<midiPattern> input, ref List<EncodedNote> output, bool decodePlaceholder = true)
        {
            int[] rootNotes = { 48, 52, 55, 59, 62, 65, 69 };
            output.Clear();
            int lastOffset = 1;
            int curOrder = 0;
            for (int i = 0; i < input.Count; i++)
            {
                EncodedNote n = new EncodedNote();
                n.velocity = input[i].velocity;
                n.midiNote = input[i].midiNote;
                if (decodePlaceholder)
                {
                    if (n.midiNote < rootNotes.Length)
                    {
                        n.midiNote = rootNotes[n.midiNote];
                    }
                }
                n.relativeOctave = input[i].relativeOctave;
                n.channel = input[i].channel;
                n.length = input[i].length;
                n.lengthTicks = input[i].lengthTicks;
                if (lastOffset != input[i].offset)
                {
                    curOrder++;
                }
                n.noteOrder = curOrder;
                lastOffset = input[i].offset;
                output.Add(n);
            }
        }

        public static void convertEncodedToStandard(List<EncodedNote> input, ref List<midiPattern> output, bool usePlaceholder)
        {
            int[] rootNotes = { 48, 52, 55, 59, 62, 65, 69 };
            output.Clear();
            int curOffset = 1;
            int curOrder = 0;
            
            int lastLength = 0;
            for (int i = 0; i < input.Count; i++)
            {
                midiPattern n = new midiPattern();
                n.velocity = input[i].velocity;
                n.midiNote = input[i].midiNote;
                if (!usePlaceholder)
                {
                    n.midiNote = input[i].midiNote;
                }
                else 
                {
                    for (int j = 0; j < rootNotes.Length; j++)
                    {
                        if (input[i].midiNote == rootNotes[j])
                        {
                            n.midiNote = i;
                            break;
                        }
                    }
                }
                n.relativeOctave = input[i].relativeOctave;

                n.channel = input[i].channel;
                n.length = input[i].length;
                n.lengthTicks = input[i].lengthTicks;
                if (curOrder != input[i].noteOrder)
                {
                    curOffset += lastLength;
                    curOrder = input[i].noteOrder;
                }
                lastLength = input[i].length;
                n.offset = curOffset;
                output.Add(n);
            }
        }
        public static bool convertOmnichordBackingToMidi(string midiFilepath, int backingNo, int bpm)
        {
            int[] rootNotes = { 48, 52, 55, 59, 62, 65, 69 };
            IntPtr bassPtr, accPtr, drumPtr;
            int bassSize = 0, accSize = 0, drumSize = 0;

            int result = NativeLoader.GetOmniChordToStruct(
                backingNo,
                out bassPtr,
                out accPtr,
                out drumPtr,
                ref bassSize,
                ref accSize,
                ref drumSize);
            if (bassPtr == IntPtr.Zero && accPtr == IntPtr.Zero && drumPtr == IntPtr.Zero)
            {
                MessageBox.Show("All native pointers are NULL — possible failure in native allocation.");
                return false;
            }
            if (result == 0 || (bassSize == 0 && accSize == 0 && drumSize == 0))
            {
                return false;
            }

            //List<midiPattern> combined = new List<midiPattern>();
            int structSize = Marshal.SizeOf<EncodedNote>();
            List<EncodedNote> ConvertPtrToList (IntPtr srcPtr, int count, int channel)
            {
                //EncodedNote[] notes = new EncodedNote[count];
                List<EncodedNote> result = new List<EncodedNote>();
                
                for (int i = 0; i < count; i++)
                {
                    EncodedNote single;
                    IntPtr ptr = IntPtr.Add(srcPtr, i * structSize);
                    single = Marshal.PtrToStructure<EncodedNote>(ptr);
                    if (channel != -1)
                    {
                        single.channel = channel; // tag channel info if needed
                    }
                    else
                    {
                        single.channel -= 1; 
                    }
                    result.Add(single);
                }

                return result;
            }
        
            List<EncodedNote> mergedEncoded = new List<EncodedNote>(); //crashes after this line
            List<EncodedNote> encodedList = new List<EncodedNote>();
            int lastOrder = -1;
            omniConverted.Clear();
            if (bassPtr != IntPtr.Zero && bassSize > 0)
            {
                encodedList = ConvertPtrToList(bassPtr, bassSize, BASS_CHANNEL); // Bass = channel 0
            }
            if (ALLOW_LOOP)
            {
                lastOrder = encodedList[bassSize - 1].noteOrder + 1;

                for (int i = 0; i < bassSize; i++)
                {
                    EncodedNote n = encodedList[i];
                    n.noteOrder = lastOrder + n.noteOrder;
                    encodedList.Add(n);
                }
            }
            
            for (int i = 0; i < encodedList.Count; i++)
            {
                mergedEncoded.Add(encodedList[i]);
            }

            if (accPtr != IntPtr.Zero && accSize > 0)
            {
                encodedList = ConvertPtrToList(accPtr, accSize, BACKING_CHANNEL); // Accompaniment = channel 1
            }
            if (ALLOW_LOOP)
            {
                lastOrder = encodedList[accSize - 1].noteOrder + 1;

                for (int i = 0; i < accSize; i++)
                {
                    EncodedNote n = encodedList[i];
                    n.noteOrder = lastOrder + n.noteOrder;
                    encodedList.Add(n);
                }
            }

            for (int i = 0; i < encodedList.Count; i++)
            {
                mergedEncoded.Add(encodedList[i]);

            }


            if (drumPtr != IntPtr.Zero && drumSize > 0)
            {
                encodedList = ConvertPtrToList(drumPtr, drumSize, -1); // Drums = channel 9
            }
            encodedList = encodedList.OrderBy(note => note.noteOrder).ToList();

            if (ALLOW_LOOP)
            {
                lastOrder = encodedList[drumSize - 1].noteOrder + 1;

                for (int i = 0; i < drumSize; i++)
                {
                    EncodedNote n = encodedList[i];
                    n.noteOrder = lastOrder + n.noteOrder;
                    encodedList.Add(n);
                }
            }
            for (int i = 0; i < encodedList.Count; i++)
            {
                mergedEncoded.Add(encodedList[i]);
            }

            // Free native memory
            if (bassPtr != IntPtr.Zero)
                NativeLoader.FreeStruct(bassPtr);
            if (accPtr != IntPtr.Zero)
                NativeLoader.FreeStruct(accPtr);
            if (drumPtr != IntPtr.Zero)
                NativeLoader.FreeStruct(drumPtr);


            int combinedSize = mergedEncoded.Count;
            EncodedNote[] encoded = new EncodedNote[combinedSize];
            string filePath = @"D:\temp\debug.txt";
            string textToAppend = "";
            
            for (int i = 0; i < mergedEncoded.Count; i++)
            {
                textToAppend = mergedEncoded[i].channel.ToString() + ": " + mergedEncoded[i].midiNote.ToString() + " " + mergedEncoded[i].length.ToString() + " " + mergedEncoded[i].lengthTicks.ToString() + " " + mergedEncoded[i].noteOrder.ToString()+"\n";
                

                encoded[i] = mergedEncoded[i];
                if (encoded[i].channel != DRUMS_CHANNEL && encoded[i].channel != (DRUMS_CHANNEL - 1) && encoded[i].midiNote != 255 && encoded[i].midiNote < 3)
                {
                    encoded[i].midiNote = rootNotes[encoded[i].midiNote];
                }
                try
                {
                    // Append text to the file, creating it if it doesn't exist
                    File.AppendAllText(filePath, textToAppend);
                }
                catch (Exception ex)
                {

                }
                //DebugLog.addToLog(debugType.miscDebug, "Converted " + encoded[i].midiNote.ToString() + " " + encoded[i].noteOrder.ToString() + " " + encoded[i].lengthTicks.ToString() + " " + encoded[i].channel);
            }

            // Marshal back to native memory
            IntPtr encPtr = Marshal.AllocHGlobal(structSize * combinedSize);
            for (int i = 0; i < combinedSize; i++)
            {
                IntPtr dest = IntPtr.Add(encPtr, i * structSize);
                Marshal.StructureToPtr(encoded[i], dest, false);
            }

            bool success = NativeLoader.writeMidiFromEncoded(encPtr, ref combinedSize, midiFilepath, bpm) != 0;

            Marshal.FreeHGlobal(encPtr);
            return success;
        }


        public static bool convertMidiToMidiPattern(string midiFilePath, int channel)
        {
            // 1. Prepare input parameters
            int cgdSize = 0;
            IntPtr standardNotesPtr;
            IntPtr placeHolderNotesPtr;

            // 2. Call the native function
            int result = NativeLoader.ConvertMidiToStruct(
                midiFilePath,
                out standardNotesPtr,
                out placeHolderNotesPtr,
                ref cgdSize,
                channel);

            if (result == 1) // assuming 0 means success
            {
                Console.WriteLine($"Success! Number of notes: {cgdSize}");
                
                // 3. Marshal the returned native array into managed array
                EncodedNote[] notes = new EncodedNote[cgdSize];
                IntPtr currentPtr = standardNotesPtr;
                EncodedNote[] notes2 = new EncodedNote[cgdSize];
                IntPtr currentPtr2 = placeHolderNotesPtr;
                int structSize = Marshal.SizeOf<EncodedNote>();

                for (int i = 0; i < cgdSize; i++)
                {
                    notes[i] = Marshal.PtrToStructure<EncodedNote>(currentPtr);
                    currentPtr = IntPtr.Add(currentPtr, structSize);
                    notes2[i] = Marshal.PtrToStructure<EncodedNote>(currentPtr2);
                    currentPtr2 = IntPtr.Add(currentPtr2, structSize);
                }
                converted.Clear();
                convertedPlaceholder.Clear();
                convertNative2MidiPattern(notes, cgdSize, ref converted);
                convertNative2MidiPattern(notes2, cgdSize, ref convertedPlaceholder);
                //for (int i = 0; i < converted.Count; i++)
                //{
                //DebugLog.addToLog(debugType.miscDebug, "Converted " + converted[i].midiNote.ToString() + " " + converted[i].offset.ToString() + " " +converted[i].lengthTicks.ToString());
                //}
                // 5. IMPORTANT: Free the native memory when done
                NativeLoader.FreeStruct(standardNotesPtr);
                NativeLoader.FreeStruct(placeHolderNotesPtr);
            }
            else
            {
                Console.WriteLine($"ConvertMidiToStruct failed with error code {result}");
            }

            return true;
        }
        static bool convertMidiPattern2EncodedList(ref List<EncodedNote> arr, bool addSpace = false)
        {

            int curOrder = 0;
            int curOffset = 1; //offset starts at 1
            int expectedNextOffset = -1;
            int lastLength = 0;
            for (int i = 0; i < converted.Count; i++)
            {
                EncodedNote n = new EncodedNote();
                n.midiNote = converted[i].midiNote;
                n.lengthTicks = converted[i].lengthTicks;
                n.length = converted[i].length;
                n.relativeOctave = converted[i].relativeOctave;
                n.channel = converted[i].channel;
                n.velocity = converted[i].velocity;
                //insert rest if there is a gap between offset + length
                if (converted[i].offset != curOffset)
                {
                    expectedNextOffset = curOffset + lastLength;
                    if (expectedNextOffset != converted[i].offset)
                    {
                        EncodedNote rest = new EncodedNote();
                        rest.midiNote = 255;
                        rest.length = converted[i].offset - expectedNextOffset;
                        rest.lengthTicks = rest.length * TPM;
                        rest.velocity = converted[i].velocity;
                        rest.relativeOctave = 0;
                        rest.channel = converted[i].channel;
                        rest.noteOrder = curOrder;
                        curOrder++;
                        arr.Add(rest);
                    }
                    curOrder++;
                    curOffset = converted[i].offset;
                    if (addSpace)
                    {
                        EncodedNote rest2 = new EncodedNote();
                        rest2.midiNote = 255;
                        rest2.length = 1;
                        rest2.lengthTicks = rest2.length * TPM;
                        rest2.velocity = converted[i].velocity;
                        rest2.relativeOctave = 0;
                        rest2.channel = converted[i].channel;
                        rest2.noteOrder = curOrder;
                        curOrder++;
                        arr.Add(rest2);
                    }
                }
                n.noteOrder = curOrder;
                lastLength = n.length;
                arr.Add(n);

            }
            return true;
        }
        static bool convertMidiPattern2Encoded(ref EncodedNote[] arr, ref int count, bool addSpace = false)
        {
            List<EncodedNote> l = new List<EncodedNote>();
            count = 0;

            if (!convertMidiPattern2EncodedList(ref l, addSpace))
            {
                return false;
            }

            count = l.Count;
            arr = new EncodedNote[count];

            // Copy the list into the array
            for (int i = 0; i < count; i++)
            {
                arr[i] = l[i];
            }
            
            return true;
        }
        public static void ConvertPlaceholderToC(ref List<EncodedNote> arr)
        {
            int[] rootNotes = { 48, 52, 55, 59, 62, 65, 69 };
            for (int i = 0; i < arr.Count; i++)
            {
                var note = arr[i]; // Make a copy (value type)
                if (note.midiNote < rootNotes.Length)
                {
                    note.midiNote = rootNotes[note.midiNote];
                    arr[i] = note; // Assign back to the list
                }
            }
        }
        public static bool convertMidiPatternToMidi(string filename, int bpm, bool addSpace = false, bool addFinalRest = false)
        {
            if (converted.Count() == 0)
            {
                return false;
            }
            if (addFinalRest) //add quiet note at the end since the player gets notified while the last note is starting
            {
                midiPattern mp = new midiPattern();
                mp.midiNote = 60;
                mp.offset = converted[converted.Count() - 1].offset + converted[converted.Count() - 1].length + 1;
                mp.velocity = 1;
                mp.length = 6;
                mp.lengthTicks = mp.length * TPM;
                mp.channel = converted[converted.Count() - 1].channel;
                converted.Add(mp);
            }
            EncodedNote[] enc = new EncodedNote[converted.Count()];
            int size = -1;
            if (!convertMidiPattern2Encoded(ref enc, ref size, addSpace))
            {
                return false;
            }
            
            //add missing steps
            
            //for (int i = 0; i < size; i++)
            //{
                //Console.WriteLine($"Note {i}: midiNote={enc[i].midiNote}, length={enc[i].length}, velocity={enc[i].velocity}");
                //DebugLog.addToLog(debugType.miscDebug, i.ToString()+ ": " + enc[i].midiNote.ToString() + " " + enc[i].noteOrder.ToString());
            //}
            int structSize = Marshal.SizeOf<EncodedNote>();
            IntPtr encPtr = Marshal.AllocHGlobal(structSize * size);
            
            try
            {
                // Copy each EncodedNote into unmanaged memory
                for (int i = 0; i < size; i++)
                {
                    IntPtr dest = IntPtr.Add(encPtr, i * structSize);
                    Marshal.StructureToPtr(enc[i], dest, false);
                }

                // Call the native function
                int result = NativeLoader.writeMidiFromEncoded(encPtr, ref size, filename, bpm);
                
                return result != 0; // assume 0 = failure 
            }
            finally
            {
                // Always free unmanaged memory
                Marshal.FreeHGlobal(encPtr);
            }
        }
        //Used to copy
        static bool convertNative2MidiPattern(EncodedNote[] original, int originalCnt, ref List<midiPattern> result)
        {
            bool ret = false;
            result.Clear();
            int cur_order = 0;
            int last_length = 0;
            int curOffset = 0;
            for (int i = 0; i < originalCnt; i++)
            {
                midiPattern m = new midiPattern();
                m.channel =  original[i].channel;
                m.relativeOctave = original[i].relativeOctave;
                m.length = original[i].length;
                m.midiNote = original[i].midiNote;
                m.lengthTicks = original[i].lengthTicks;
                m.velocity = original[i].velocity; 

                //means we need to copy last offset
                if (original[i].noteOrder != cur_order)
                {
                    curOffset += last_length;
                    cur_order = original[i].noteOrder;
                }
                m.offset = curOffset;
                last_length = original[i].lengthTicks;
                if (!ret)
                {
                    ret = true;
                }
                result.Add(m);
            }
            return ret;
        }
    }
}
