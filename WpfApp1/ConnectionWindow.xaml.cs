using System;
using System.Collections.Generic;
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
        private bool connectOK = false;
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
                    // Extract numeric part for correct sorting (e.g. COM1, COM2, COM10)
                    var match = Regex.Match(port, @"COM(\d+)");
                    return match.Success ? int.Parse(match.Groups[1].Value) : int.MaxValue;
                })
                .ToList();

            comPortComboBox.ItemsSource = sortedPorts;

            if (sortedPorts.Count > 0 && forceConnect)
            {
                comPortComboBox.SelectedIndex = -1;
            }
            else if (!forceConnect && sortedPorts.Count > 0)
            {
                string currentPort = SerialManager.Device.PortName;

                if (!string.IsNullOrWhiteSpace(currentPort))
                {
                    myPort = sortedPorts.IndexOf(currentPort);

                    if (myPort >= 0)
                    {
                        // current port found in the list — do something with myPort
                        comPortComboBox.SelectedIndex = myPort;
                    }
                    else
                    {
                        // Port not found — optionally select nothing or handle it
                        comPortComboBox.SelectedIndex = -1;
                    }
                }
            }
            else
            {
                comPortComboBox.ItemsSource = new string[] { "" }; // Empty item
                comPortComboBox.SelectedIndex = 0;
            }
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
                    DebugLog.addToLog(debugType.replyDebug, "Received Parsed Data on Serial is 0");
                }
                else
                {
                    int nTemp = -1;
                    bool bTemp = false;
                    //curPreset
                    if (command == DEVICEIDCMD)
                    {
                        if (parsedData.Count >= 3)
                        {
                            //we don't care about first 2
                            nTemp = int.Parse(parsedData[2]);
                            if (nTemp == 0)
                            {
                                connectOK = true;
                                isGood = true;
                            }
                        }

                    }
                    else
                    {
                        MessageBox.Show("Received wrong command's reply: " + command);
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
                    Close();
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
