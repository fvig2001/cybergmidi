using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Ports;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
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
    /// Interaction logic for ConnectionWindow.xaml
    /// </summary>
    public partial class ConnectionWindow : Window
    {
        private const string iniPath = "lastCOM.ini";
        //private bool connectOK = false;
        private const string DEVICEIDCMD = "DEVI";
        private const int USB_SERIAL_BAUD = 9600; //not really used
        private const int BT_SERIAL_BAUD = 9600;
        private bool forceConnect;
        public ConnectionWindow()
        {
            InitializeComponent();
            InitializeState();
            ResetComPortComboBox();
            //start handling serial
            if (SerialManager.isDeviceConnected())
            {
                SerialManager.Device.DataReceived += OnSettingsSerialDataReceived;
                forceConnect = false;
            }

        }
        private void ResetComPortComboBox()
        {
            int myPort = -1;
            var ports = SerialPort.GetPortNames().Distinct();
            var sortedPorts = ports
                .OrderBy(port =>
                {
                    var match = Regex.Match(port, @"COM(\d+)");
                    return match.Success ? int.Parse(match.Groups[1].Value) : int.MaxValue;
                })
                .ToList();

            comPortComboBox.ItemsSource = sortedPorts;

            if (sortedPorts.Count == 0)
            {
                comPortComboBox.ItemsSource = new string[] { "" }; // Empty item
                comPortComboBox.SelectedIndex = 0;
                return;
            }
            /*
            if (forceConnect)
            {
                comPortComboBox.SelectedIndex = -1;
                return;
            }
            */

            // Try to get current port from SerialManager
            string currentPort = SerialManager.Device?.PortName;
            
            // If current port is valid and exists in list, select it
            if (!string.IsNullOrWhiteSpace(currentPort) && sortedPorts.Contains(currentPort))
            {
                comPortComboBox.SelectedItem = currentPort;
                return;
            }

            // Check if lastCOM.ini exists
            
            else if (File.Exists(iniPath))
            {
                string savedPort = File.ReadAllText(iniPath).Trim();
                if (!string.IsNullOrWhiteSpace(savedPort) && sortedPorts.Contains(savedPort))
                {
                    comPortComboBox.SelectedItem = savedPort;
                    return;
                }
            }

            // Default to first port
            comPortComboBox.SelectedIndex = 0;
        }

        private void InitializeState()
        {
            if (SerialManager.isDeviceConnected())
            {
                forceConnect = false;
            }
            else
            {
                forceConnect = true;
            }
        }

        private void OnSettingsSerialDataReceived(object sender, string data)
        {
            Dispatcher.Invoke(() =>
            {
                string command = SerialManager.Device.LastCommandSent;
                string response = data.Trim();
                List<string> parsedData = SerialReceiveParser.Parse(response);
                bool isGood = false;
                if (parsedData.Count == 0)
                {
                    DebugLog.addToLog(debugType.replyDebug, (string)Application.Current.Resources["NoDataReceived"]);
                }
                else
                {
                    int nTemp = -1;
                    //bool bTemp = false;
                    //curPreset
                    if (command == DEVICEIDCMD)
                    {
                        if (parsedData.Count >= 3)
                        {
                            //we don't care about first 2
                            nTemp = int.Parse(parsedData[2]);
                            if (nTemp == 0)
                            {
                                //connectOK = true;
                                isGood = true;
                            }
                        }

                    }
                    else
                    {
                        MessageBox.Show((string)Application.Current.Resources["WrongReply"] + " " + command);
                    }
                }
                //if (isGood)
                {
                    //clean up serial state to allow next serial command
                    SerialManager.Device.clearLastCommand();
                }

            });
        }
        protected override void OnClosed(EventArgs e)
        {
            //end handling serial
            if (!forceConnect)
            {
                SerialManager.Device.DataReceived -= OnSettingsSerialDataReceived;
            }
            base.OnClosed(e);
        }

        private void refreshButton_Click(object sender, RoutedEventArgs e)
        {
            ResetComPortComboBox();
        }
        private void PauseSerial()
        {
            if (SerialManager.Device != null)
            {
                SerialManager.Device.DataReceived -= OnConnectionWindowSerialDataReceived;
            }
        }
        private void ResumeSerial()
        {
            if (SerialManager.Device != null)
            {
                SerialManager.Device.DataReceived += OnConnectionWindowSerialDataReceived;
            }
        }

        private void OnConnectionWindowSerialDataReceived(object sender, string data)
        {

            Dispatcher.Invoke(() => {
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
                    //int nTemp = -1;
                    //bool bTemp = false;
                    //curPreset
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
                        if (command == SerialDevice.GET_DEVICE_ID)
                        {
                            if (parsedData.Count >= 2)
                            {
                                
                                if (parsedData[0] == "OK" && parsedData[2] == "0")
                                {
                                    isOK = true;
                                    PauseSerial();
                                    // === Save current COM port to iniPath if needed ===
                                    if (!string.IsNullOrWhiteSpace(iniPath)) // iniPath should be declared elsewhere as a member
                                    {
                                        try
                                        {
                                            string currentPort = SerialManager.Device?.PortName;

                                            if (!string.IsNullOrWhiteSpace(currentPort))
                                            {
                                                bool shouldWrite = true;

                                                if (File.Exists(iniPath))
                                                {
                                                    string existingPort = File.ReadAllText(iniPath).Trim();
                                                    if (string.Equals(existingPort, currentPort, StringComparison.OrdinalIgnoreCase))
                                                    {
                                                        shouldWrite = false; // No need to update
                                                    }
                                                }

                                                if (shouldWrite)
                                                {
                                                    File.WriteAllText(iniPath, currentPort);
                                                }
                                            }
                                        }
                                        catch (Exception ex)
                                        {
                                            // Optional: handle/log exception
                                            Debug.WriteLine("Error writing to iniPath: " + ex.Message);
                                        }
                                    }
                                    Close();
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
        private void connectButton_Click(object sender, RoutedEventArgs e)
        {
            string selectedPort = comPortComboBox.SelectedItem as string;
            bool skip = false;
            if (!forceConnect)
            {
                if (selectedPort == SerialManager.Device.PortName)
                {
                    skip = true;
                }
                else
                {
                    SerialManager.Device.Disconnect();
                }
            }
            if (!skip && !string.IsNullOrWhiteSpace(selectedPort))
            {
                SerialManager.Initialize(selectedPort, USB_SERIAL_BAUD, forceConnect);
                if (SerialManager.Device.Connect())
                {
                    //todo: consider case where there is no reply
                    //SerialManager.Device.Send("DEVI");
                    //Close();
                    ResumeSerial();
                    SendCmd(SerialDevice.GET_DEVICE_ID);
                }
                else
                {
                    MessageBox.Show((string)Application.Current.Resources["ComConnectFailed"]);
                }
            }
            else if (!skip)
            {
                MessageBox.Show((string)Application.Current.Resources["NoComPortSelected"]);
            }
            else
            {
                //skip
                Close();
            }
        }
    }
}
