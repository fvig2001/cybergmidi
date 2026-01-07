using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.Metrics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;
using System.Runtime.InteropServices;
using System.IO;

using System.Windows.Interop;
using Microsoft.Win32;
using System.Xml.Linq;
using System.Reflection;



namespace CyberG
{

    /// <summary>
    /// Interaction logic for PatternWindow.xaml
    /// </summary>
    public struct patternData
    {
        public string name;
        public string path; // full path to the folder
        public int id;      // folder name as integer
    }

    public partial class PatternWindow : Window
    {
        [DllImport("winmm.dll")]
        private static extern long mciSendString(string command, StringBuilder returnValue, int returnLength, IntPtr winHandle);


        private const int MM_MCINOTIFY = 0x3B9;
        private const int MCI_NOTIFY_SUCCESSFUL = 0x0001;

        private HwndSource hwndSource;
        private IntPtr hwnd;

        protected override void OnSourceInitialized(EventArgs e)
        {
            base.OnSourceInitialized(e);

            hwnd = new WindowInteropHelper(this).Handle;
            hwndSource = HwndSource.FromHwnd(hwnd);
            hwndSource.AddHook(WndProc);
        }

        private IntPtr WndProc(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam, ref bool handled)
        {
            if (msg == MM_MCINOTIFY && wParam.ToInt32() == MCI_NOTIFY_SUCCESSFUL)
            {

                Dispatcher.Invoke(() => stopPlaying());
                isPlaying = false;
                Dispatcher.Invoke(() => UpdatePlayButtonGraphicStopAll());
                handled = true;
            }
            return IntPtr.Zero;
        }
        bool internalHasNotes = false;
        int curID = 0;
        int curIDtoSet = 0;
        bool curIDExists = false;
        const int GUITAR_CHANNEL = 0;
        const int PIANO_CHANNEL = 1;
        const int BASS_CHANNEL = 2;
        const int BACKING_CHANNEL = 4;
        const int DRUMS_CHANNEL = 9;
        int selected_Channel = 0;
        string clearCMD = SerialDevice.CLEAR_GUITAR_DATA;
        string addCMD = SerialDevice.ADD_GUITAR_DATA;
        int selected_Slot = 0;
        private List<midiPattern> lm = new List<midiPattern>();
        private const int TPM = 12;
        private TaskCompletionSource<bool> dataReceivedTcs;
        private int patternLeftToRead = 0;
        private List<patternData> patternDataList = new List<patternData>();

        //loaded pattern
        private List<EncodedNote> tempPattern = new List<EncodedNote>();
        private List<EncodedNote> tempPatternPlaceholder = new List<EncodedNote>();

        private List<EncodedNote> importPattern = new List<EncodedNote>();
        private List<EncodedNote> importPatternPlaceholder = new List<EncodedNote>();

        private List<midiPattern> tempPatternSend = new List<midiPattern>();
        private List<midiPattern> tempPatternPreview = new List<midiPattern>();
        private List<EncodedNote> guitarPattern0 = new List<EncodedNote>();
        private List<EncodedNote> guitarPattern1 = new List<EncodedNote>();
        private List<EncodedNote> guitarPattern2 = new List<EncodedNote>();
        private List<EncodedNote> drumPattern0 = new List<EncodedNote>();
        private List<EncodedNote> drumPattern1 = new List<EncodedNote>();
        private List<EncodedNote> drumPattern2 = new List<EncodedNote>();
        private List<EncodedNote> drumPattern3 = new List<EncodedNote>();
        private List<EncodedNote> drumPattern4 = new List<EncodedNote>();
        private List<EncodedNote> bassPattern = new List<EncodedNote>();
        private List<EncodedNote> backingPattern = new List<EncodedNote>();
        private List<Button> allPlayButtons = new List<Button>();
        private bool isPlaying = false;
        private bool isPaused = false;
        private string midiAlias = "backingTrack";
        private int pausedPosition = 0;

        private bool makeMidi = false;
        private int oldBPM = 0;
        private readonly string tempPath = System.IO.Path.GetTempPath();
        private bool initFetch = false;
        private const int MaxID = 65535;
        private const int MaxBackingState = 10;
        public int backingState = 0;
        private int lastMidiBackingState = -1;
        private int dataToRead;
        private int dataRead;
        private bool isPatternReading;
        private bool doneLoading;
        private const string basePath = "library";
        private string curPatternPath = "guitar";
        private int firstFreeSlot = -1;
        public PatternWindow(int bpm)
        {
            doneLoading = false;
            this.Loaded += Window_Loaded;
            isPatternReading = false;
            dataToRead = -1;
            dataRead = -1;
            InitializeComponent();
            setupComboBoxes();
            setupListBoxes();
            setupRadioButtons();
            setupButtons();
            //setupRightSide(false);
            setupRightSide(true);

            allPlayButtons.Add(playBackingButton);
            allPlayButtons.Add(playButton0);
            allPlayButtons.Add(playButton1);
            allPlayButtons.Add(playButton2);
            allPlayButtons.Add(playButton3);
            allPlayButtons.Add(playButton4);
            allPlayButtons.Add(playButton5);
            allPlayButtons.Add(playButton6);
            allPlayButtons.Add(playButton7);
            allPlayButtons.Add(playButton8);
            allPlayButtons.Add(playButton9);
            allPlayButtons.Add(playSampleButton);

            bpmTextBox.Text = bpm.ToString();
            if (SerialManager.isDeviceConnected())
            {
                SerialManager.Device.DataReceived += OnPatternWindowSerialDataReceived;
                initFetch = true;
                SendCmd(SerialDevice.GET_BACKING_STATE);
                SendCmd(SerialDevice.GET_DRUM_ID, "0");
                SendCmd(SerialDevice.GET_DRUM_ID, "1");
                SendCmd(SerialDevice.GET_DRUM_ID, "2");
                SendCmd(SerialDevice.GET_DRUM_ID, "3");
                SendCmd(SerialDevice.GET_DRUM_ID, "4");
                SendCmd(SerialDevice.GET_BASS_ID);
                SendCmd(SerialDevice.GET_BACKING_ID);
                SendCmd(SerialDevice.GET_GUITAR_ID, "0");
                SendCmd(SerialDevice.GET_GUITAR_ID, "1");
                SendCmd(SerialDevice.GET_GUITAR_ID, "2");
                getInternalPattern();
            }
            //todo erase
            EncodedNote n = new EncodedNote();
            n.relativeOctave = 0;
            n.midiNote = 0;
            n.noteOrder = 0;
            n.velocity = 110;
            n.channel = 1;
            n.length = 12;
            n.lengthTicks = n.length * TPM;
            guitarPattern0.Add(n);
            n.midiNote = 1;
            n.lengthTicks = n.length * TPM;
            guitarPattern0.Add(n);
            n.midiNote = 2;
            n.relativeOctave = 0;
            n.lengthTicks = n.length * TPM;
            guitarPattern0.Add(n);
            n.relativeOctave = -2;
            n.midiNote = 0;
            n.noteOrder = 1;
            n.length = 6;
            n.lengthTicks = n.length * TPM;
            guitarPattern0.Add(n);
            n.midiNote = 255;
            n.length = 12;
            n.noteOrder = 2;
            n.lengthTicks = n.length * TPM;
            guitarPattern0.Add(n);
            n.relativeOctave = 3;
            n.midiNote = 2;
            n.noteOrder = 3;
            n.length = 24;
            n.lengthTicks = n.length * TPM;
            guitarPattern0.Add(n);
            n.relativeOctave = 2;
            n.midiNote = 2;
            n.noteOrder = 4;
            n.length = 24;
            n.lengthTicks = n.length * TPM;
            guitarPattern0.Add(n);
            n.relativeOctave = 3;
            n.midiNote = 2;
            n.noteOrder = 5;
            n.length = 12;
            n.lengthTicks = n.length * TPM;
            guitarPattern0.Add(n);
            n.relativeOctave = -2;
            n.midiNote = 2;
            n.noteOrder = 6;
            n.length = 24;
            n.lengthTicks = n.length * TPM;
            guitarPattern0.Add(n);

        }

