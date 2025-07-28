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



namespace CyberG
{

    /// <summary>
    /// Interaction logic for PatternWindow.xaml
    /// </summary>
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
                isPlaying = false;
                Dispatcher.Invoke(() => UpdatePlayButtonGraphicStopAll());
                handled = true;
            }
            return IntPtr.Zero;
        }
        private List<midiPattern> lm = new List<midiPattern>();
        private const int TPM = 12;
        private TaskCompletionSource<bool> dataReceivedTcs;
        private int patternLeftToRead = 0;
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
        private List<Button> allLeftPlayButtons = new List<Button>();
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
        public PatternWindow(int bpm)
        {
            isPatternReading = false;
            dataToRead = -1;
            dataRead = -1;
            InitializeComponent();
            setupComboBoxes();
            allLeftPlayButtons.Add(playBackingButton);
            allLeftPlayButtons.Add(playButton0);
            allLeftPlayButtons.Add(playButton1);
            allLeftPlayButtons.Add(playButton2);
            allLeftPlayButtons.Add(playButton3);
            allLeftPlayButtons.Add(playButton4);
            allLeftPlayButtons.Add(playButton5);
            allLeftPlayButtons.Add(playButton6);
            allLeftPlayButtons.Add(playButton7);
            allLeftPlayButtons.Add(playButton8);
            allLeftPlayButtons.Add(playButton9);
            guitar0PatternRadioButton.IsChecked = true;
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
            /*
            SendCmd(SerialDevice.GET_GUITAR_DATA, "1");
            await dataReceivedTcs.Task;
            SendCmd(SerialDevice.GET_GUITAR_DATA, "2");
            await dataReceivedTcs.Task;
            */
            if (backingState == 0)
            {
                /*
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
                */
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
                    m.lengthTicks = int.Parse(parsedData[4]);
                    m.velocity = int.Parse(parsedData[5]);
                    m.channel = int.Parse(parsedData[6]);
                    m.relativeOctave = int.Parse(parsedData[7]);
                    m.length = m.lengthTicks * TPM;
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
            else if (command == SerialDevice.GET_DRUM_ID)
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
            else if (command == SerialDevice.GET_BACKING_ID || command == SerialDevice.GET_BASS_ID || command == SerialDevice.GET_GUITAR_ID)
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
            Dispatcher.Invoke(() => {
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
                                m.lengthTicks = int.Parse(parsedData[4]);
                                m.velocity = int.Parse(parsedData[5]);
                                m.channel = int.Parse(parsedData[6]);
                                m.relativeOctave = int.Parse(parsedData[7]);
                                m.length = m.lengthTicks * TPM;
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
                                    drumPatternTextBox0.Text = nTemp.ToString();
                                }
                                else if (int.Parse(param) == 1)
                                {
                                    drumPatternTextBox1.Text = nTemp.ToString();
                                }
                                else if (int.Parse(param) == 2)
                                {
                                    drumPatternTextBox2.Text = nTemp.ToString();
                                }
                                else if (int.Parse(param) == 3)
                                {
                                    drumPatternTextBox3.Text = nTemp.ToString();
                                }
                                else
                                {
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
                            }
                        }
                        else if (command == SerialDevice.GET_BACKING_ID)
                        {
                            if (checkReceivedValid(parsedData))
                            {
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
                                }
                                else if (temp == 1)
                                {
                                    guitar1PatternTextBox.Text = nTemp.ToString();
                                }
                                else
                                {
                                    guitar2PatternTextBox.Text = nTemp.ToString();
                                    initFetch = false;
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
            }
            else if (backingState == 0)
            {
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
            for (int i = 0; i < allLeftPlayButtons.Count; i++)
            {
                var uri = new Uri("Images/play.png", UriKind.Relative);
                ((Image)allLeftPlayButtons[i].Content).Source = new BitmapImage(uri);
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
            /*
            string path = tempPath + "\\omni.mid";
            
            int newBPM = int.Parse(bpmTextBox.Text);
            
            if (!isPlaying)
            {
                bool needsRegenerate = !makeMidi || (lastMidiBackingState != backingState) || (oldBPM != newBPM);
                if (needsRegenerate)
                {
                    // Stop any current playback
                    mciSendString($"stop {midiAlias}", null, 0, IntPtr.Zero);
                    mciSendString($"close {midiAlias}", null, 0, IntPtr.Zero);
                    isPlaying = false;
                    isPaused = false;
                    UpdatePlayButtonGraphic((Button) sender); // Change icon to play
                    makeMidi = true;
                    generateOmniBackingMidi(backingState);
                    oldBPM = newBPM; // Update BPM tracker
                }

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
                UpdatePlayButtonGraphic();
                // Start monitor task to detect when playback finishes
                Task.Run(CheckIfFinished);
            }
            */
        }

        void generateBackingMidi()
        {
            //todo:
            //determine which is selected
            //generate struct data from the passed data
            //convert to the target struct
            //convert to midi
        }
        private void playButton_Click(object sender, RoutedEventArgs e)
        {
            bool isBacking = false;
            string path = tempPath + "\\pattern.mid";
            if (sender == playBackingButton)
            {
                path = tempPath + "\\omni.mid";
                isBacking = true;
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
            else
            {
                bool needsRegenerate = (lastMidiBackingState != backingState) || (oldBPM != newBPM);
                if (needsRegenerate)
                {
                    generateBackingMidi();
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
    }
}
