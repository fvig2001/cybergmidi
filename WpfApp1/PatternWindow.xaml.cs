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

namespace CyberG
{
    /// <summary>
    /// Interaction logic for PatternWindow.xaml
    /// </summary>
    public partial class PatternWindow : Window
    {
        private const int MaxID = 65535;
        private const int MaxBackingState = 10;
        public int backingState = 0;
        private int dataToRead;
        private int dataRead;
        private bool isPatternReading;
        public PatternWindow()
        {
            isPatternReading = false;
            dataToRead = -1;
            dataRead = -1;
            InitializeComponent();
            setupComboBoxes();
            
            if (SerialManager.isDeviceConnected())
            {
                SerialManager.Device.DataReceived += OnPatternWindowSerialDataReceived;
                SendCmd(SerialDevice.GET_BACKING_STATE);
                SendCmd(SerialDevice.GET_DRUM_ID);
                SendCmd(SerialDevice.GET_BASS_ID);
                SendCmd(SerialDevice.GET_BACKING_ID);
                SendCmd(SerialDevice.GET_GUITAR_ID,"0");
                SendCmd(SerialDevice.GET_GUITAR_ID, "1");
                SendCmd(SerialDevice.GET_GUITAR_ID, "2");
            }
            getCurData();

        }
        private void getCurData()
        {
            string cmd = "";
            int pattern = -1;
            if (backingPatternRadioButton.IsChecked == true)
            {
                cmd = SerialDevice.GET_BACKING_DATA;
            }
            else if (drumPatternRadioButton.IsChecked == true)
            {
                cmd = SerialDevice.GET_DRUM_DATA;
            }
            else if (bassPatternRadioButton.IsChecked == true)
            {
                cmd = SerialDevice.GET_BASS_DATA;
            }
            else if (guitar0PatternRadioButton.IsChecked == true)
            {
                cmd = SerialDevice.GET_DRUM_DATA;
                pattern = 0;
            }
            else if (guitar1PatternRadioButton.IsChecked == true)
            {
                cmd = SerialDevice.GET_DRUM_DATA;
                pattern = 1;
            }
            else 
            {
                cmd = SerialDevice.GET_DRUM_DATA;
                pattern = 2;
            }
            if (pattern != -1)
            {
                SendCmd(cmd);
            }
            else
            { 
                SendCmd(cmd, pattern.ToString());
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
            int nTemp = -1;
            bool result = checkResult(parsedData);
            if (!result)
            {
                return false;
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
            else if (command == SerialDevice.GET_BACKING_ID || command == SerialDevice.GET_BASS_ID || command == SerialDevice.GET_DRUM_ID || command == SerialDevice.GET_GUITAR_ID)
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

            Dispatcher.Invoke(() => {
                int nTemp = -1;
                string response = data.Trim();
                List<string> parsedData = SerialReceiveParser.Parse(response);

                bool isDebug = false;
                if (parsedData.Count != 0)
                {
                    bool isOK = false;
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
                        if (command == SerialDevice.GET_BACKING_STATE)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                backingState = int.Parse(parsedData[2]);
                                BackingModeComboBox.SelectedIndex = backingState;
                            }
                        }
                        else if (command == SerialDevice.GET_DRUM_ID)
                        {
                            if (checkReceivedValid(parsedData))
                            {
                                nTemp = int.Parse(parsedData[2]);
                                drumPatternTextBox.Text = nTemp.ToString();
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
                                int temp = int.Parse(SerialManager.Device.LastCommandSent);
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
                                }
                            }
                        }

                        if (!isOK)
                        {
                            MessageBox.Show((string)Application.Current.Resources["Error"] + " " + SerialManager.Device.PortName + " " + (string)Application.Current.Resources["WrongDevice"]);
                        }
                        //if here 
                    }
                }
                if (!isDebug)
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
            if (SerialManager.isDeviceConnected())
            {
                SendCmd(SerialDevice.SET_BACKING_STATE, BackingModeComboBox.SelectedIndex.ToString());
            }
            if (!isCustom)
            {
                backingPatternRadioButton.IsEnabled = false;
                drumPatternRadioButton.IsEnabled = false;
                bassPatternRadioButton.IsEnabled = false;
                backingPatternTextBox.IsEnabled = false;
                drumPatternTextBox.IsEnabled = false;
                bassPatternTextBox.IsEnabled = false;
                if (backingPatternRadioButton.IsChecked == true || drumPatternRadioButton.IsChecked == true || bassPatternRadioButton.IsChecked == true)
                {
                    guitar0PatternRadioButton.IsChecked = true;
                }
            }
            else
            {
                backingPatternRadioButton.IsEnabled = true;
                drumPatternRadioButton.IsEnabled = true;
                bassPatternRadioButton.IsEnabled = true;
                backingPatternTextBox.IsEnabled = true;
                drumPatternTextBox.IsEnabled = true;
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
            //end handling serial
            if (SerialManager.isDeviceConnected())
            {
                SerialManager.Device.DataReceived -= OnPatternWindowSerialDataReceived;
            }
            base.OnClosed(e);
        }
    }
}