        private void setupButtons()
        {
            playButton0.Click += playGeneric_Click;
            playButton1.Click += playGeneric_Click;
            playButton2.Click += playGeneric_Click;
            playButton3.Click += playGeneric_Click;
            playButton4.Click += playGeneric_Click;
            playButton5.Click += playGeneric_Click;
            playButton6.Click += playGeneric_Click;
            playButton7.Click += playGeneric_Click;
            playButton8.Click += playGeneric_Click;
            playButton9.Click += playGeneric_Click;
        }

        private void setupListBoxes()
        {
            patternListBox.Items.Clear();
            patternListBox.SelectionChanged += PatternListBox_SelectionChanged;
        }

        private void PatternListBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (patternListBox.SelectedItem is ListBoxItem selectedItem)
            {
                // Get the tag
                int tag = (int)selectedItem.Tag;
                if (tag == -2)
                {
                    bool hasNotes = false;
                    //not in library

                    curIDExists = false;
                    DrawNotes(importPattern, false);
                    if (importPattern.Count > 0)
                    {
                        hasNotes = true;
                    }
                    tempPattern = new List<EncodedNote>(importPattern);

                    saveButton.IsEnabled = hasNotes;
                    deleteButton.IsEnabled = false;
                }
                else if (tag == -1)
                {
                    bool hasNotes = false;
                    //not in library
                    if (curID == 0)
                    {
                        curIDExists = false;
                        if (selected_Channel == DRUMS_CHANNEL)
                        {
                            if (selected_Slot == 0)
                            {
                                DrawNotes(drumPattern0, false);
                                if (drumPattern0.Count > 0)
                                {
                                    hasNotes = true;
                                }
                                tempPattern = new List<EncodedNote>(drumPattern0);
                            }
                            else if (selected_Slot == 1)
                            {
                                DrawNotes(drumPattern1, false);
                                if (drumPattern1.Count > 0)
                                {
                                    hasNotes = true;
                                }
                                tempPattern = new List<EncodedNote>(drumPattern1);
                            }
                            else if (selected_Slot == 2)
                            {
                                DrawNotes(drumPattern2, false);
                                if (drumPattern2.Count > 0)
                                {
                                    hasNotes = true;
                                }
                                tempPattern = new List<EncodedNote>(drumPattern2);
                            }
                            else if (selected_Slot == 3)
                            {
                                DrawNotes(drumPattern3, false);
                                if (drumPattern3.Count > 0)
                                {
                                    hasNotes = true;
                                }
                                tempPattern = new List<EncodedNote>(drumPattern3);
                            }
                            else
                            {
                                DrawNotes(drumPattern4, false);
                                if (drumPattern4.Count > 0)
                                {
                                    hasNotes = true;
                                }
                                tempPattern = new List<EncodedNote>(drumPattern4);
                            }
                        }
                        else if (selected_Channel == GUITAR_CHANNEL)
                        {
                            if (selected_Slot == 0)
                            {
                                DrawNotes(guitarPattern0, true);
                                if (guitarPattern0.Count > 0)
                                {
                                    hasNotes = true;
                                }
                                tempPattern = new List<EncodedNote>(guitarPattern0);
                            }
                            else if (selected_Slot == 1)
                            {
                                DrawNotes(guitarPattern1, true);
                                if (guitarPattern1.Count > 0)
                                {
                                    hasNotes = true;
                                }
                                tempPattern = new List<EncodedNote>(guitarPattern1);
                            }
                            else
                            {
                                DrawNotes(guitarPattern2, true);
                                if (guitarPattern2.Count > 0)
                                {
                                    hasNotes = true;
                                }
                                tempPattern = new List<EncodedNote>(guitarPattern2);
                            }
                        }
                        else if (selected_Channel == BASS_CHANNEL)
                        {
                            DrawNotes(bassPattern, true);
                            if (bassPattern.Count > 0)
                            {
                                hasNotes = true;
                            }
                            tempPattern = new List<EncodedNote>(bassPattern);
                        }
                        else
                        {
                            DrawNotes(backingPattern, selected_Channel != DRUMS_CHANNEL);
                            if (backingPattern.Count > 0)
                            {
                                hasNotes = true;
                            }
                            tempPattern = new List<EncodedNote>(backingPattern);
                        }
                        if (selected_Channel != DRUMS_CHANNEL)
                        {
                            tempPatternPlaceholder = new List<EncodedNote>(tempPattern);
                            Native2WPF.ConvertPlaceholderToC(ref tempPattern);
                        }
                    }
                    else
                    {
                        DrawNotes(tempPattern, false);
                        if (tempPattern.Count > 0)
                        {
                            hasNotes = true;
                        }
                    }
                    saveButton.IsEnabled = hasNotes;
                    deleteButton.IsEnabled = false;
                }
                else
                {
                    curIDExists = true;
                    saveButton.IsEnabled = true;
                    patternNameTextBox.IsEnabled = true;
                    deleteButton.IsEnabled = true;
                    int id = patternDataList[patternListBox.SelectedIndex].id;
                    patternNameTextBox.Text = patternDataList[patternListBox.SelectedIndex].name;
                    //load pattern
                    if (Native2WPF.convertMidiToMidiPattern(patternDataList[patternListBox.SelectedIndex].path + "\\data.mid", selected_Channel) && Native2WPF.converted.Count > 0)
                    {
                        if (selected_Channel != DRUMS_CHANNEL)
                        {
                            Native2WPF.convertStandardToEncoded(Native2WPF.convertedPlaceholder, ref tempPatternPlaceholder, false);
                        }
                        else
                        {
                            tempPatternPlaceholder = new List<EncodedNote>(tempPattern);
                        }
                        Native2WPF.convertStandardToEncoded(Native2WPF.converted, ref tempPattern);
                        //DrawNotes(tempPattern, false);
                        DrawNotes(tempPattern, false);
                        saveButton.IsEnabled = true;
                        patternNameTextBox.IsEnabled = true;
                        deleteButton.IsEnabled = true;
                    }
                }

            }
        }

