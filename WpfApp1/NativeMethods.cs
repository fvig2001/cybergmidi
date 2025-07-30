using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace CyberG
{
    [StructLayout(LayoutKind.Sequential)]
    public struct EncodedNote
    {
        public int midiNote;         // MIDI note number, 255 for rest
        public int noteOrder;        // Sequential group index
        public int length;           // Length in 1/64th or 1/256th notes (units)
        public int lengthTicks;      // Length in ticks (for debug)
        public int velocity;         // velocity
        public int relativeOctave;   // octave relative to root note
        public int channel;
    }
    public static class NativeLoader
    {
        private static IntPtr _dllHandle;

        public delegate int MyFunctionDelegate(int x);
        // Delegate for writeMidiFromEncoded
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] // or StdCall if your DLL uses that
        public delegate int WriteMidiFromEncodedDelegate(
            IntPtr standardNotes,   // EncodedNote* standardNotes
            ref int noteSize,       // int* noteSize
            [MarshalAs(UnmanagedType.LPStr)] string filename,
            int bpm,
            bool midiPlayerFix);

        public static WriteMidiFromEncodedDelegate writeMidiFromEncoded;
        
        // Delegate for ConvertMidiToStruct
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public unsafe delegate int ConvertMidiToStructDelegate(
            [MarshalAs(UnmanagedType.LPStr)] string fMidiFile,
            out IntPtr standardNotes,        // EncodedNote**
            out IntPtr placeHolderNotes,     // EncodedNote**
            ref int cgdSize,
            int channel);

        public static ConvertMidiToStructDelegate ConvertMidiToStruct;

        // Delegate for FreeStruct
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate int FreeStructDelegate(IntPtr data); // EncodedNote*

        public static FreeStructDelegate FreeStruct;

        // Delegate for GetOmniChordToStruct
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public unsafe delegate int GetOmniChordToStructDelegate(
            int backingNo,
            out IntPtr placeHolderNotesBass,
            out IntPtr placeHolderNotesAccompaniment,
            out IntPtr placeHolderNotesDrums,
            ref int cgdSizeBass,
            ref int cgdSizeAccompaniment,
            ref int cgdSizeDrums);

        public static GetOmniChordToStructDelegate GetOmniChordToStruct;

        public static void LoadNativeLibrary()
        {

            // Build path to DLL
            string exeDir = AppDomain.CurrentDomain.BaseDirectory;
            string dllPath = Path.Combine(exeDir, "MidiToStruct.dll");

            if (!File.Exists(dllPath))
                throw new FileNotFoundException("Native DLL not found: " + dllPath);

            _dllHandle = LoadLibrary(dllPath);
            if (_dllHandle == IntPtr.Zero)
            {
                throw new Exception("Failed to load native DLL from " + dllPath);
            }

            IntPtr funcPtr;

            funcPtr = GetProcAddress(_dllHandle, "writeMidiFromEncoded");
            if (_dllHandle == IntPtr.Zero)
            {
                int errCode = Marshal.GetLastWin32Error();
                throw new Exception($"Failed to load DLL. Error code: {errCode}");
            }
            writeMidiFromEncoded = Marshal.GetDelegateForFunctionPointer<WriteMidiFromEncodedDelegate>(funcPtr);

            funcPtr = GetProcAddress(_dllHandle, "ConvertMidiToStruct");
            ConvertMidiToStruct = Marshal.GetDelegateForFunctionPointer<ConvertMidiToStructDelegate>(funcPtr);

            funcPtr = GetProcAddress(_dllHandle, "FreeStruct");
            FreeStruct = Marshal.GetDelegateForFunctionPointer<FreeStructDelegate>(funcPtr);

            funcPtr = GetProcAddress(_dllHandle, "GetOmniChordToStruct");
            GetOmniChordToStruct = Marshal.GetDelegateForFunctionPointer<GetOmniChordToStructDelegate>(funcPtr);
        }

        // Helper to find parent folder by name
        private static string? FindParentFolder(string startDir, string folderName)
        {
            DirectoryInfo? dir = new DirectoryInfo(startDir);
            while (dir != null)
            {
                if (string.Equals(dir.Name, folderName, StringComparison.OrdinalIgnoreCase))
                    return dir.FullName;

                dir = dir.Parent;
            }
            return null;
        }

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr LoadLibrary(string lpFileName);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr GetProcAddress(IntPtr hModule, string procName);
    }
}
