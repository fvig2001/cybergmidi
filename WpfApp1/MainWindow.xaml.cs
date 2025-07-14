using Microsoft.Win32;
using System.Collections.Generic;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Windows.Threading;

namespace CyberG
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    /// 
    public partial class MainWindow : Window
    {
        private const int SAVEWINDOWID = 0;
        private const int MaxBackingState = 10;
        private const int MaxPatternID = 65535;
        private const int MaxStrumPatternVal = 2;
        private const int MaxNoteVal = 11;
        private const int MaxChordVal = 29;
        private const int NECK_ROWS = 7;
        private const int NECK_COLS = 3;
        private const int MAX_MODE_VALUE = 3;
        private const string LAST_PRESET_CMD = "BPMR";
        private bool isSerialEnabled = false; //disable pinging of keyboard and preset
        private bool isKeyboard = false;
        private bool isPresetReading = false;
        private bool isLoadingFile = false;
        private DebugLogWindow _debugLogWindow;
        private SerialLogWindow _serialLogWindow;
        private DispatcherTimer _serialPingTimer;
        private const int MaxPresetCount = 6;
        private const int MaxGuitarPatternCount = 3;
        private List<ComboBox> allComboRoots;
        private List<ComboBox> allComboChords;
        private List<ComboBox> allComboPatterns;
        private const int MaxBPM = 300;
        private const int MinBPM = 50;
        private const int MaxStrumStyle = 2;
        private int curBPM = 50;
        public int curPreset = -1;
        private const int MaxKBTranspose = 2;
        private const int MinKBTranspose = -2;
        private const int MaxCapoTranspose = 12;
        private const int MinCapoTranspose = -12;
        private const int MinStrumSeparation = 0;
        private const int MaxStrumSeparation = 100;
        private const int MinMuteSeparation = 0;
        private const int MaxMuteSeparation = 50;

        private DispatcherTimer _resetTimer;
        private readonly List<DateTime> _tapTimes = new List<DateTime>();
        private const int MinTaps = Config.MAINWINDOW_BPM_TAPS_MIN;
        private volatile bool isExpectingSerialData = false;
        private bool initDone = false;
        public int drumPatternID = -1;
        public int bassPatternID = -1;
        public int backingPatternID = -1;
        public int[] guitarPatternID = { -1, -1, -1 };
        public int lastdrumPatternID = -1;
        public int lastbassPatternID = -1;
        public int lastbackingPatternID = -1;
        public int[] lastguitarPatternID = { -1, -1, -1 };
        public int backingState = 0;
        public MainWindow()
        {
            SetLanguage("en"); //default english
            this.PreviewKeyDown += MainWindow_PreviewKeyDown;
            InitializeComponent();
            SetupComponents();
            Loaded += MainWindow_Loaded;
            if (!SerialManager.isInitialized())
            {
                //open window here to force connection
                //SerialManager.Initialize("COM3", 9600, true);
                openConnectionWindow();
                SendCmd(SerialDevice.GET_PRESET);
            }
            if (!Config.NODEVICEMODE)
            {
                if (SerialManager.isDeviceConnected())
                {
                    StartSerialPingTimer();
                }
            }
            initDone = true;
        }

        private void saveButton_Click(object sender, RoutedEventArgs e)
        {
            var dlg = new SaveFileDialog
            {
                Filter = "JSON files (*.json)|*.json|All files (*.*)|*.*",
                DefaultExt = ".json",
                FileName = "controls.json"
            };

            if (dlg.ShowDialog() == true)
            {
                ControlStateSerializer.SaveControlStates(this, dlg.FileName, SAVEWINDOWID);
                Dictionary<string, string> toSave = new Dictionary<string, string>();
                toSave["drumPatternID"] = drumPatternID.ToString();
                toSave["backingPatternID"] = backingPatternID.ToString();
                toSave["bassPatternID"] = bassPatternID.ToString();
                toSave["guitarPatternID0"] = guitarPatternID[0].ToString();
                toSave["guitarPatternID1"] = guitarPatternID[1].ToString();
                toSave["guitarPatternID2"] = guitarPatternID[2].ToString();
                ControlStateSerializer.SaveManualSettings(dlg.FileName, toSave);
            }

        }

        private void loadButton_Click(object sender, RoutedEventArgs e)
        {
            var dlg = new OpenFileDialog
            {
                Filter = "JSON files (*.json)|*.json|All files (*.*)|*.*"
            };

            if (dlg.ShowDialog() == true)
            {
                isSerialEnabled = false;
                isLoadingFile = true;
                ChangePicDisplayed();
                bool ret = ControlStateSerializer.LoadControlStates(this, dlg.FileName, SAVEWINDOWID);
                isLoadingFile = false;
                isSerialEnabled = true;
                ChangePicDisplayed();
                if (!ret)
                {
                    MessageBox.Show((string)Application.Current.Resources["ErrLoadFile"]);
                    Dictionary<string, string> loadedSettings = ControlStateSerializer.LoadManualSettings(dlg.FileName);
                    if (loadedSettings.TryGetValue("drumPatternID", out string drumVal))
                    {
                        lastdrumPatternID = drumPatternID;
                        drumPatternID = int.Parse(drumVal);
                    }

                    if (loadedSettings.TryGetValue("backingPatternID", out string backingVal))
                    {
                        lastbackingPatternID = backingPatternID;
                        backingPatternID = int.Parse(backingVal); 
                    }

                    if (loadedSettings.TryGetValue("bassPatternID", out string bassVal))
                    {
                        lastbassPatternID = backingPatternID;
                        bassPatternID = int.Parse(bassVal);
                    }

                    if (loadedSettings.TryGetValue("guitarPatternID0", out string g0))
                    {
                        lastguitarPatternID[0] = backingPatternID;
                        guitarPatternID[0] = int.Parse(g0);
                    }
                    if (loadedSettings.TryGetValue("guitarPatternID1", out string g1))
                    {
                        lastguitarPatternID[1] = backingPatternID;
                        guitarPatternID[1] = int.Parse(g1);
                    }

                    if (loadedSettings.TryGetValue("guitarPatternID2", out string g2))
                    {
                        lastguitarPatternID[2] = backingPatternID;
                        guitarPatternID[2] = int.Parse(g2);
                    }
                    //todo Add code to load the pattern from the library if it is not the same:
                    if (lastdrumPatternID != drumPatternID && lastdrumPatternID > 0)
                    { 
                        //load drum pattern
                    }
                    if (lastbackingPatternID != backingPatternID && lastbackingPatternID > 0)
                    {
                        //load backing pattern
                    }
                    if (lastbassPatternID != bassPatternID && lastbassPatternID > 0)
                    {
                        //load bass pattern
                    }
                    if (lastguitarPatternID[0] != guitarPatternID[0] && lastguitarPatternID[0] > 0)
                    {
                        //load guitar pattern 0
                    }
                    if (lastguitarPatternID[1] != guitarPatternID[1] && lastguitarPatternID[1] > 0)
                    {
                        //load guitar pattern 1
                    }
                    if (lastguitarPatternID[2] != guitarPatternID[2] && lastguitarPatternID[2] > 0)
                    {
                        //load guitar pattern 2
                    }
                }
            }
        }
        
        private void ChangePicDisplayed()
        {
            Dispatcher.Invoke(() =>
            {
                BitmapImage pic;
                if (isKeyboard)
                {
                    if (!isPresetReading)
                    {
                        pic = new BitmapImage(new Uri("pack://application:,,,/Images/pianoReady.png"));
                    }
                    else if (!isLoadingFile)
                    {
                        pic = new BitmapImage(new Uri("pack://application:,,,/Images/pianoLoad.png"));
                    }
                    else
                    {
                        pic = new BitmapImage(new Uri("pack://application:,,,/Images/pianoRead.png"));
                    }
                }
                else
                {
                    if (!isPresetReading)
                    {
                        pic = new BitmapImage(new Uri("pack://application:,,,/Images/guitarReady.png"));
                    }
                    else if (!isLoadingFile)
                    {
                        pic = new BitmapImage(new Uri("pack://application:,,,/Images/guitarLoad.png"));
                    }
                    else
                    {
                        pic = new BitmapImage(new Uri("pack://application:,,,/Images/guitarRead.png"));
                    }
                }
                DisplayPic.Source = pic;
            });
        }
        private void RemoveDisconnectHandler()
        {
            SerialManager.Device.DeviceDisconnected -= OnMainDeviceDisconnectedReceived;
        }
        private void AddDisconnectHandler()
        {
            if (SerialManager.isDeviceConnected())
            {
                SerialManager.Device.DeviceDisconnected += OnMainDeviceDisconnectedReceived;
            }
        }
        private void SendCmd(string cmd, string param = "")
        {
            if (!SerialManager.isDeviceConnected())
            {
                if (SerialManager.Device != null)
                {
                    SerialManager.Device.RaiseDeviceDisconnected();
                }
                return;
            }
            try
            {
                SerialManager.Device.Send(cmd, param); // Or your custom heartbeat command
            }
            catch (Exception)
            {
                
                if (SerialManager.isInitialized() && SerialManager.Device.HasDisconnectHandler(OnMainDeviceDisconnectedReceived))
                {
                    SerialManager.Device.NotifyDisconnect();
                }
        
        
            }
        }
        private void UpdateReadPresetValues()
        {
            isSerialEnabled = false;
            isPresetReading = true;
            ChangePicDisplayed();
            _serialLogWindow.sendButton.IsEnabled = false;
            _serialLogWindow.cmdTextbox.IsEnabled = false;
            RootGrid.IsEnabled = false;
            //todo change pic to make it seem I'm reading
            //to do
            //pause ping reading
            //enqueue all commands related

            SendCmd(SerialDevice.GET_DEVICE_MODE, curPreset.ToString());
            SendCmd(SerialDevice.GET_BASS_ENABLE, curPreset.ToString());
            SendCmd(SerialDevice.GET_DRUMS_ENABLE, curPreset.ToString());
            SendCmd(SerialDevice.GET_STRUM_STYLE, curPreset.ToString());
            SendCmd(SerialDevice.GET_CAPO, curPreset.ToString());
            SendCmd(SerialDevice.GET_KB_TRANSPOSE, curPreset.ToString());
            SendCmd(SerialDevice.GET_MUTE_SEPARATION, curPreset.ToString());
            SendCmd(SerialDevice.GET_STRUM_SEPARATION, curPreset.ToString());

            SendCmd(SerialDevice.GET_ACCOMPANIMENT_ENABLE, curPreset.ToString());
            SendCmd(SerialDevice.GET_SIMPLE_CHORD_MODE, curPreset.ToString());
            
            
            SendCmd(SerialDevice.GET_CHORD_HOLD, curPreset.ToString());
            SendCmd(SerialDevice.GET_ALTERNATING_STRUM, curPreset.ToString());
            SendCmd(SerialDevice.GET_SUSTAIN_MODE, curPreset.ToString());
            SendCmd(SerialDevice.GET_CHORD_MODE, curPreset.ToString());
            SendCmd(SerialDevice.GET_IGNORE_MODE, curPreset.ToString());
            SendCmd(SerialDevice.GET_PROPER_OMNICHORD, curPreset.ToString());

            SendCmd(SerialDevice.GET_BACKING_STATE, "");
            SendCmd(SerialDevice.GET_DRUM_ID, "");
            SendCmd(SerialDevice.GET_BASS_ID, "");
            SendCmd(SerialDevice.GET_BACKING_ID, "");
            SendCmd(SerialDevice.GET_GUITAR_ID, "0");
            SendCmd(SerialDevice.GET_GUITAR_ID, "1");
            SendCmd(SerialDevice.GET_GUITAR_ID, "2");

            for (int i = 0; i < NECK_ROWS; i++)
            {
                for (int j = 0; j < NECK_COLS; j++)
                {
                    SendCmd(SerialDevice.GET_NECK_ASSIGNMENT, curPreset.ToString() + "," + i.ToString() + "," + j.ToString());
                    SendCmd(SerialDevice.GET_NECK_PATTERN, curPreset.ToString() + "," + i.ToString() + "," + j.ToString());
                }
            }
            // this is the last ok
            SendCmd(SerialDevice.GET_BPM,curPreset.ToString());
        }
        private void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            if (Config.NEED_DEBUG_WINDOWS)
            {
                OpenDebugLogWindow();
                OpenSerialLogWindow();
            }
        }
        private void MainWindow_PreviewKeyDown(object sender, KeyEventArgs e)
        {
            if (Keyboard.IsKeyDown(Key.LeftCtrl) || Keyboard.IsKeyDown(Key.RightCtrl))
            {
                if (e.Key == Key.D)
                {
                    OpenDebugLogWindow();
                    e.Handled = true;
                }
                else if (e.Key == Key.G)
                {
                    OpenSerialLogWindow();
                    e.Handled = true;
                }
            }
        }
        private void OpenSerialLogWindow()
        {
            if (_serialLogWindow == null || !_serialLogWindow.IsLoaded)
            {
                _serialLogWindow = new SerialLogWindow();
                _serialLogWindow.Owner = this;
                _serialLogWindow.Show(); // Non-blocking
            }
            else
            {
                _serialLogWindow.Focus(); // Bring to front if already open
            }
        }
        private void OpenDebugLogWindow()
        {
            if (_debugLogWindow == null || !_debugLogWindow.IsLoaded)
            {
                _debugLogWindow = new DebugLogWindow();
                _debugLogWindow.Owner = this;
                _debugLogWindow.Show(); // Non-blocking
            }
            else
            {
                _debugLogWindow.Focus(); // Bring to front if already open
            }
        }

        private void StartSerialPingTimer()
        {
            _serialPingTimer = new DispatcherTimer();
            _serialPingTimer.Interval = TimeSpan.FromSeconds(Config.MAINWINDOW_PING_TIME);
            _serialPingTimer.Tick += OnSerialPingTimerTick;
            _serialPingTimer.Start();
        }

        private void OnSerialPingTimerTick(object sender, EventArgs e)
        {
            if (!isSerialEnabled)
            {
                return;
            }
            var device = SerialManager.Device;

            SendCmd(SerialDevice.GET_PRESET);
            SendCmd(SerialDevice.GET_ISKB);
        }
        private void OnMainDeviceDisconnectedReceived(object sender, EventArgs e)
        {
            RemoveDisconnectHandler();
            Dispatcher.Invoke(() =>
            {
                //MessageBox.Show("Serial device disconnected.");
                MessageBox.Show(this, (string)Application.Current.Resources["GotDisconnected"]);
                isSerialEnabled = false;
                PauseSerial();
                openConnectionWindow(true);


            });
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
        private bool checkCommandParameterRange(int min, int max, int val, int pos)
        {
            string command = SerialManager.Device.LastCommandSent;
            if (val > max)
            {
                addToDebugLog(debugType.replyDebug, (string)Application.Current.Resources["Error"] + " " + command + " " + (string)Application.Current.Resources["ErrorDataReceivedAtPosition"] + " " + pos.ToString() + " " + val.ToString()  + " > " + max.ToString());
                return false;
            }
            if (val < min)
            {
                addToDebugLog(debugType.replyDebug, (string)Application.Current.Resources["Error"] + " " + command + (string)Application.Current.Resources["ErrorDataReceivedAtPosition"] + " " + pos.ToString() + " " + val.ToString() + " < " + min.ToString());
                return false;
            }

            return true;
        }
        private bool checkReceivedValid(List<string> parsedData)
        {
            string command = SerialManager.Device.LastCommandSent;
            int nTemp = -1;
            bool result = checkResult(parsedData);
            if (!result)
            {
                return false;
            }
            if (command == SerialDevice.GET_PRESET)
            {
                if (checkCommandParamCount(parsedData, 3))
                {
                    //check ranges here
                    nTemp = int.Parse(parsedData[2]);

                    if (!checkCommandParameterRange(0, MaxPresetCount, nTemp, 0))
                    {
                        return false;
                    }
                }
            }
            else if (command == SerialDevice.SET_PRESET)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }
            else if (command == SerialDevice.GET_NECK_PATTERN)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }
            else if (command == SerialDevice.STOP_ALL_NOTES)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }
            else if (command == SerialDevice.GET_ISKB)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    //check ranges here
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(0, 1, nTemp, 0))
                    {
                        return false;
                    }
                }
            }
            
            else if (command == SerialDevice.SET_SIMPLE_CHORD_MODE)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }

            else if (command == SerialDevice.SET_KB_TRANSPOSE)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }
            else if (command == SerialDevice.SET_STRUM_STYLE)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }
            else if (command == SerialDevice.SET_STRUM_STYLE)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }
            else if (command == SerialDevice.SET_PROPER_OMNICHORD)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }


            else if (command == SerialDevice.SET_ALTERNATING_STRUM)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }
            else if (command == SerialDevice.SET_BASS_ENABLE)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }
            else if (command == SerialDevice.SET_DRUMS_ENABLE)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }
            else if (command == SerialDevice.SET_ACCOMPANIMENT_ENABLE)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }
            else if (command == SerialDevice.SET_MUTE_WHEN_LET_GO)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }
            else if (command == SerialDevice.SET_CHORD_HOLD)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }
            else if (command == SerialDevice.SET_SUSTAIN_MODE   )
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }
            else if (command == SerialDevice.SET_IGNORE_MODE)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }


            else if (command == SerialDevice.SET_BPM)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }

            else if (command == SerialDevice.SET_BACKING_STATE)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }

            else if (command == SerialDevice.SET_DRUM_ID)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }

            else if (command == SerialDevice.SET_BASS_ID)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }

            else if (command == SerialDevice.SET_BACKING_ID)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }
            else if (command == SerialDevice.SET_GUITAR_ID)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }

            else if (command == SerialDevice.SET_STRUM_SEPARATION)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }

            else if (command == SerialDevice.SET_MUTE_SEPARATION)
            {
                if (!checkCommandParamCount(parsedData, 2))
                {
                    return false;
                }
            }
            else if (command == SerialDevice.GET_CAPO)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(MinCapoTranspose, MaxCapoTranspose, nTemp, 0))
                    {
                        return false;
                    }
                }
            }
            else if (command == SerialDevice.GET_MUTE_SEPARATION)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(MinMuteSeparation, MaxMuteSeparation, nTemp, 0))
                    {
                        return false;
                    }
                }
            }
            else if (command == SerialDevice.GET_STRUM_SEPARATION)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(MinStrumSeparation, MaxStrumSeparation, nTemp, 0))
                    {
                        return false;
                    }
                }
            }

            else if (command == SerialDevice.GET_KB_TRANSPOSE)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(MinKBTranspose, MaxKBTranspose, nTemp, 0))
                    {
                        return false;
                    }
                }
            }
            else if (command == SerialDevice.GET_BASS_ENABLE)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(0, 1, nTemp, 0))
                    {
                        return false;
                    }
                }
            }



            else if (command == SerialDevice.GET_CHORD_HOLD)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(0, 1, nTemp, 0))
                    {
                        return false;
                    }
                }
            }
            else if (command == SerialDevice.GET_ALTERNATING_STRUM)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(0, 1, nTemp, 0))
                    {
                        return false;
                    }
                }
            }
            else if (command == SerialDevice.GET_SUSTAIN_MODE)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(0, 1, nTemp, 0))
                    {
                        return false;
                    }
                }
            }
            else if (command == SerialDevice.GET_CHORD_MODE)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(0, 1, nTemp, 0))
                    {
                        return false;
                    }
                }
            }
            else if (command == SerialDevice.GET_IGNORE_MODE)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(0, 1, nTemp, 0))
                    {
                        return false;
                    }
                }
            }
            
            else if (command == SerialDevice.GET_PROPER_OMNICHORD)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(0, 1, nTemp, 0))
                    {
                        return false;
                    }
                }
            }

            
            else if (command == SerialDevice.GET_NECK_PATTERN)
            {
                if (!checkCommandParamCount(parsedData, 3)) //OK, 00, pattern,
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(0, MaxStrumPatternVal, nTemp, 0))
                    {
                        return false;
                    }
                }
            }
            else if (command == SerialDevice.GET_NECK_ASSIGNMENT)
            {
                if (!checkCommandParamCount(parsedData, 4)) //OK, 00, root, chord
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(0, MaxNoteVal, nTemp, 0))
                    {
                        return false;
                    }
                    if (!checkCommandParameterRange(0, MaxChordVal, nTemp, 1))
                    {
                        return false;
                    }
                }
            }
            




            else if (command == SerialDevice.GET_SIMPLE_CHORD_MODE)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(0, 1, nTemp, 0))
                    {
                        return false;
                    }
                }
            }
            else if (command == SerialDevice.GET_ACCOMPANIMENT_ENABLE)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(0, 1, nTemp, 0))
                    {
                        return false;
                    }
                }
            }
            else if (command == SerialDevice.GET_DRUMS_ENABLE)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(0, 1, nTemp, 0))
                    {
                        return false;
                    }
                }
            }
            else if (command == SerialDevice.GET_STRUM_STYLE)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(0, MaxStrumStyle, nTemp, 0))
                    {
                        return false;
                    }
                }
            }
            else if (command == SerialDevice.GET_DEVICE_MODE)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(0, MAX_MODE_VALUE, nTemp, 0))
                    {
                        return false;
                    }
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
                    if (!checkCommandParameterRange(0, MaxBackingState, nTemp, 0))
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
                    if (!checkCommandParameterRange(0, MaxPatternID, nTemp, 0))
                    {
                        return false;
                    }
                }
            }
            else if (command == SerialDevice.GET_BACKING_ID)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(0, MaxPatternID, nTemp, 0))
                    {
                        return false;
                    }
                }
            }
            else if (command == SerialDevice.GET_BASS_ID)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(0, MaxPatternID, nTemp, 0))
                    {
                        return false;
                    }
                }
            }
            else if (command == SerialDevice.GET_GUITAR_ID)
            {
                if (!checkCommandParamCount(parsedData, 3))
                {
                    return false;
                }
                else
                {
                    nTemp = int.Parse(parsedData[2]);
                    if (!checkCommandParameterRange(0, MaxPatternID, nTemp, 0))
                    {
                        return false;
                    }
                }
            }

            return true;
        }
        private void OnMainSerialDataReceived(object sender, string data)
        {

            Dispatcher.Invoke(() => {
                string response = data.Trim();
                List<string> parsedData = SerialReceiveParser.Parse(response);
                
                bool isDebug = false;
                if (parsedData.Count != 0)
                {
                    if (parsedData[0] == "DB")
                    {
                        isDebug = true;
                    }
                    int nTemp = -1;
                    bool bTemp = false;
                    //curPreset
                    if (isDebug)
                    {
                        string dbgMsg = "";
                        for (int i = 2; i < parsedData.Count; i++)
                        {
                            dbgMsg += parsedData[i];
                        }
                        DebugLog.addToLog(debugType.deviceDebug,dbgMsg);
                    }
                    else
                    {
                        string command = SerialManager.Device.LastCommandSent;
                        string param = SerialManager.Device.LastParamSent;
                        if (command == SerialDevice.GET_PRESET)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                if (nTemp != curPreset && !isLoadingFile)
                                {
                                    curPreset = nTemp;
                                    Dispatcher.BeginInvoke(new Action(() =>
                                    {
                                        presetComboBox.SelectedIndex = curPreset;
                                        UpdateReadPresetValues();
                                    }));

                                }
                            }
                        }
                        else if (command == SerialDevice.SET_PRESET)
                        {
                            //we don't care about first 2
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                            else
                            {
                                //curPreset = nTemp;
                                Dispatcher.BeginInvoke(new Action(() =>
                                {
                                    UpdateReadPresetValues();
                                }));
                            }
                        }

                        else if (command == SerialDevice.STOP_ALL_NOTES)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }

                        }

                        else if (command == SerialDevice.GET_NECK_PATTERN)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                        }
                        else if (command == SerialDevice.GET_ISKB)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                //we don't care about first 2
                                bTemp = int.Parse(parsedData[2]) > 0;
                                bool oldKb = isKeyboard;
                                if (bTemp != isKeyboard)
                                {
                                    //todo force image change here
                                    isKeyboard = bTemp;
                                    if (oldKb != isKeyboard)
                                    {
                                        Dispatcher.BeginInvoke(new Action(() =>
                                        {
                                            ChangePicDisplayed();
                                        }));
                                    }
                                }
                            }
                        }
                        else if (command == SerialDevice.SET_SIMPLE_CHORD_MODE)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                        }
                        else if (command == SerialDevice.SET_KB_TRANSPOSE)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                        }
                        else if (command == SerialDevice.SET_PROPER_OMNICHORD)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                        }

                        else if (command == SerialDevice.SET_ALTERNATING_STRUM)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                        }
                        else if (command == SerialDevice.SET_BASS_ENABLE)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                        }
                        else if (command == SerialDevice.SET_DRUMS_ENABLE)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                        }
                        else if (command == SerialDevice.SET_ACCOMPANIMENT_ENABLE)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                        }

                        else if (command == SerialDevice.SET_MUTE_WHEN_LET_GO)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                        }
                        else if (command == SerialDevice.SET_CHORD_HOLD)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                        }
                        else if (command == SerialDevice.SET_SUSTAIN_MODE)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                        }
                        else if (command == SerialDevice.SET_IGNORE_MODE)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                        }
                        else if (command == SerialDevice.SET_BACKING_STATE)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                        }
                        else if (command == SerialDevice.SET_BPM)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                        }
                        else if (command == SerialDevice.SET_DRUM_ID)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                        }
                        else if (command == SerialDevice.SET_BACKING_ID)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                        }
                        else if (command == SerialDevice.SET_BASS_ID)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                        }
                        else if (command == SerialDevice.SET_GUITAR_ID)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                        }
                        else if (command == SerialDevice.SET_MUTE_SEPARATION)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                        }

                        else if (command == SerialDevice.SET_STRUM_SEPARATION)
                        {
                            if (!checkReceivedValid(parsedData))
                            {

                            }
                        }
                        else if (command == SerialDevice.GET_STRUM_SEPARATION)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                strumSeparationTextbox.Text = nTemp.ToString();
                                UpdateSliderFromTextBox(strumSeparationSlider, strumSeparationTextbox);
                                sendSetStrumSeparation();
                            }
                        }
                        else if (command == SerialDevice.GET_MUTE_SEPARATION)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                muteSeparationTextbox.Text = nTemp.ToString();
                                UpdateSliderFromTextBox(muteSeparationSlider, muteSeparationTextbox);
                                sendSetMuteSeparation();
                            }
                        }
                        else if (command == SerialDevice.GET_KB_TRANSPOSE)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                kbTransposeTextbox.Text = nTemp.ToString();
                                UpdateSliderFromTextBox(kbTransposeSlider, kbTransposeTextbox);
                            }
                        }
                        else if (command == SerialDevice.GET_CAPO)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                capoTextbox.Text = nTemp.ToString();
                                UpdateSliderFromTextBox(capoSlider, capoTextbox);
                            }
                        }
                        else if (command == SerialDevice.GET_BASS_ENABLE)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                bassComboBox.SelectedIndex = nTemp;
                            }
                        }

                        else if (command == SerialDevice.GET_CHORD_HOLD)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                chordHoldComboBox.SelectedIndex = nTemp;
                            }
                        }
                        else if (command == SerialDevice.GET_ALTERNATING_STRUM)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                alternatingStrumComboBox.SelectedIndex = nTemp;
                            }
                        }
                        else if (command == SerialDevice.GET_SUSTAIN_MODE)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                sustainModeComboBox.SelectedIndex = nTemp;
                            }
                        }
                        else if (command == SerialDevice.GET_CHORD_MODE)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                chordButtonModeComboBox.SelectedIndex = nTemp;
                            }
                        }
                        else if (command == SerialDevice.GET_IGNORE_MODE)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                ignoreSameButtonComboBox.SelectedIndex = nTemp;
                            }
                        }
                        else if (command == SerialDevice.GET_PROPER_OMNICHORD)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                lower5thComboBox.SelectedIndex = nTemp;
                            }
                        }

                        else if (command == SerialDevice.GET_NECK_PATTERN)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                int curRow = -1;
                                int curCol = -1;
                                string[] parts = SerialManager.Device.LastParamSent.Split(',');

                                if (parts.Length >= 3 &&
                                    int.TryParse(parts[1], out curRow) &&
                                    int.TryParse(parts[2], out curCol))
                                {
                                    allComboPatterns[curRow * NECK_COLS + curCol].SelectedIndex = int.Parse(parsedData[2]);
                                }
                                else
                                {
                                    DebugLog.addToLog(debugType.replyDebug, "Error! Parsing " + command + " encountered unexpected parsing issue.");
                                }

                            }
                        }
                        else if (command == SerialDevice.GET_NECK_ASSIGNMENT)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                int curRow = -1;
                                int curCol = -1;
                                string[] parts = SerialManager.Device.LastParamSent.Split(',');

                                if (parts.Length >= 3 &&
                                    int.TryParse(parts[1], out curRow) &&
                                    int.TryParse(parts[2], out curCol))
                                {
                                    allComboRoots[curRow * NECK_COLS + curCol].SelectedIndex = int.Parse(parsedData[2]);
                                    allComboChords[curRow * NECK_COLS + curCol].SelectedIndex = int.Parse(parsedData[3]);
                                }
                                else
                                {
                                    DebugLog.addToLog(debugType.replyDebug, (string)Application.Current.Resources["ErrorParsingCommand"] + " " + command + " " + (string)Application.Current.Resources["ErrorParsingCommand2"]);
                                }

                            }
                        }

                        else if (command == SerialDevice.GET_SIMPLE_CHORD_MODE)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                if (nTemp == 0)
                                {
                                    standardRadioButton.IsChecked = true;
                                }
                                else
                                {
                                    simpleRadioButton.IsChecked = true;
                                }
                            }
                        }
                        else if (command == SerialDevice.GET_ACCOMPANIMENT_ENABLE)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                backingComboBox.SelectedIndex = nTemp;
                            }
                        }
                        else if (command == SerialDevice.GET_DRUMS_ENABLE)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                drumComboBox.SelectedIndex = nTemp;
                            }
                        }
                        else if (command == SerialDevice.GET_STRUM_STYLE)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                strumStyleComboBox.SelectedIndex = nTemp;
                            }
                        }
                        else if (command == SerialDevice.GET_DEVICE_MODE)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                modeComboBox.SelectedIndex = nTemp;
                            }

                        }
                        else if (command == SerialDevice.GET_BACKING_STATE)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                backingState = nTemp;
                            }

                        }
                        else if (command == SerialDevice.GET_DRUM_ID)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                drumPatternID = nTemp;
                            }

                        }
                        else if (command == SerialDevice.GET_BASS_ID)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                bassPatternID = nTemp;
                            }
                        }
                        else if (command == SerialDevice.GET_BACKING_ID)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                backingPatternID = nTemp;
                            }
                        }
                        else if (command == SerialDevice.GET_GUITAR_ID)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                int nTemp2 = int.Parse(param);
                                nTemp = int.Parse(parsedData[2]);
                                if (nTemp2 >= guitarPatternID.Length || nTemp2 < 0)
                                {
                                    DebugLog.addToLog(debugType.replyDebug, (string)Application.Current.Resources["InvalidGuitarPatternIndex"] + " " + nTemp2.ToString());
                                }
                                else
                                {
                                    guitarPatternID[nTemp2] = nTemp;
                                }
                            }

                        }
                        else if (command == SerialDevice.GET_BPM)
                        {
                            if (parsedData.Count >= 3)
                            {
                                if (parsedData[0] != "OK")
                                {
                                }
                                else
                                {
                                    changeBPM(int.Parse(parsedData[2]));
                                    isPresetReading = false;
                                    ChangePicDisplayed();
                                    RootGrid.IsEnabled = true;
                                    _serialLogWindow.sendButton.IsEnabled = true;
                                    _serialLogWindow.cmdTextbox.IsEnabled = true;
                                    //todo revert picture back
                                }
                            }
                            isSerialEnabled = true;//restart ping

                        }
                    }
                }
                if (!isDebug)
                {
                    SerialManager.Device.clearLastCommand();
                }

            });
        }

        //events
        private void PauseSerial()
        {
            if (SerialManager.Device != null)
            {
                SerialManager.Device.DataReceived -= OnMainSerialDataReceived;
            }
        }
        private void ResumeSerial()
        {
            if (SerialManager.Device != null)
            {
                SerialManager.Device.DataReceived += OnMainSerialDataReceived;
            }
        }

        private async Task WaitForSerialProcessingAsync()
        {
            int timeoutMs = 500; // Max time to wait
            int waited = 0;
            int interval = 100; // Check every 100ms

            while (isExpectingSerialData && waited < timeoutMs)
            {
                await Task.Delay(interval);
                waited += interval;
            }

            // Optional: throw if timed out
            if (isExpectingSerialData)
            {
                throw new TimeoutException((string)Application.Current.Resources["SerialTimeout"]);
            }
        }
        private void SetLanguage(string cultureCode)
        {
            var dictionary = new ResourceDictionary();
            switch (cultureCode)
            {
                case "en":
                default:
                    dictionary.Source = new Uri("Resources/Strings.en.xaml", UriKind.Relative);
                    break;
            }

            // Replace existing dictionaries
            var existing = Application.Current.Resources.MergedDictionaries
                             .FirstOrDefault(d => d.Source?.OriginalString.Contains("Strings.") == true);
            if (existing != null)
                Application.Current.Resources.MergedDictionaries.Remove(existing);

            Application.Current.Resources.MergedDictionaries.Add(dictionary);
        }
        private void SetupComponents()
        {
            SetupComboBoxes();
            SetupSliders();
            SetupRadioButtons();
            _resetTimer = new DispatcherTimer
            {
                Interval = TimeSpan.FromSeconds(2)
            };
            _resetTimer.Tick += (s, e) =>
            {
                _tapTimes.Clear();
                _resetTimer.Stop();
            };
        }

        private void BpmButton_Click(object sender, RoutedEventArgs e)
        {
            var now = DateTime.Now;
            _tapTimes.Add(now);

            // Remove old taps if list grows large (optional)
            if (_tapTimes.Count > 10)
                _tapTimes.RemoveAt(0);

            if (_tapTimes.Count >= MinTaps)
            {
                double bpm = CalculateBPM(_tapTimes);
                bpm = Math.Max(MinBPM, Math.Min(MaxBPM, bpm));
                bpmTextbox.Focus();
                bpmTextbox.Text = bpm.ToString("F0"); // or update slider, label etc.
                Keyboard.ClearFocus();
                UpdateSliderFromTextBox(bpmSlider, bpmTextbox);
                changeBPM(int.Parse(bpmTextbox.Text));
            }


            // Restart reset timer
            _resetTimer.Stop();
            _resetTimer.Start();
        }

        private double CalculateBPM(List<DateTime> taps)
        {
            if (taps.Count < 2) return 0;

            // Calculate intervals between successive taps
            var intervals = new List<double>();
            for (int i = 1; i < taps.Count; i++)
            {
                intervals.Add((taps[i] - taps[i - 1]).TotalMilliseconds);
            }

            double avgIntervalMs = 0;
            foreach (var interval in intervals)
                avgIntervalMs += interval;
            avgIntervalMs /= intervals.Count;

            if (avgIntervalMs == 0) return 0;

            // BPM = 60000 ms per minute / average interval in ms
            return 60000 / avgIntervalMs;
        }
        private void SetupRadioButtons()
        {
            standardRadioButton.IsChecked = true;
        }
        private void simpleRadioButton_Checked(object sender, RoutedEventArgs e)
        {
            var radioButton = sender as RadioButton;
            if (radioButton != null)
            {
                //disable 2nd and 3rd columns of frets
                A02ComboChord.IsEnabled = false;
                A02ComboPattern.IsEnabled = false;
                A02ComboRoot.IsEnabled = false;
                A03ComboChord.IsEnabled = false;
                A03ComboPattern.IsEnabled = false;
                A03ComboRoot.IsEnabled = false;
                B02ComboChord.IsEnabled = false;
                B02ComboPattern.IsEnabled = false;
                B02ComboRoot.IsEnabled = false;
                B03ComboChord.IsEnabled = false;
                B03ComboPattern.IsEnabled = false;
                B03ComboRoot.IsEnabled = false;
                C02ComboChord.IsEnabled = false;
                C02ComboPattern.IsEnabled = false;
                C02ComboRoot.IsEnabled = false;
                C03ComboChord.IsEnabled = false;
                C03ComboPattern.IsEnabled = false;
                C03ComboRoot.IsEnabled = false;
                D02ComboChord.IsEnabled = false;
                D02ComboPattern.IsEnabled = false;
                D02ComboRoot.IsEnabled = false;
                D03ComboChord.IsEnabled = false;
                D03ComboPattern.IsEnabled = false;
                D03ComboRoot.IsEnabled = false;
                E02ComboChord.IsEnabled = false;
                E02ComboPattern.IsEnabled = false;
                E02ComboRoot.IsEnabled = false;
                E03ComboChord.IsEnabled = false;
                E03ComboPattern.IsEnabled = false;
                E03ComboRoot.IsEnabled = false;
                F02ComboChord.IsEnabled = false;
                F02ComboPattern.IsEnabled = false;
                F02ComboRoot.IsEnabled = false;
                F03ComboChord.IsEnabled = false;
                F03ComboPattern.IsEnabled = false;
                F03ComboRoot.IsEnabled = false;
                G02ComboChord.IsEnabled = false;
                G02ComboPattern.IsEnabled = false;
                G02ComboRoot.IsEnabled = false;
                G03ComboChord.IsEnabled = false;
                G03ComboPattern.IsEnabled = false;
                G03ComboRoot.IsEnabled = false;
            }
            SimpleRadioButton_Changed(true);
        }

        private void standardRadioButton_Checked(object sender, RoutedEventArgs e)
        {
            var radioButton = sender as RadioButton;
            if (radioButton != null)
            {
                //enable 2nd and 3rd columns of frets
                A02ComboChord.IsEnabled = true;
                A02ComboPattern.IsEnabled = true;
                A02ComboRoot.IsEnabled = true;
                A03ComboChord.IsEnabled = true;
                A03ComboPattern.IsEnabled = true;
                A03ComboRoot.IsEnabled = true;
                B02ComboChord.IsEnabled = true;
                B02ComboPattern.IsEnabled = true;
                B02ComboRoot.IsEnabled = true;
                B03ComboChord.IsEnabled = true;
                B03ComboPattern.IsEnabled = true;
                B03ComboRoot.IsEnabled = true;
                C02ComboChord.IsEnabled = true;
                C02ComboPattern.IsEnabled = true;
                C02ComboRoot.IsEnabled = true;
                C03ComboChord.IsEnabled = true;
                C03ComboPattern.IsEnabled = true;
                C03ComboRoot.IsEnabled = true;
                D02ComboChord.IsEnabled = true;
                D02ComboPattern.IsEnabled = true;
                D02ComboRoot.IsEnabled = true;
                D03ComboChord.IsEnabled = true;
                D03ComboPattern.IsEnabled = true;
                D03ComboRoot.IsEnabled = true;
                E02ComboChord.IsEnabled = true;
                E02ComboPattern.IsEnabled = true;
                E02ComboRoot.IsEnabled = true;
                E03ComboChord.IsEnabled = true;
                E03ComboPattern.IsEnabled = true;
                E03ComboRoot.IsEnabled = true;
                F02ComboChord.IsEnabled = true;
                F02ComboPattern.IsEnabled = true;
                F02ComboRoot.IsEnabled = true;
                F03ComboChord.IsEnabled = true;
                F03ComboPattern.IsEnabled = true;
                F03ComboRoot.IsEnabled = true;
                G02ComboChord.IsEnabled = true;
                G02ComboPattern.IsEnabled = true;
                G02ComboRoot.IsEnabled = true;
                G03ComboChord.IsEnabled = true;
                G03ComboPattern.IsEnabled = true;
                G03ComboRoot.IsEnabled = true;
            }
            SimpleRadioButton_Changed(false);
        }
        private void SimpleRadioButton_Changed(bool isSimple)
        {
            if (!isPresetReading && initDone)
            {
                SendCmd(SerialDevice.SET_SIMPLE_CHORD_MODE, isSimple ? "1" : "0");
            }
        }
        
        private void kbTransposeTextbox_LostFocus(object sender, RoutedEventArgs e)
        {
            UpdateSliderFromTextBox(kbTransposeSlider, kbTransposeTextbox);
            if (!isPresetReading)
            {
                SendCmd(SerialDevice.SET_KB_TRANSPOSE, curPreset.ToString() + ',' + kbTransposeTextbox.Text);
            }
        }

        private void kbTransposeTextbox_PreviewKeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
            {
                UpdateSliderFromTextBox(kbTransposeSlider, kbTransposeTextbox);
                e.Handled = true; // Prevent beep or unexpected behavior
                                  // Optionally move focus away to simulate leaving the textbox
                Keyboard.ClearFocus();
            }
        }
        private void sendSetMuteSeparation()
        {
            if (!isPresetReading)
            {
                SendCmd(SerialDevice.SET_MUTE_SEPARATION, curPreset.ToString() + ',' + muteSeparationTextbox.Text);
            }
        }
        private void sendSetStrumSeparation()
        {
            if (!isPresetReading)
            {
                SendCmd(SerialDevice.SET_STRUM_SEPARATION, curPreset.ToString() + ',' + strumSeparationTextbox.Text);
            }
        }
        private void strumSeparationTextbox_LostFocus(object sender, RoutedEventArgs e)
        {
            UpdateSliderFromTextBox(strumSeparationSlider, strumSeparationTextbox);
            sendSetStrumSeparation();
        }

        private void strumSeparationTextbox_PreviewKeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
            {
                UpdateSliderFromTextBox(strumSeparationSlider, strumSeparationTextbox);
                e.Handled = true; // Prevent beep or unexpected behavior
                                  // Optionally move focus away to simulate leaving the textbox
                Keyboard.ClearFocus();
            }
        }
        private void muteSeparationTextbox_LostFocus(object sender, RoutedEventArgs e)
        {
            Keyboard.ClearFocus();
            sendSetMuteSeparation();
        }

        private void muteSeparationTextbox_PreviewKeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
            {
                UpdateSliderFromTextBox(muteSeparationSlider, muteSeparationTextbox);
                e.Handled = true; // Prevent beep or unexpected behavior
                                  // Optionally move focus away to simulate leaving the textbox
                Keyboard.ClearFocus();
            }
        }

        private void capoTextbox_LostFocus(object sender, RoutedEventArgs e)
        {
            UpdateSliderFromTextBox(capoSlider, capoTextbox);
        }

        private void capoTextbox_PreviewKeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
            {
                UpdateSliderFromTextBox(capoSlider, capoTextbox);
                e.Handled = true; // Prevent beep or unexpected behavior
                                  // Optionally move focus away to simulate leaving the textbox
                Keyboard.ClearFocus();
            }
        }
        private void bpmTextbox_PreviewKeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
            {
                UpdateSliderFromTextBox(bpmSlider, bpmTextbox);
                e.Handled = true; // Prevent beep or unexpected behavior
                                  // Optionally move focus away to simulate leaving the textbox
                Keyboard.ClearFocus();
            }
        }

        // Called when the textbox loses focus
        private void bpmTextbox_LostFocus(object sender, RoutedEventArgs e)
        {
            UpdateSliderFromTextBox(bpmSlider, bpmTextbox);
            changeBPM(int.Parse(bpmTextbox.Text));
        }
        private void changeBPM(int bpm)
        {
            if (!SerialManager.isDeviceConnected())
            {
                return;
            }
            else if (isPresetReading)
            {
                //called by serial thread
                Dispatcher.Invoke(() =>
                {
                    bpmTextbox.Text = bpm.ToString();
                    UpdateSliderFromTextBox(bpmSlider, bpmTextbox);
                });
                return;
            }
            Application.Current.Dispatcher.Invoke(() =>
            {
                
                if (bpm != curBPM)
                { 
                    SerialManager.Device.Send(SerialDevice.SET_BPM, curPreset.ToString() + "," + bpm.ToString()); // Or your custom heartbeat command
                    curBPM = bpm;
                }
            });
        }
        // Shared logic
        private void UpdateSliderFromTextBox(Slider s, TextBox t)
        {
            if (s == null)
                return;

            if (double.TryParse(t.Text, out double inputValue))
            {
                // Check if input is within the slider's valid range
                if (inputValue >= s.Minimum && inputValue <= s.Maximum)
                {
                    s.Value = inputValue;
                }
                else
                {
                    // Invalid input: revert to current slider value
                    t.Text = s.Value.ToString("F0");
                }
            }
            else
            {
                // Invalid number: revert to current slider value
                t.Text = s.Value.ToString("F0");
            }

        }

        private void muteSeparationSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (muteSeparationTextbox != null)
            {
                muteSeparationTextbox.Text = e.NewValue.ToString("F0");
                sendSetMuteSeparation();
            }
        }
        private void strumSeparationSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (strumSeparationTextbox != null)
            {
                strumSeparationTextbox.Text = e.NewValue.ToString("F0");
                sendSetStrumSeparation();
            }
        }

        private void bpmSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (bpmTextbox != null)
            {
                bpmTextbox.Text = e.NewValue.ToString("F0");
                changeBPM(int.Parse(bpmTextbox.Text));
            }
        }
        private void capoSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (capoTextbox != null)
            {
                capoTextbox.Text = e.NewValue.ToString("F0");
                if (!isPresetReading)
                {
                    SendCmd(SerialDevice.SET_CAPO, curPreset.ToString()+"," + capoTextbox.Text);
                }
            }
        }
        private void kbTransposeSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (kbTransposeTextbox != null)
            {
                kbTransposeTextbox.Text = e.NewValue.ToString("F0");
            }
            if (!isPresetReading)
            {
                SendCmd(SerialDevice.SET_KB_TRANSPOSE, curPreset.ToString() + ',' + kbTransposeTextbox.Text);
            }
        }
        private void SetupSliders()
        {
            //bpmSlider;
            //capoSlider;
            //kbTransposeSlider;
            bpmSlider.Minimum = MinBPM;
            bpmSlider.Maximum = MaxBPM;
            capoSlider.Minimum = MinCapoTranspose;
            capoSlider.Maximum = MaxCapoTranspose;
            kbTransposeSlider.Minimum = MinKBTranspose;
            kbTransposeSlider.Maximum = MaxKBTranspose;
            strumSeparationSlider.Minimum = MinStrumSeparation;
            strumSeparationSlider.Maximum = MaxStrumSeparation;
            muteSeparationSlider.Minimum = MinMuteSeparation;
            muteSeparationSlider.Maximum = MaxMuteSeparation;

        }

        private void SetupComboBoxes()
        {
            //Enable/Disable
            var optionsEnabled = new[] { (string)Application.Current.Resources["Disabled"], (string)Application.Current.Resources["Enabled"] };

            // Populate each ComboBox
            lower5thComboBox.ItemsSource = optionsEnabled;
            bassComboBox.ItemsSource = optionsEnabled;
            drumComboBox.ItemsSource = optionsEnabled;
            backingComboBox.ItemsSource = optionsEnabled;
            chordHoldComboBox.ItemsSource = optionsEnabled;
            alternatingStrumComboBox.ItemsSource = optionsEnabled;
            ignoreSameButtonComboBox.ItemsSource = optionsEnabled;

            backingComboBox.SelectedIndex = 0;
            bassComboBox.SelectedIndex = 0;
            drumComboBox.SelectedIndex = 0;
            chordHoldComboBox.SelectedIndex = 0;
            alternatingStrumComboBox.SelectedIndex = 0;
            ignoreSameButtonComboBox.SelectedIndex = 0;
            lower5thComboBox.SelectedIndex = 0;

            lower5thComboBox.SelectionChanged += lower5thComboBox_SelectionChanged;
            var optionsPresets = Enumerable.Range(1, MaxPresetCount)
                                  .Select(i => i.ToString())
                                  .ToArray();
            presetComboBox.ItemsSource = optionsPresets;
            presetComboBox.SelectedIndex = -1;

            var optionsOmni = new[] { (string)Application.Current.Resources["OmniMode1"], (string)Application.Current.Resources["OmniMode2"], (string)Application.Current.Resources["OmniMode3"], (string)Application.Current.Resources["OmniMode4"] };
            
            modeComboBox.ItemsSource = optionsOmni;
            modeComboBox.SelectedIndex = 0;

            var optionsStrumStyle = new[] { (string)Application.Current.Resources["StrumStyle1"], (string)Application.Current.Resources["StrumStyle2"], (string)Application.Current.Resources["StrumStyle3"] };
            strumStyleComboBox.ItemsSource = optionsStrumStyle;
            strumStyleComboBox.SelectedIndex = 0; 
            strumStyleComboBox.SelectionChanged += strumStyleComboBox_SelectionChanged;
            var optionsToggleStyle = new[] { (string)Application.Current.Resources["Pressed"], (string)Application.Current.Resources["Toggled"] };
            sustainModeComboBox.ItemsSource = optionsToggleStyle;
            sustainModeComboBox.SelectedIndex = 0;


            var optionsChordModeStyle = new[] { (string)Application.Current.Resources["ChordMode2"], (string)Application.Current.Resources["ChordMode1"] };
            chordButtonModeComboBox.ItemsSource = optionsChordModeStyle;
            chordButtonModeComboBox.SelectedIndex = 0;
            var optionsRootNoteStyle = new[] { (string)Application.Current.Resources["RootNote1"], (string)Application.Current.Resources["RootNote2"], (string)Application.Current.Resources["RootNote3"], (string)Application.Current.Resources["RootNote4"], (string)Application.Current.Resources["RootNote5"], (string)Application.Current.Resources["RootNote6"], (string)Application.Current.Resources["RootNote7"], (string)Application.Current.Resources["RootNote8"], (string)Application.Current.Resources["RootNote9"], (string)Application.Current.Resources["RootNote10"], (string)Application.Current.Resources["RootNote11"], (string)Application.Current.Resources["RootNote12"] };

            allComboRoots = new List<ComboBox>
            {
                A01ComboRoot, A02ComboRoot, A03ComboRoot,
                B01ComboRoot, B02ComboRoot, B03ComboRoot,
                C01ComboRoot, C02ComboRoot, C03ComboRoot,
                D01ComboRoot, D02ComboRoot, D03ComboRoot,
                E01ComboRoot, E02ComboRoot, E03ComboRoot,
                F01ComboRoot, F02ComboRoot, F03ComboRoot,
                G01ComboRoot, G02ComboRoot, G03ComboRoot
            };
            for (int i = 0; i < allComboRoots.Count; i++)
            {
                allComboRoots[i].ItemsSource = optionsRootNoteStyle;
                allComboRoots[i].SelectedIndex = 0;
            }

            var optionsChordNoteStyle = new[] 
            { (string)Application.Current.Resources["ChordNote1"], (string)Application.Current.Resources["ChordNote2"], (string)Application.Current.Resources["ChordNote3"], (string)Application.Current.Resources["ChordNote4"], (string)Application.Current.Resources["ChordNote5"], (string)Application.Current.Resources["ChordNote6"], (string)Application.Current.Resources["ChordNote7"], (string)Application.Current.Resources["ChordNote8"], (string)Application.Current.Resources["ChordNote9"],
              (string)Application.Current.Resources["ChordNote10"], (string)Application.Current.Resources["ChordNote11"], (string)Application.Current.Resources["ChordNote12"], (string)Application.Current.Resources["ChordNote13"], (string)Application.Current.Resources["ChordNote14"], (string)Application.Current.Resources["ChordNote15"], (string)Application.Current.Resources["ChordNote16"], (string)Application.Current.Resources["ChordNote17"], (string)Application.Current.Resources["ChordNote18"],(string)Application.Current.Resources["ChordNote19"],
              (string)Application.Current.Resources["ChordNote20"] ,(string)Application.Current.Resources["ChordNote21"], (string)Application.Current.Resources["ChordNote22"], (string)Application.Current.Resources["ChordNote23"], (string)Application.Current.Resources["ChordNote24"], (string)Application.Current.Resources["ChordNote25"], (string)Application.Current.Resources["ChordNote26"], (string)Application.Current.Resources["ChordNote27"], (string)Application.Current.Resources["ChordNote28"],(string)Application.Current.Resources["ChordNote29"],
              (string)Application.Current.Resources["ChordNote30"]
            };
            allComboChords = new List<ComboBox>
            {
                A01ComboChord, A02ComboChord, A03ComboChord,
                B01ComboChord, B02ComboChord, B03ComboChord,
                C01ComboChord, C02ComboChord, C03ComboChord,
                D01ComboChord, D02ComboChord, D03ComboChord,
                E01ComboChord, E02ComboChord, E03ComboChord,
                F01ComboChord, F02ComboChord, F03ComboChord,
                G01ComboChord, G02ComboChord, G03ComboChord
            };
            for (int i = 0; i < allComboRoots.Count; i++)
            {
                allComboChords[i].ItemsSource = optionsChordNoteStyle;
                allComboChords[i].SelectedIndex = 0;
            }

            foreach (var cb in allComboChords)
            {
                cb.SelectionChanged += RootChordComboBox_SelectionChanged;
            }
            foreach (var cb in allComboRoots)
            {
                cb.SelectionChanged += RootChordComboBox_SelectionChanged;
            }
            string baseLabel = (string)Application.Current.Resources["GuitarPattern"];

            var optionsChordPatternStyle = Enumerable.Range(1, MaxGuitarPatternCount)
                .Select(i => $"{baseLabel} {i}")
                .ToArray();

            allComboPatterns = new List<ComboBox>
            {
                A01ComboPattern, A02ComboPattern, A03ComboPattern,
                B01ComboPattern, B02ComboPattern, B03ComboPattern,
                C01ComboPattern, C02ComboPattern, C03ComboPattern,
                D01ComboPattern, D02ComboPattern, D03ComboPattern,
                E01ComboPattern, E02ComboPattern, E03ComboPattern,
                F01ComboPattern, F02ComboPattern, F03ComboPattern,
                G01ComboPattern, G02ComboPattern, G03ComboPattern
            };
            for (int i = 0; i < allComboRoots.Count; i++)
            {
                allComboPatterns[i].ItemsSource = optionsChordPatternStyle;
                allComboPatterns[i].SelectedIndex = 0;
            }
            foreach (var cb in allComboPatterns)
            {
                cb.SelectionChanged += PatternComboBox_SelectionChanged;
            }
            setAllGuitarPatternComboBox.ItemsSource = optionsChordPatternStyle;
            setAllGuitarPatternComboBox.SelectedIndex = -1;
            setAllGuitarPatternComboBox.SelectionChanged += AllPatternComboBox_SelectionChanged;


            //set generic handler
            alternatingStrumComboBox.SelectionChanged += GenericComboBox_SelectionChanged;
            bassComboBox.SelectionChanged += GenericComboBox_SelectionChanged;
            drumComboBox.SelectionChanged += GenericComboBox_SelectionChanged;
            backingComboBox.SelectionChanged += GenericComboBox_SelectionChanged;
            chordHoldComboBox.SelectionChanged += GenericComboBox_SelectionChanged;
            sustainModeComboBox.SelectionChanged += GenericComboBox_SelectionChanged;
            chordButtonModeComboBox.SelectionChanged += GenericComboBox_SelectionChanged;
            ignoreSameButtonComboBox.SelectionChanged += GenericComboBox_SelectionChanged;
        }
        private void GenericComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (isPresetReading || !initDone)
            {
                return;
            }
            if (sender == alternatingStrumComboBox)
            {
                SendCmd(SerialDevice.SET_ALTERNATING_STRUM, curPreset.ToString() + ',' + ((ComboBox)sender).SelectedIndex);
            }
            else if (sender == bassComboBox)
            {
                SendCmd(SerialDevice.SET_BASS_ENABLE, curPreset.ToString() + ',' + ((ComboBox)sender).SelectedIndex);
            }
            else if (sender == drumComboBox)
            {
                SendCmd(SerialDevice.SET_DRUMS_ENABLE, curPreset.ToString() + ',' + ((ComboBox)sender).SelectedIndex);
            }
            else if (sender == backingComboBox)
            {
                SendCmd(SerialDevice.SET_ACCOMPANIMENT_ENABLE, curPreset.ToString() + ',' + ((ComboBox)sender).SelectedIndex);
            }
            else if (sender == chordHoldComboBox)
            {
                SendCmd(SerialDevice.SET_CHORD_HOLD, curPreset.ToString() + ',' + ((ComboBox)sender).SelectedIndex);
                int toSend = -1;
                if (((ComboBox)sender).SelectedIndex == 0 )
                {
                    toSend = 1;
                }
                else if (((ComboBox)sender).SelectedIndex == 1)
                {
                    toSend = 0;
                }
                SendCmd(SerialDevice.SET_MUTE_WHEN_LET_GO, curPreset.ToString() + ',' + toSend.ToString());
            }
            else if (sender == sustainModeComboBox)
            {
                SendCmd(SerialDevice.SET_SUSTAIN_MODE, curPreset.ToString() + ',' + ((ComboBox)sender).SelectedIndex);
            }
            else if (sender == ignoreSameButtonComboBox)
            {
                SendCmd(SerialDevice.SET_IGNORE_MODE, curPreset.ToString() + ',' + ((ComboBox)sender).SelectedIndex);
            }
            else if (sender == chordButtonModeComboBox)
            {
                SendCmd(SerialDevice.SET_CHORD_MODE, curPreset.ToString() + ',' + ((ComboBox)sender).SelectedIndex);
            }
        }

        public void openConnectionWindow(bool isDisconnected = false)
        {
            isSerialEnabled = false;
            PauseSerial();
            var connectionWindow = new ConnectionWindow();
            connectionWindow.ShowDialog(); // This blocks until it's closed
            ResumeSerial();
            isSerialEnabled = true;
            if (SerialManager.isDeviceConnected())
            {
                statusEllipse.Fill = (Brush)Application.Current.Resources["STATUSOK"];
                statusLabel.Content = (string)Application.Current.Resources["Connected"];
            }
            else
            {
                statusEllipse.Fill = (Brush)Application.Current.Resources["STATUSNG"];
                statusLabel.Content = (string)Application.Current.Resources["NotConnected"];
            }
            if (isPresetReading)
            {
                //handle weird case
                DebugLog.addToLog(debugType.miscDebug, (string)Application.Current.Resources["PresetEarlyReading"]);
                isDisconnected = true;
                isPresetReading = false;
            }
            if (isDisconnected)
            {
                SendCmd(SerialDevice.GET_PRESET);
            }
            
            AddDisconnectHandler();
        }
        private async void connectionSettingsButton_Click(object sender, RoutedEventArgs e)
        {

            try
            {
                await WaitForSerialProcessingAsync();

                

                openConnectionWindow();


                
            }
            catch (TimeoutException ex)
            {
                MessageBox.Show((string)Application.Current.Resources["FailedOpenConnectionWindow"] + " " + ex.Message);
            }


        }
        private void addToDebugLog(debugType type, string message)
        {
            Application.Current.Dispatcher.Invoke(() =>
            {
                DebugLog.addToLog(type, message);
            });
        }

        private void addToSerialLog(string message)
        {
            Application.Current.Dispatcher.Invoke(() =>
            {
                SerialLog.addToLog(message);
            });
        }


        private void patternFactoryButton_Click(object sender, RoutedEventArgs e)
        {

        }

        private void patternSettingsButton_Click(object sender, RoutedEventArgs e)
        {

        }

        private void otherSettingsButton_Click(object sender, RoutedEventArgs e)
        {

        }

        private void presetComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (curPreset != presetComboBox.SelectedIndex)
            {
                curPreset = presetComboBox.SelectedIndex;
                SendCmd(SerialDevice.SET_PRESET, curPreset.ToString());
            }
        }

        private void stopButton_Click(object sender, RoutedEventArgs e)
        {
            SendCmd(SerialDevice.STOP_ALL_NOTES);
        }
        private void lower5thComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (!isPresetReading)
            {
                SendCmd(SerialDevice.SET_PROPER_OMNICHORD, curPreset.ToString() + ',' + lower5thComboBox.SelectedIndex.ToString());
                return;
            }
        }
        private void strumStyleComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (!isPresetReading)
            {
                SendCmd(SerialDevice.SET_STRUM_STYLE, curPreset.ToString() + ',' + strumStyleComboBox.SelectedIndex);
                return;
            }
        }
        private void RootChordComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (isPresetReading)
            {
                return;
            }
            if (sender is ComboBox changedComboBox)
            {
                int index1 = allComboRoots.IndexOf(changedComboBox);
                int index2 = allComboChords.IndexOf(changedComboBox);
                int row = -1;
                int col = -1;
                if (index1 != -1)
                {
                    row = index1 / NECK_COLS;
                    col = index1 % NECK_COLS;
                    SendCmd(SerialDevice.SET_NECK_ASSIGNMENT, curPreset.ToString() + ',' + row.ToString() + "," + col.ToString() + "," + allComboRoots[index1].SelectedIndex.ToString() + ',' + allComboChords[index1].SelectedIndex.ToString());
                }
                else if (index2 != -1)
                {
                    row = index2 / NECK_COLS;
                    col = index2 % NECK_COLS;
                    SendCmd(SerialDevice.SET_NECK_ASSIGNMENT, curPreset.ToString() + ',' + row.ToString() + "," + col.ToString() + "," + allComboRoots[index2].SelectedIndex.ToString() + ',' + allComboChords[index2].SelectedIndex.ToString());
                }
                else 
                {
                    DebugLog.addToLog(debugType.sendDebug, (string)Application.Current.Resources["InvalidChordRootIndex"]);
                }
            }
        }

        private void AllPatternComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (isPresetReading || setAllGuitarPatternComboBox.SelectedIndex==-1)
            {
                return;
            }
            for (int i = 0; i < allComboPatterns.Count; i++)
            {
                allComboPatterns[i].SelectedIndex = setAllGuitarPatternComboBox.SelectedIndex;
            }
            setAllGuitarPatternComboBox.SelectedIndex = -1;

        }

        private void PatternComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (isPresetReading)
            {
                return;
            }
            if (sender is ComboBox changedComboBox)
            {
                int index = allComboPatterns.IndexOf(changedComboBox);

                int row = -1;
                int col = -1;
                if (index != -1)
                {
                    row = index / NECK_COLS;
                    col = index % NECK_COLS;
                    SendCmd(SerialDevice.SET_NECK_PATTERN, curPreset.ToString() + ',' + row.ToString() + "," + col.ToString() + "," + allComboPatterns[index].SelectedIndex.ToString());
                }
                else
                {
                    DebugLog.addToLog(debugType.sendDebug, (string)Application.Current.Resources["InvalidPatternSettingIndex"]);
                }
            }
        }

        private void modeComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (isPresetReading || !initDone)
            {
                return;
            }
            SendCmd(SerialDevice.SET_DEVICE_MODE, curPreset.ToString() + ',' + modeComboBox.SelectedIndex.ToString());
        }
    }
}