        private void updateRightSide(int selectedID = -1)
        {
            if (curID == 0) //if not available in pattern files (0 means pattern was made manually), put loaded data as pattern
            {
                curIDExists = false;
                if (selected_Channel == GUITAR_CHANNEL)
                {
                    if (selected_Slot == 0)
                    {
                        tempPattern = new List<EncodedNote>(guitarPattern0);
                    }
                    else if (selected_Slot == 1)
                    {
                        tempPattern = new List<EncodedNote>(guitarPattern1);
                    }
                    else
                    {
                        tempPattern = new List<EncodedNote>(guitarPattern2);
                    }
                }
                else if (selected_Channel == DRUMS_CHANNEL)
                {
                    if (selected_Slot == 0)
                    {
                        tempPattern = new List<EncodedNote>(drumPattern0);
                    }
                    else if (selected_Slot == 1)
                    {
                        tempPattern = new List<EncodedNote>(drumPattern1);
                    }
                    else if (selected_Slot == 2)
                    {
                        tempPattern = new List<EncodedNote>(drumPattern2);
                    }
                    else if (selected_Slot == 3)
                    {
                        tempPattern = new List<EncodedNote>(drumPattern3);
                    }
                    else
                    {
                        tempPattern = new List<EncodedNote>(drumPattern4);
                    }
                }
                else if (selected_Channel == BASS_CHANNEL)
                {
                    tempPattern = new List<EncodedNote>(bassPattern);
                }
                else
                {
                    tempPattern = new List<EncodedNote>(backingPattern);
                }
            }

            if (tempPattern.Count == 0)
            {
                patternNameTextBox.IsEnabled = false;
                saveButton.IsEnabled = false;
                deleteButton.IsEnabled = false;
            }
            else
            {
                deleteButton.IsEnabled = false; //can't delete because it doesn't exist
                patternNameTextBox.IsEnabled = true;
                saveButton.IsEnabled = true;
            }
            LoadPatterns(selectedID);
            //check if ID matches an existing pattern
            //initialize list box
            //if ID == 0 or unlisted, create an entry for it and copy it to tempPattern, but if it's empty, disable save
            //else, load the midi file from the folder and update tempPattern

        }
        private void setupRightSide(bool enabled = true)
        {

            saveButton.IsEnabled = enabled;
            deleteButton.IsEnabled = enabled;
            importMidiButton.IsEnabled = enabled;
            playBackingButton.IsEnabled = enabled;
            nextButton.IsEnabled = enabled;
            previousButton.IsEnabled = enabled;
            uploadButton.IsEnabled = enabled;

        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {

            doneLoading = true;
            guitar0PatternRadioButton.IsChecked = true;
        }

        int[] rootNotes = { 48, 52, 55, 59, 62, 65, 69 };

        private void DrawNotes(List<EncodedNote> notes, bool updatePlaceholder = true)
        {
            if (NoteCanvas.ActualWidth <= 0 || NoteCanvas.ActualHeight <= 0)
            {
                return;
            }
            List<EncodedNote> myNotes = new List<EncodedNote>(notes);
            if (updatePlaceholder)
            {
                for (int i = 0; i < myNotes.Count; i++)
                {
                    if (myNotes[i].midiNote < rootNotes.Length)
                    {
                        var note = myNotes[i];
                        note.midiNote = rootNotes[note.midiNote];
                        myNotes[i] = note;
                    }
                }
            }

            NoteCanvas.Children.Clear();

            double canvasWidth = NoteCanvas.ActualWidth;
            double canvasHeight = NoteCanvas.ActualHeight;

            // Include all notes for time spacing
            var allNotes = myNotes.Where(n => n.length > 0).ToList();
            if (allNotes.Count == 0) return;

            // Compute pitch range from actual notes only (exclude rests)
            var validNotes = allNotes.Where(n => n.midiNote != 255).ToList();
            if (validNotes.Count == 0) return;

            int pitchMin = validNotes.Select(n => n.midiNote + n.relativeOctave * 12).Min() - 2;
            int pitchMax = validNotes.Select(n => n.midiNote + n.relativeOctave * 12).Max() + 2;

            // Group all notes (including rests) by noteOrder
            var groups = allNotes
                .GroupBy(n => n.noteOrder)
                .OrderBy(g => g.Key)
                .ToList();

            // Map noteOrder → start tick
            Dictionary<int, int> noteOrderToStartTick = new();
            int currentTick = 0;

            foreach (var group in groups)
            {
                noteOrderToStartTick[group.Key] = currentTick;

                // Longest note in this group determines group duration
                int groupDuration = group.Max(n => n.length);
                currentTick += groupDuration;
            }

            int totalTicks = currentTick;
            if (totalTicks == 0) totalTicks = 1;

            // Draw non-rest notes only
            foreach (var note in validNotes)
            {
                int startTick = noteOrderToStartTick[note.noteOrder];
                int actualPitch = note.midiNote + note.relativeOctave * 12;

                double x = (startTick / (double)totalTicks) * canvasWidth;
                double width = (note.length / (double)totalTicks) * canvasWidth;
                if (width < 1) width = 1;

                double pitchRatio = (actualPitch - pitchMin) / (double)(pitchMax - pitchMin);
                pitchRatio = Math.Max(0, Math.Min(1, pitchRatio));
                double y = canvasHeight - pitchRatio * canvasHeight;

                double height = 10;

                var rect = new Rectangle
                {
                    Width = width,
                    Height = height,
                    Fill = Brushes.Gray,
                    Stroke = Brushes.Black,
                    StrokeThickness = 0.5
                };

                Canvas.SetLeft(rect, x);
                Canvas.SetTop(rect, y - height / 2);
                NoteCanvas.Children.Add(rect);
            }
        }


        private async void getInternalPattern()
        {
            await getCurData();
        }
        private void generateOmniBackingMidi(int backingValue)
        {
            String path = tempPath + "\\omni.mid";
            Native2WPF.convertOmnichordBackingToMidi(path, backingValue, int.Parse(bpmTextBox.Text));
            lastMidiBackingState = backingValue;
            oldBPM = int.Parse(bpmTextBox.Text);
        }
        private async Task getCurData()
        {
            dataReceivedTcs = new TaskCompletionSource<bool>();

            SendCmd(SerialDevice.GET_GUITAR_DATA, "0");
            await dataReceivedTcs.Task;

            SendCmd(SerialDevice.GET_GUITAR_DATA, "1");
            await dataReceivedTcs.Task;
            SendCmd(SerialDevice.GET_GUITAR_DATA, "2");
            await dataReceivedTcs.Task;
            if (backingState == 0)
            {

                SendCmd(SerialDevice.GET_BACKING_DATA, "0");
                await dataReceivedTcs.Task;

                SendCmd(SerialDevice.GET_DRUM_DATA, "0");
                await dataReceivedTcs.Task;
                SendCmd(SerialDevice.GET_DRUM_DATA, "1");
                await dataReceivedTcs.Task;
                SendCmd(SerialDevice.GET_DRUM_DATA, "2");
                await dataReceivedTcs.Task;
                SendCmd(SerialDevice.GET_DRUM_DATA, "3");
                await dataReceivedTcs.Task;
                SendCmd(SerialDevice.GET_DRUM_DATA, "4");
                await dataReceivedTcs.Task;
                SendCmd(SerialDevice.GET_BASS_DATA, "0");
                await dataReceivedTcs.Task;

            }
        }

        private void setupComboBoxes()
        {
            var optionsBacking = new[] {
                (string)Application.Current.Resources["CustomBacking"],
                (string)Application.Current.Resources["Rock01"],
                (string)Application.Current.Resources["Rock02"],
                (string)Application.Current.Resources["Bossanova"],
                (string)Application.Current.Resources["Funk"],
                (string)Application.Current.Resources["Country"],
                (string)Application.Current.Resources["Disco"],
                (string)Application.Current.Resources["SlowRock"],
                (string)Application.Current.Resources["Swing"],
                (string)Application.Current.Resources["Waltz"],
                (string)Application.Current.Resources["Hiphop"]
            };

            // Populate each ComboBox
            BackingModeComboBox.ItemsSource = optionsBacking;
            BackingModeComboBox.SelectionChanged += ComboModeComboBox_SelectionChanged;
        }
        private bool checkResult(List<string> parsedData)
        {
            if (parsedData.Count > 0)
            {
                if (parsedData[0] != "OK")
                {
                    DebugLog.addToLog(debugType.replyDebug, (string)Application.Current.Resources["Command"] + " " + SerialManager.Device.LastCommandSent + " " + (string)Application.Current.Resources["CommandNotOK"]);
                }
                else
                {
                    return true;
                }
            }
            else
            {
                DebugLog.addToLog(debugType.replyDebug, (string)Application.Current.Resources["Command"] + " " + SerialManager.Device.LastCommandSent + " " + (string)Application.Current.Resources["CommandReplyEmpty"]);
            }
            return false;
        }
        private void addToDebugLog(debugType type, string message)
        {
            Application.Current.Dispatcher.Invoke(() =>
            {
                DebugLog.addToLog(type, message);
            });
        }
        private bool checkCommandParameterRange(int min, int max, int val)
        {
            string command = SerialManager.Device.LastCommandSent;
            if (val > max)
            {
                addToDebugLog(debugType.replyDebug, (string)Application.Current.Resources["Error"] + " " + command + " " + (string)Application.Current.Resources["ErrorDataReceivedAtPosition"] + " " + val.ToString() + " > " + max.ToString());
                return false;
            }
            if (val < min)
            {
                addToDebugLog(debugType.replyDebug, (string)Application.Current.Resources["Error"] + " " + command + (string)Application.Current.Resources["ErrorDataReceivedAtPosition"] + " " + val.ToString() + " < " + min.ToString());
                return false;
            }

            return true;
        }

        private bool checkCommandParamCount(List<string> parsedData, int expected)
        {
            string command = SerialManager.Device.LastCommandSent;
            if (parsedData.Count < expected)
            {
                addToDebugLog(debugType.replyDebug, (string)Application.Current.Resources["Error"] + " " + command.ToString() + " " + (string)Application.Current.Resources["ErrorOnlyHas"] + " " + parsedData.Count() + " " + (string)Application.Current.Resources["ErrorDataVs"] + " " + expected.ToString());
                return false;
            }
            return true;
        }

        private bool checkReceivedValid(List<string> parsedData)
        {
            string command = SerialManager.Device.LastCommandSent;
            string param = SerialManager.Device.LastParamSent;
            int nTemp = -1;
            bool result = checkResult(parsedData);
            if (!result)
            {
                return false;
            }

            else if (command == SerialDevice.GET_BACKING_DATA)
            {
                List<midiPattern> lm = new List<midiPattern>();
                if (dataToRead == -1) //first one
                {
                    nTemp = int.Parse(parsedData[2]);
                    dataToRead = nTemp;
                }
                else
                {
                    midiPattern m = new midiPattern();
                    m.midiNote = int.Parse(parsedData[2]);
                    m.offset = int.Parse(parsedData[3]);
                    m.length = int.Parse(parsedData[4]);
                    m.velocity = int.Parse(parsedData[5]);
                    m.channel = int.Parse(parsedData[6]);
                    m.relativeOctave = int.Parse(parsedData[7]);
                    m.lengthTicks = m.length * TPM;
                    lm.Add(m);
                    dataToRead--;

                }
                if (dataToRead == 0)
                {
                    dataToRead = -1;
                    dataReceivedTcs.SetResult(true);
                }
                if (lm.Count > 0)
                {
                    Native2WPF.convertStandardToEncoded(lm, ref backingPattern);
                }
            }

            else if (command == SerialDevice.GET_BACKING_STATE)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(0, MaxBackingState, nTemp))
                    {
                        return false;
                    }
                }
            }
            else if (command == SerialDevice.CLEAR_BACKING_DATA || command == SerialDevice.CLEAR_BASS_DATA || command == SerialDevice.CLEAR_GUITAR_DATA || command == SerialDevice.CLEAR_DRUMS_DATA)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }
            else if (command == SerialDevice.ADD_BACKING_DATA || command == SerialDevice.ADD_BASS_DATA || command == SerialDevice.ADD_GUITAR_DATA || command == SerialDevice.ADD_DRUMS_DATA)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }
            else if (command == SerialDevice.SET_BACKING_ID || command == SerialDevice.SET_BASS_ID || command == SerialDevice.SET_GUITAR_ID || command == SerialDevice.SET_DRUM_ID)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }
            else if (command == SerialDevice.GET_DRUM_ID || command == SerialDevice.GET_BACKING_ID || command == SerialDevice.GET_BASS_ID || command == SerialDevice.GET_GUITAR_ID)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(0, MaxID, nTemp))
                    {
                        return false;
                    }
                }
            }

            return true;
        }

        private void OnPatternWindowSerialDataReceived(object sender, string data)
        {
            bool skipDeque = false;
            Dispatcher.Invoke(() =>
            {
                int nTemp = -1;
                string response = data.Trim();
                List<string> parsedData = SerialReceiveParser.Parse(response);

                bool isDebug = false;
                if (parsedData.Count != 0)
                {
                    //bool isOK = false;
                    if (parsedData[0] == "DB")
                    {
                        isDebug = true;
                    }

                    if (isDebug)
                    {
                        string dbgMsg = "";
                        for (int i = 2; i < parsedData.Count; i++)
                        {
                            dbgMsg += parsedData[i];
                        }
                        DebugLog.addToLog(debugType.deviceDebug, dbgMsg);
                    }
                    else
                    {
                        string command = SerialManager.Device.LastCommandSent;
                        string param = SerialManager.Device.LastParamSent;
                        if (command == SerialDevice.GET_BACKING_STATE)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                backingState = int.Parse(parsedData[2]);
                                BackingModeComboBox.SelectedIndex = backingState;
                                if (backingState != 0)
                                {
                                    generateOmniBackingMidi(backingState);
                                    makeMidi = true;
                                }
                            }
                        }
                        else if (command == SerialDevice.GET_GUITAR_DATA)
                        {

                            int id = int.Parse(param);
                            if (dataToRead == -1) //first one
                            {
                                lm = new List<midiPattern>();
                                nTemp = int.Parse(parsedData[2]);
                                dataToRead = nTemp;
                                skipDeque = true;
                            }
                            else
                            {
                                midiPattern m = new midiPattern();
                                m.midiNote = int.Parse(parsedData[2]);
                                m.offset = int.Parse(parsedData[3]);
                                m.length = int.Parse(parsedData[4]);
                                m.velocity = int.Parse(parsedData[5]);
                                m.channel = int.Parse(parsedData[6]);
                                m.relativeOctave = int.Parse(parsedData[7]);
                                m.lengthTicks = m.length * TPM;
                                lm.Add(m);
                                dataToRead--;
                                skipDeque = true;
                            }
                            if (dataToRead == 0)
                            {
                                dataToRead = -1;
                                dataReceivedTcs.SetResult(true);
                                skipDeque = false;
                            }
                            if (lm.Count > 0 && dataToRead == -1)
                            {
                                if (id == 0)
                                {
                                    Native2WPF.convertStandardToEncoded(lm, ref guitarPattern0);
                                }
                                else if (id == 1)
                                {
                                    Native2WPF.convertStandardToEncoded(lm, ref guitarPattern1);
                                }
                                else
                                {
                                    Native2WPF.convertStandardToEncoded(lm, ref guitarPattern2);
                                }
                            }
                        }
                        else if (command == SerialDevice.GET_DRUM_ID)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                if (int.Parse(param) == 0)
                                {
                                    if (drumPatternRadioButton0.IsChecked == true)
                                    {
                                        curID = nTemp;
                                    }
                                    drumPatternTextBox0.Text = nTemp.ToString();
                                }
                                else if (int.Parse(param) == 1)
                                {
                                    if (drumPatternRadioButton1.IsChecked == true)
                                    {
                                        curID = nTemp;
                                    }
                                    drumPatternTextBox1.Text = nTemp.ToString();
                                }
                                else if (int.Parse(param) == 2)
                                {
                                    if (drumPatternRadioButton2.IsChecked == true)
                                    {
                                        curID = nTemp;
                                    }
                                    drumPatternTextBox2.Text = nTemp.ToString();
                                }
                                else if (int.Parse(param) == 3)
                                {
                                    if (drumPatternRadioButton3.IsChecked == true)
                                    {
                                        curID = nTemp;
                                    }
                                    drumPatternTextBox3.Text = nTemp.ToString();
                                }
                                else
                                {
                                    if (drumPatternRadioButton4.IsChecked == true)
                                    {
                                        curID = nTemp;
                                    }
                                    drumPatternTextBox4.Text = nTemp.ToString();
                                }

                            }
                        }
                        else if (command == SerialDevice.GET_BASS_ID)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                bassPatternTextBox.Text = nTemp.ToString();
                                if (bassPatternRadioButton.IsChecked == true)
                                {
                                    curID = nTemp;
                                }
                            }
                        }
                        else if (command == SerialDevice.GET_BACKING_ID)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                if (backingPatternRadioButton.IsChecked == true)
                                {
                                    curID = nTemp;
                                }
                                nTemp = int.Parse(parsedData[2]);
                                backingPatternTextBox.Text = nTemp.ToString();
                            }
                        }
                        else if (command == SerialDevice.GET_GUITAR_ID)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                int temp = int.Parse(SerialManager.Device.LastParamSent);
                                if (temp == 0)
                                {
                                    guitar0PatternTextBox.Text = nTemp.ToString();
                                    if (guitar0PatternRadioButton.IsChecked == true)
                                    {
                                        curID = nTemp;
                                    }
                                }
                                else if (temp == 1)
                                {
                                    guitar1PatternTextBox.Text = nTemp.ToString();
                                    if (guitar1PatternRadioButton.IsChecked == true)
                                    {
                                        curID = nTemp;
                                    }
                                }
                                else
                                {
                                    if (guitar2PatternRadioButton.IsChecked == true)
                                    {
                                        curID = nTemp;
                                    }
                                    guitar2PatternTextBox.Text = nTemp.ToString();
                                    initFetch = false;
                                    setupRightSide();
                                }
                            }
                        }
                    }
                }
                if (!isDebug && !skipDeque)
                {
                    SerialManager.Device.clearLastCommand();
                }

            });

        }
        private void ComboModeComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            bool isCustom = false;
            if (BackingModeComboBox.SelectedIndex.ToString() == "0")
            {
                isCustom = true;
            }
            if (SerialManager.isDeviceConnected() && !initFetch)
            {
                SendCmd(SerialDevice.SET_BACKING_STATE, BackingModeComboBox.SelectedIndex.ToString());
            }
            backingState = BackingModeComboBox.SelectedIndex;
            if (lastMidiBackingState != backingState && backingState != 0)
            {
                stopPlaying();
                generateOmniBackingMidi(backingState);
                //add test code here
            }
            else if (backingState == 0)
            {
                stopPlaying();
                makeMidi = false;
            }
            if (!isCustom)
            {
                backingPatternRadioButton.IsEnabled = false;
                playButton3.IsEnabled = false;
                playButton4.IsEnabled = false;
                playButton5.IsEnabled = false;
                playButton6.IsEnabled = false;
                playButton7.IsEnabled = false;
                playButton8.IsEnabled = false;
                playButton9.IsEnabled = false;

                drumPatternRadioButton0.IsEnabled = false;
                drumPatternRadioButton1.IsEnabled = false;
                drumPatternRadioButton2.IsEnabled = false;
                drumPatternRadioButton3.IsEnabled = false;
                drumPatternRadioButton4.IsEnabled = false;
                bassPatternRadioButton.IsEnabled = false;
                backingPatternTextBox.IsEnabled = false;
                drumPatternTextBox0.IsEnabled = false;
                drumPatternTextBox1.IsEnabled = false;
                drumPatternTextBox2.IsEnabled = false;
                drumPatternTextBox3.IsEnabled = false;
                drumPatternTextBox4.IsEnabled = false;
                bassPatternTextBox.IsEnabled = false;
                if (backingPatternRadioButton.IsChecked == true || drumPatternRadioButton0.IsChecked == true || drumPatternRadioButton1.IsChecked == true || drumPatternRadioButton2.IsChecked == true || drumPatternRadioButton3.IsChecked == true || drumPatternRadioButton4.IsChecked == true || bassPatternRadioButton.IsChecked == true)
                {
                    guitar0PatternRadioButton.IsChecked = true;
                }
                playBackingButton.IsEnabled = true;
            }
            else
            {
                playButton3.IsEnabled = true;
                playButton4.IsEnabled = true;
                playButton5.IsEnabled = true;
                playButton6.IsEnabled = true;
                playButton7.IsEnabled = true;
                playButton8.IsEnabled = true;
                playButton9.IsEnabled = true;
                playBackingButton.IsEnabled = false;
                backingPatternRadioButton.IsEnabled = true;
                drumPatternRadioButton0.IsEnabled = true;
                drumPatternRadioButton1.IsEnabled = true;
                drumPatternRadioButton2.IsEnabled = true;
                drumPatternRadioButton3.IsEnabled = true;
                drumPatternRadioButton4.IsEnabled = true;
                bassPatternRadioButton.IsEnabled = true;
                backingPatternTextBox.IsEnabled = true;
                drumPatternTextBox0.IsEnabled = true;
                drumPatternTextBox1.IsEnabled = true;
                drumPatternTextBox2.IsEnabled = true;
                drumPatternTextBox3.IsEnabled = true;
                drumPatternTextBox4.IsEnabled = true;
                bassPatternTextBox.IsEnabled = true;
            }
        }
        private void SendCmd(string cmd, string param = "")
        {
            if (!SerialManager.isDeviceConnected())
            {
                //DebugLog.addToLog(debugType.sendDebug, "Error! Tried to send " + cmd + " while device is disconected.");
                SerialManager.Device.RaiseDeviceDisconnected();
                return;
            }
            try
            {
                SerialManager.Device.Send(cmd, param); // Or your custom heartbeat command
            }
            catch (Exception)
            {

            }
        }

        protected override void OnClosed(EventArgs e)
        {
            stopPlaying();
            //end handling serial
            if (SerialManager.isDeviceConnected())
            {
                SerialManager.Device.DataReceived -= OnPatternWindowSerialDataReceived;
            }
            base.OnClosed(e);
        }

        private void closeButton_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
        private void UpdatePlayButtonGraphic(Button sender)
        {
            var uri = new Uri(!isPlaying ? "Images/pause.png" : "Images/play.png", UriKind.Relative);
            ((Image)sender.Content).Source = new BitmapImage(uri);
        }
        private void UpdatePlayButtonGraphicStopAll()
        {
            for (int i = 0; i < allPlayButtons.Count; i++)
            {
                var uri = new Uri("Images/play.png", UriKind.Relative);
                ((Image)allPlayButtons[i].Content).Source = new BitmapImage(uri);
            }
            var uri2 = new Uri("Images/play.png", UriKind.Relative);
            ((Image)playSampleButton.Content).Source = new BitmapImage(uri2);

        }

        private async Task CheckIfFinished()
        {
            while (isPlaying)
            {
                await Task.Delay(500);

                StringBuilder posBuilder = new StringBuilder(128);
                StringBuilder lenBuilder = new StringBuilder(128);

                // Get current position
                mciSendString($"status {midiAlias} position", posBuilder, posBuilder.Capacity, IntPtr.Zero);

                // Get total length
                mciSendString($"status {midiAlias} length", lenBuilder, lenBuilder.Capacity, IntPtr.Zero);

                if (int.TryParse(posBuilder.ToString(), out int position) &&
                    int.TryParse(lenBuilder.ToString(), out int length))
                {
                    if (position >= length)
                    {
                        isPlaying = false;
                        Dispatcher.Invoke(() =>
                        {
                            mciSendString($"stop {midiAlias}", null, 0, IntPtr.Zero);
                            mciSendString($"close {midiAlias}", null, 0, IntPtr.Zero);
                            UpdatePlayButtonGraphicStopAll();
                        });
                        break;
                    }
                }
            }
        }

        private void stopPlaying()
        {
            // Stop MIDI
            mciSendString($"stop {midiAlias}", null, 0, IntPtr.Zero);
            mciSendString($"close {midiAlias}", null, 0, IntPtr.Zero);
            isPlaying = false;
            UpdatePlayButtonGraphicStopAll();
        }

        private void playBackingButton_Click(object sender, RoutedEventArgs e)
        {
            playButton_Click(sender, e);
        }

        void generateBackingMidi()
        {
            //todo:
            //determine which is selected
            //generate struct data from the passed data
            //convert to the target struct
            //convert to midi
        }
        private void playGeneric_Click(object sender, RoutedEventArgs e)
        {
            playButton_Click(sender, e);
        }
        private void playButton_Click(object sender, RoutedEventArgs e)
        {

            string path = tempPath + "\\pattern.mid";
            bool isBacking = false;

            if (sender == playSampleButton)
            {
                Native2WPF.changeEncodedChannel(ref tempPattern, selected_Channel);
                tempPatternPreview.Clear();
                //convert tempPattern to midi first

                Native2WPF.convertEncodedToStandard(tempPattern, ref tempPatternPreview, false);
                Native2WPF.converted = new List<midiPattern>(tempPatternPreview);
                path = tempPath + "preview.mid";
                Native2WPF.convertMidiPatternToMidi(path, int.Parse(bpmTextBox.Text), true, true);

            }
            else if (sender == playBackingButton)
            {
                path = tempPath + "omni.mid";
                isBacking = true;
            }
            else
            {
                int myChannel = GUITAR_CHANNEL;
                bool convertPlaceholder = true;
                //determine which button it is:
                if (sender == playButton0) //guitar 0
                {
                    tempPattern = new List<EncodedNote>(guitarPattern0);
                }
                else if (sender == playButton1) //guitar 1
                {
                    tempPattern = new List<EncodedNote>(guitarPattern1);
                }
                else if (sender == playButton2) //guitar 2
                {
                    tempPattern = new List<EncodedNote>(guitarPattern2);
                }
                else if (sender == playButton3) //bass
                {
                    myChannel = BASS_CHANNEL;
                    tempPattern = new List<EncodedNote>(bassPattern);
                }
                else if (sender == playButton4) //backing
                {
                    myChannel = BACKING_CHANNEL;
                    tempPattern = new List<EncodedNote>(backingPattern);
                }
                else if (sender == playButton5) //drum intro
                {
                    myChannel = DRUMS_CHANNEL;
                    convertPlaceholder = false;
                    tempPattern = new List<EncodedNote>(drumPattern0);
                }
                else if (sender == playButton6) //drum loop
                {
                    myChannel = DRUMS_CHANNEL;
                    convertPlaceholder = false;
                    tempPattern = new List<EncodedNote>(drumPattern1);
                }
                else if (sender == playButton7) //drum loop half
                {
                    myChannel = DRUMS_CHANNEL;
                    convertPlaceholder = false;
                    tempPattern = new List<EncodedNote>(drumPattern2);
                }
                else if (sender == playButton8) //drum fill
                {
                    myChannel = DRUMS_CHANNEL;
                    convertPlaceholder = false;
                    tempPattern = new List<EncodedNote>(drumPattern3);
                }
                else //drums end
                {
                    myChannel = DRUMS_CHANNEL;
                    convertPlaceholder = false;
                    tempPattern = new List<EncodedNote>(drumPattern4);
                }
                if (tempPattern.Count == 0)
                {
                    MessageBox.Show((string)Application.Current.Resources["NoPatternData"]);
                    return;
                }
                Native2WPF.changeEncodedChannel(ref tempPattern, myChannel);
                tempPatternPreview.Clear();
                Native2WPF.convertEncodedToStandard(tempPattern, ref tempPatternPreview, false, convertPlaceholder);
                Native2WPF.converted = new List<midiPattern>(tempPatternPreview);
                path = tempPath + "internal.mid";
                Native2WPF.convertMidiPatternToMidi(path, int.Parse(bpmTextBox.Text), true, true);
            }
            int newBPM = int.Parse(bpmTextBox.Text);

            if (isBacking)
            {
                bool needsRegenerate = !makeMidi || (lastMidiBackingState != backingState) || (oldBPM != newBPM);
                if (needsRegenerate)
                {
                    generateOmniBackingMidi(backingState);
                    oldBPM = newBPM; // Update BPM tracker
                }
            }

            if (!isPlaying)
            {
                // Stop any current playback
                mciSendString($"stop {midiAlias}", null, 0, IntPtr.Zero);
                mciSendString($"close {midiAlias}", null, 0, IntPtr.Zero);
                isPlaying = false;
                isPaused = false;
                UpdatePlayButtonGraphicStopAll();
                UpdatePlayButtonGraphic((Button)sender); // Change icon to play

            }
            if (isPlaying)
            {
                stopPlaying();
            }
            else
            {
                // Ensure any previous instance is closed
                mciSendString($"close {midiAlias}", null, 0, IntPtr.Zero);
                // Open and play
                mciSendString($"open \"{path}\" type sequencer alias {midiAlias}", null, 0, IntPtr.Zero);
                mciSendString($"play {midiAlias} notify", null, 0, hwnd);
                isPlaying = true;

                // Start monitor task to detect when playback finishes
                Task.Run(CheckIfFinished);
            }
        }

        private void importMidiButton_Click(object sender, RoutedEventArgs e)
        {
            var dlg = new OpenFileDialog
            {
                Filter = (string)Application.Current.Resources["MidiDLG"]
            };
            if (dlg.ShowDialog() == true)
            {

                if (Native2WPF.convertMidiToMidiPattern(dlg.FileName, selected_Channel) && Native2WPF.converted.Count > 0)
                {
                    bool noPlaceholder = false;
                    if (selected_Channel != DRUMS_CHANNEL)
                    {
                        Native2WPF.convertStandardToEncoded(Native2WPF.convertedPlaceholder, ref importPatternPlaceholder, false);
                        Native2WPF.convertStandardToEncoded(Native2WPF.converted, ref importPattern);
                    }
                    else
                    {
                        importPatternPlaceholder.Clear();
                        Native2WPF.convertStandardToEncoded(Native2WPF.converted, ref importPattern, false);
                    }

                    var listBoxItem = new ListBoxItem
                    {
                        Content = $"{firstFreeSlot} - <Imported>",
                        Tag = -2 // import
                    };

                    patternListBox.Items.Add(listBoxItem);
                    patternListBox.SelectedItem = listBoxItem;
                    patternListBox.ScrollIntoView(listBoxItem);

                }
            }
        }

        private void deleteButton_Click(object sender, RoutedEventArgs e)
        {
            if (patternListBox.SelectedIndex < 0 || (int)((System.Windows.Controls.ListBoxItem)patternListBox.SelectedItem).Tag == -1)
            {
                MessageBox.Show("Error! Selected index is invalid");
                return;
            }
            int id = patternDataList[patternListBox.SelectedIndex].id;
            int oldIndex = patternListBox.SelectedIndex;
            string path = patternDataList[patternListBox.SelectedIndex].path;

            if (Directory.Exists(path))
            {
                Directory.Delete(path, true);
                patternDataList.RemoveAt(oldIndex);
            }
            int newID = -1;
            if (oldIndex == patternDataList.Count())
            {
                if (internalHasNotes)
                {
                    newID = 0;
                }
                else
                {
                    newID = patternDataList[oldIndex - 1].id;
                }
            }
            else if (oldIndex > patternDataList.Count())
            {
                newID = patternDataList[patternDataList.Count() - 1].id;
            }
            else
            {
                newID = patternDataList[oldIndex + 1].id;
            }
            updateRightSide(newID);
        }

        private void uploadButton_Click(object sender, RoutedEventArgs e)
        {
            if (tempPatternPlaceholder.Count < 0)
            {
                MessageBox.Show((string)Application.Current.Resources["NoPatternToSend"]);
                return;
            }
            int id = patternDataList[patternListBox.SelectedIndex].id;
            if (SerialManager.isDeviceConnected())
            {
                string param = "";
                SendCmd(clearCMD, selected_Slot.ToString());
                Native2WPF.convertEncodedToStandard(tempPattern, ref tempPatternSend, false);
                for (int i = 0; i < tempPatternSend.Count; i++)
                {
                    param = selected_Slot.ToString();
                    param += ',';
                    param += tempPatternSend[i].midiNote.ToString();
                    param += ',';
                    param += tempPatternSend[i].offset;
                    param += ',';
                    param += tempPatternSend[i].length;
                    param += ',';
                    param += tempPatternSend[i].velocity;
                    param += ',';
                    param += tempPatternSend[i].relativeOctave;
                    SendCmd(addCMD, param);
                }
                if (selected_Channel == DRUMS_CHANNEL)
                {
                    SendCmd(SerialDevice.SET_DRUM_ID, selected_Channel.ToString() + "," + id.ToString());
                    SendCmd(SerialDevice.GET_DRUM_ID, selected_Channel.ToString());
                }
                else if (selected_Channel == GUITAR_CHANNEL)
                {
                    SendCmd(SerialDevice.SET_GUITAR_ID, selected_Channel.ToString() + "," + id.ToString());
                    SendCmd(SerialDevice.GET_GUITAR_ID, selected_Channel.ToString());
                }
                else if (selected_Channel == BASS_CHANNEL)
                {
                    SendCmd(SerialDevice.SET_BASS_ID, selected_Channel.ToString() + "," + id.ToString());
                    SendCmd(SerialDevice.GET_BASS_ID);
                }
                else if (selected_Channel == BACKING_CHANNEL)
                {
                    SendCmd(SerialDevice.SET_BACKING_ID, selected_Channel.ToString() + "," + id.ToString());
                    SendCmd(SerialDevice.GET_BACKING_ID);
                }
                curID = id;

            }
        }

        private void nextButton_Click(object sender, RoutedEventArgs e)
        {
            int cnt = patternListBox.Items.Count;
            if (patternListBox.SelectedIndex + 1 < cnt)
            {
                patternListBox.SelectedIndex = patternListBox.SelectedIndex + 1;
                patternListBox.ScrollIntoView(patternListBox.Items[patternListBox.SelectedIndex]);
            }
        }

        private void previousButton_Click(object sender, RoutedEventArgs e)
        {
            int cnt = patternListBox.Items.Count;
            if (patternListBox.SelectedIndex - 1 >= 0)
            {
                patternListBox.SelectedIndex = patternListBox.SelectedIndex - 1;
                patternListBox.ScrollIntoView(patternListBox.Items[patternListBox.SelectedIndex]);
            }
        }

        private void saveButton_Click(object sender, RoutedEventArgs e)
        {
            if (curIDExists && patternListBox.SelectedItem is ListBoxItem selectedItem)
            {
                // Try to extract the index from the Tag
                if (selectedItem.Tag is int patternIndex && patternIndex >= 0 && patternIndex < patternDataList.Count)
                {
                    string folderPath = patternDataList[patternIndex].path;
                    string nameFilePath = System.IO.Path.Combine(folderPath, "Name.txt");

                    try
                    {
                        File.WriteAllText(nameFilePath, patternNameTextBox.Text.Trim());
                        MessageBox.Show((string)Application.Current.Resources["PatternNameSaved"]);
                        patternData pd = patternDataList[patternIndex];
                        pd.name = patternNameTextBox.Text.Trim();
                        patternDataList[patternIndex] = pd;
                        ListBoxItem lb = (ListBoxItem)patternListBox.SelectedItem;
                        lb.Content = $"{pd.id} - {pd.name}";

                    }
                    catch (Exception ex)
                    {
                        MessageBox.Show($"Failed to save name: {ex.Message}");
                    }
                }

            }
            else if (patternListBox.SelectedItem is ListBoxItem selectedItem2)
            {
                string folderPath = basePath + "\\" + curPatternPath + "\\" + firstFreeSlot.ToString();
                string nameFilePath = System.IO.Path.Combine(folderPath, "Name.txt");

                try
                {
                    //assumes copying from the guitar, so no conversion needed
                    if (tempPattern.Count > 0 && tempPatternPlaceholder.Count == 0)
                    {
                        MessageBox.Show("Error! Temp Pattern placeholder is empty!");

                    }
                    if (!Directory.Exists(folderPath))
                    {
                        Directory.CreateDirectory(folderPath);
                    }
                    File.WriteAllText(nameFilePath, patternNameTextBox.Text.Trim());
                    MessageBox.Show((string)Application.Current.Resources["PatternNameSaved"]);
                    Native2WPF.convertEncodedToStandard(tempPatternPlaceholder, ref tempPatternSend, false);
                    Native2WPF.converted = new List<midiPattern>(tempPatternSend);
                    string midipath = folderPath + "\\data.mid";
                    Native2WPF.convertMidiPatternToMidi(midipath, int.Parse(bpmTextBox.Text), false, false);
                    //curID = firstFreeSlot;
                    patternDataList.Clear();
                    updateRightSide(firstFreeSlot);
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Failed to save name: {ex.Message}");
                }
            }
        }
        private void setupRadioButtons()
        {
            guitar0PatternRadioButton.Checked += patternRadioButton_Checked;
            guitar1PatternRadioButton.Checked += patternRadioButton_Checked;
            guitar2PatternRadioButton.Checked += patternRadioButton_Checked;
            drumPatternRadioButton0.Checked += patternRadioButton_Checked;
            drumPatternRadioButton1.Checked += patternRadioButton_Checked;
            drumPatternRadioButton2.Checked += patternRadioButton_Checked;
            drumPatternRadioButton3.Checked += patternRadioButton_Checked;
            drumPatternRadioButton4.Checked += patternRadioButton_Checked;
            bassPatternRadioButton.Checked += patternRadioButton_Checked;
            backingPatternRadioButton.Checked += patternRadioButton_Checked;
        }
        private void patternRadioButton_Checked(object sender, RoutedEventArgs e)
        {
            importPattern.Clear();
            importPatternPlaceholder.Clear();
            if (!doneLoading) return;
            selected_Slot = 0;
            if ((RadioButton)sender == guitar0PatternRadioButton)
            {
                selected_Channel = GUITAR_CHANNEL;
                clearCMD = SerialDevice.CLEAR_GUITAR_DATA;
                addCMD = SerialDevice.ADD_GUITAR_DATA;
                if (SerialManager.isDeviceConnected())
                {
                    curID = int.Parse(guitar0PatternTextBox.Text);
                }

                curPatternPath = "guitar";
            }
            else if ((RadioButton)sender == guitar1PatternRadioButton)
            {
                clearCMD = SerialDevice.CLEAR_GUITAR_DATA;
                addCMD = SerialDevice.ADD_GUITAR_DATA;
                selected_Channel = GUITAR_CHANNEL;
                selected_Slot = 1;
                if (SerialManager.isDeviceConnected())
                {
                    curID = int.Parse(guitar1PatternTextBox.Text);
                }
                curPatternPath = "guitar";
            }
            else if ((RadioButton)sender == guitar2PatternRadioButton)
            {
                clearCMD = SerialDevice.CLEAR_GUITAR_DATA;
                addCMD = SerialDevice.ADD_GUITAR_DATA;
                selected_Channel = GUITAR_CHANNEL;
                selected_Slot = 2;
                if (SerialManager.isDeviceConnected())
                {
                    curID = int.Parse(guitar2PatternTextBox.Text);
                }
                curPatternPath = "guitar";
            }
            else if ((RadioButton)sender == bassPatternRadioButton)
            {
                if (SerialManager.isDeviceConnected())
                {
                    curID = int.Parse(bassPatternTextBox.Text);
                }
                clearCMD = SerialDevice.CLEAR_BASS_DATA;
                addCMD = SerialDevice.ADD_BASS_DATA;
                selected_Channel = BASS_CHANNEL;
                curPatternPath = "bass";
            }
            else if ((RadioButton)sender == backingPatternRadioButton)
            {
                clearCMD = SerialDevice.CLEAR_BACKING_DATA;
                addCMD = SerialDevice.ADD_BACKING_DATA;
                selected_Channel = BACKING_CHANNEL;
                if (SerialManager.isDeviceConnected())
                {
                    curID = int.Parse(backingPatternTextBox.Text);
                }
                curPatternPath = "backing";

            }
            else if ((RadioButton)sender == drumPatternRadioButton0)
            {
                if (SerialManager.isDeviceConnected())
                {
                    curID = int.Parse(drumPatternTextBox0.Text);
                }
                clearCMD = SerialDevice.CLEAR_DRUMS_DATA;
                addCMD = SerialDevice.ADD_DRUMS_DATA;
                selected_Channel = DRUMS_CHANNEL;
                curPatternPath = "drum_intro";
            }

            else if ((RadioButton)sender == drumPatternRadioButton1)
            {
                if (SerialManager.isDeviceConnected())
                {
                    curID = int.Parse(drumPatternTextBox1.Text);
                }
                clearCMD = SerialDevice.CLEAR_DRUMS_DATA;
                addCMD = SerialDevice.ADD_DRUMS_DATA;
                selected_Channel = DRUMS_CHANNEL;
                selected_Slot = 1;
                curPatternPath = "drum_loop";
            }
            else if ((RadioButton)sender == drumPatternRadioButton2)
            {
                if (SerialManager.isDeviceConnected())
                {
                    curID = int.Parse(drumPatternTextBox2.Text);
                }
                clearCMD = SerialDevice.CLEAR_DRUMS_DATA;
                addCMD = SerialDevice.ADD_DRUMS_DATA;
                selected_Channel = DRUMS_CHANNEL;
                selected_Slot = 2;
                curPatternPath = "drum_loop_half";
            }
            else if ((RadioButton)sender == drumPatternRadioButton3)
            {
                if (SerialManager.isDeviceConnected())
                {
                    curID = int.Parse(drumPatternTextBox3.Text);
                }
                clearCMD = SerialDevice.CLEAR_DRUMS_DATA;
                addCMD = SerialDevice.ADD_DRUMS_DATA;
                selected_Channel = DRUMS_CHANNEL;
                selected_Slot = 3;
                curPatternPath = "drum_fill";
            }
            else if ((RadioButton)sender == drumPatternRadioButton4)
            {
                if (SerialManager.isDeviceConnected())
                {
                    curID = int.Parse(drumPatternTextBox4.Text);
                }
                clearCMD = SerialDevice.CLEAR_DRUMS_DATA;
                addCMD = SerialDevice.ADD_DRUMS_DATA;
                selected_Channel = DRUMS_CHANNEL;
                selected_Slot = 4;
                curPatternPath = "drum_end";
            }
            updateRightSide();
        }

        private void LoadPatterns(int selectedID = -1)
        {
            // Clear previous entries
            patternListBox.Items.Clear();
            patternDataList.Clear();

            // Construct full path
            string rootPath = System.IO.Path.Combine(Directory.GetCurrentDirectory(), basePath, curPatternPath);

            if (selected_Channel == GUITAR_CHANNEL)
            {
                if (selected_Slot == 0)
                {
                    internalHasNotes = guitarPattern0.Count > 0;
                }
                else if (selected_Slot == 1)
                {
                    internalHasNotes = guitarPattern1.Count > 0;
                }
                else
                {
                    internalHasNotes = guitarPattern2.Count > 0;
                }
            }
            else if (selected_Channel == DRUMS_CHANNEL)
            {
                if (selected_Slot == 0)
                {
                    internalHasNotes = drumPattern0.Count > 0;
                }
                else if (selected_Slot == 1)
                {
                    internalHasNotes = drumPattern1.Count > 0;
                }
                else if (selected_Slot == 2)
                {
                    internalHasNotes = drumPattern2.Count > 0;
                }
                else if (selected_Slot == 3)
                {
                    internalHasNotes = drumPattern3.Count > 0;
                }
                else
                {
                    internalHasNotes = drumPattern4.Count > 0;
                }
            }
            else if (selected_Channel == BASS_CHANNEL)
            {
                internalHasNotes = bassPattern.Count > 0;
            }
            else if (selected_Channel == BACKING_CHANNEL)
            {
                internalHasNotes = backingPattern.Count > 0;
            }
            if (!Directory.Exists(rootPath))
            {
                MessageBox.Show($"Directory does not exist: {rootPath}");
                return;
            }
            firstFreeSlot = -1;
            // Loop through folders 1 to 65535
            for (int i = 1; i <= 65535; i++)
            {
                string folderPath = System.IO.Path.Combine(rootPath, i.ToString());
                string midiPath = System.IO.Path.Combine(folderPath, "data.mid");
                string namePath = System.IO.Path.Combine(folderPath, "Name.txt");

                if (Directory.Exists(folderPath) &&
                    File.Exists(midiPath) &&
                    File.Exists(namePath))
                {
                    try
                    {
                        string name = File.ReadAllText(namePath).Trim();

                        var data = new patternData
                        {
                            name = name,
                            path = folderPath,
                            id = i
                        };

                        patternDataList.Add(data);

                        var listBoxItem = new ListBoxItem
                        {
                            Content = $"{i} - {name}",
                            Tag = patternDataList.Count - 1 // index into patternDataList
                        };

                        patternListBox.Items.Add(listBoxItem);
                    }
                    catch (Exception ex)
                    {
                        MessageBox.Show($"Error reading folder {i}: {ex.Message}");
                    }
                }
                else if (!Directory.Exists(folderPath) && firstFreeSlot == -1)
                {
                    firstFreeSlot = i;
                }
            }
            if (curID == 0 && internalHasNotes)
            {
                var listBoxItem = new ListBoxItem
                {
                    Content = $"0 - ????",
                    Tag = -1 // index into patternDataList
                };
                patternListBox.Items.Add(listBoxItem);
            }
            // Selection logic (moved outside loop)
            //            if (hasInternal && curID == 0)
            if (curID == 0 && internalHasNotes && selectedID == -1)
            {
                curIDExists = false;
                patternListBox.SelectedIndex = patternListBox.Items.Count - 1;
                patternListBox.ScrollIntoView(patternListBox.Items[patternListBox.Items.Count - 1]);
            }
            else if (curID != 0 && selectedID == -1)
            {
                bool found = false;
                foreach (var item in patternListBox.Items)
                {
                    if (item is ListBoxItem listBoxItem && listBoxItem.Content is string content)
                    {
                        var parts = content.Split(new[] { " - " }, StringSplitOptions.None);
                        if (parts.Length >= 2 && int.TryParse(parts[0], out int patternId))
                        {
                            if (patternId == curID)
                            {
                                found = true;
                                patternListBox.SelectedItem = listBoxItem;
                                patternListBox.ScrollIntoView(listBoxItem);
                                curIDExists = true;
                                break;
                            }
                        }
                    }
                }

                if (!found) //if pattern was saved but not on PC, assume it's like 0 and can be saved
                {
                    curIDExists = false;
                    var tempBoxItem = new ListBoxItem
                    {
                        Content = $"X - ????",
                        Tag = -1
                    };
                    patternListBox.Items.Add(tempBoxItem);
                    patternListBox.SelectedIndex = patternListBox.Items.Count - 1;
                    patternListBox.ScrollIntoView(tempBoxItem);
                }
            }
            else if (selectedID != -1)
            {
                bool found = false;
                foreach (var item in patternListBox.Items)
                {
                    if (item is ListBoxItem listBoxItem && listBoxItem.Content is string content)
                    {
                        var parts = content.Split(new[] { " - " }, StringSplitOptions.None);
                        if (parts.Length >= 2 && int.TryParse(parts[0], out int patternId))
                        {
                            if (patternId == selectedID)
                            {
                                found = true;
                                patternListBox.SelectedItem = listBoxItem;
                                patternListBox.ScrollIntoView(listBoxItem);
                                break;
                            }
                        }
                    }
                }
            }
            if (patternListBox.Items.Count == 0)
            {
                NoteCanvas.Children.Clear();
                MessageBox.Show((string)Application.Current.Resources["NoPatternDataAtAll"]);
            }
        }


    }
}
