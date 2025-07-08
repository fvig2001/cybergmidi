using CyberG;
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Ports;
using System.Linq;
using System.Management;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;

public class SerialDevice
{
    public const string GET_PRESET = "PRSR";
    public const string GET_ISKB = "ISKB";
    public const bool ignoreGetPreset = false;
    public const bool ignoreGetKB = false;
    private bool isWaiting = false;
    private readonly Queue<CommandItem> _commandQueue = new Queue<CommandItem>();
    private readonly AutoResetEvent _queueSignal = new AutoResetEvent(false);
    private readonly object _queueLock = new object();
    private bool _isRunning = true;
    private Thread _workerThread;

    private string _lastCommandSent;
    private string _lastParamSent;
    private readonly object _lock = new object();

    public string PortName { get; set; }
    public int BaudRate { get; set; }
    public bool isUSB { get; set; }

    private SerialPort _serialPort;

    public bool IsConnected => _serialPort?.IsOpen ?? false;
    public event EventHandler<string> DataReceived;
    public event EventHandler DeviceDisconnected;

    private SerialDataReceivedEventHandler _internalHandler;

    public SerialDevice(string portName, int baudRate)
    {
        PortName = portName;
        BaudRate = baudRate;

        _serialPort = new SerialPort(PortName, BaudRate);
        _serialPort.ReadTimeout = 5000; // milliseconds
        isUSB = DetermineIfUSB(portName);
        _internalHandler = new SerialDataReceivedEventHandler(OnSerialDataReceived);
        _serialPort.DataReceived += _internalHandler;
        
        StartCommandProcessor();
    }

    private bool DetermineIfUSB(string portName)
    {
        using (var searcher = new ManagementObjectSearcher("SELECT * FROM Win32_PnPEntity WHERE Name LIKE '%(" + portName + ")%'"))
        {
            foreach (ManagementObject obj in searcher.Get())
            {
                string name = obj["Name"]?.ToString() ?? "";
                string pnpId = obj["PNPDeviceID"]?.ToString() ?? "";

                if (pnpId.Contains("USB")) return true;
                if (pnpId.Contains("BTHENUM")) return false;
                if (name.ToLower().Contains("bluetooth")) return false;
                if (name.ToLower().Contains("usb")) return true;

                return false;
            }
        }
        return false;
    }

    private void StartCommandProcessor()
    {
        _workerThread = new Thread(() =>
        {
            while (_isRunning)
            {
                CommandItem item = null;
                
                lock (_queueLock)
                {
                    if (!isWaiting)
                    {
                        //todo block if something is waiting
                        if (_commandQueue.Count > 0)
                            item = _commandQueue.Dequeue();
                    }
                }

                if (item != null)
                {
                    try
                    {
                        if (!_serialPort.IsOpen)
                            throw new InvalidOperationException("Serial port not open.");

                        string sent = string.IsNullOrEmpty(item.Param)
                        ? item.Command + "\r\n"
                        : item.Command + "," + item.Param + "\r\n";
                        //string sent = item.Command + "," + item.Param + "\r\n";
                        isWaiting = true;
                        _serialPort.WriteLine(sent);

                        lock (_lock)
                        {
                            _lastCommandSent = item.Command;
                            _lastParamSent = item.Param;
                        }

                        if (!(item.Command == GET_PRESET && ignoreGetPreset) &&
                            !(item.Command == GET_ISKB && ignoreGetKB))
                        {
                            SerialLog.addToLog(sent);
                        }
                    }
                    catch (Exception ex)
                    {
                        DebugLog.addToLog(debugType.sendDebug, "Error in serial send: " + ex.Message);
                    }
                }
                else
                {
                    _queueSignal.WaitOne();
                }
            }
        });
        _workerThread.IsBackground = true;
        _workerThread.Start();
    }

    public void Send(string command, string param = "")
    {
        var item = new CommandItem
        {
            Command = command,
            Param = param
        };

        lock (_queueLock)
        {
            _commandQueue.Enqueue(item);
        }

        _queueSignal.Set();
    }

    public void clearLastCommand()
    {
        lock (_lock)
        {
            lock (_queueLock)
            {
                _lastParamSent = "";
                _lastCommandSent = "";
                isWaiting = false;
            }
        }
    }

    public bool Connect()
    {
        if (_serialPort != null && !_serialPort.IsOpen)
        {
            try
            {
                _serialPort.Open();
                _serialPort.DiscardInBuffer();
                _serialPort.DiscardOutBuffer();
                return true;
            }
            catch
            {
                return false;
            }
        }
        return false;
    }

    public void Disconnect()
    {
        _isRunning = false;
        _queueSignal.Set();
        if (_serialPort != null && _serialPort.IsOpen)
        {
            _serialPort.Close();
        }
    }

    public string LastCommandSent
    {
        get 
        { 
            lock (_lock) 
            {
                return _lastCommandSent; 
            } 
        }
    }

    public string LastParamSent
    {
        get
        { 
            lock (_lock) 
            {
                return _lastCommandSent; 
            } 
        }
    }

    private void OnSerialDataReceived(object sender, SerialDataReceivedEventArgs e)
    {
        try
        {
            while (true)
            {
                string data = _serialPort.ReadLine(); // waits until \n or timeout
                if (string.IsNullOrWhiteSpace(data)) continue;

                lock (_lock)
                {
                    if (!data.StartsWith("OK"))
                    {
                        //if (_lastCommandSent.Count > 0 && _lastParamSent.Count > 0)
                        {
                            DebugLog.addToLog(debugType.sendDebug, _lastCommandSent + "," + _lastParamSent);
                        }
                        DebugLog.addToLog(debugType.replyDebug, data);
                    }
                    
                    //if (_lastCommandSent.Count > 0 && _lastParamSent.Count > 0)
                    {
                        string cmd = _lastCommandSent;
                        string param = _lastParamSent;

                        if (!(cmd == GET_PRESET && ignoreGetPreset) &&
                            !(cmd == GET_ISKB && ignoreGetKB))
                        {
                            SerialLog.addToLog(data);
                        }

                        //isWaiting = false;
                    }
                }
                if (DataReceived != null)
                {
                    DataReceived?.Invoke(this, data);
                    break;
                }
                
            }
        }
        catch (TimeoutException)
        {
            // No full line received yet
            int i = 0;
        }
        catch (IOException)
        {
            OnDeviceDisconnected();
        }
        catch (InvalidOperationException)
        {
            OnDeviceDisconnected();
        }
    }

    private void OnDeviceDisconnected()
    {
        Disconnect();
        DeviceDisconnected?.Invoke(this, EventArgs.Empty);
    }

    private class CommandItem
    {
        public string Command;
        public string Param;
    }
}

public class SerialManager
{
    private static SerialDevice _instance;

    public static SerialDevice Device => _instance;

    public static bool isInitialized() => _instance != null;

    public static void Initialize(string port, int baud)
    {
        if (_instance == null)
            _instance = new SerialDevice(port, baud);
        else
        {
            _instance.PortName = port;
            _instance.BaudRate = baud;
        }
    }

    public static bool isDeviceConnected() => isInitialized() && Device.IsConnected;

    public static void Disconnect()
    {
        _instance?.Disconnect();
        _instance = null;
    }
}
