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
    public const string SET_CHORD_HOLD = "CH_W";
    public const string GET_CHORD_HOLD = "CH_R";
    public const string SET_MUTE_WHEN_LET_GO = "MWLW";
    public const string GET_MUTE_WHEN_LET_GO = "MWLR";
    public const string SET_ALTERNATING_STRUM = "ASDW";
    public const string GET_ALTERNATING_STRUM = "ASDR";
    public const string SET_SUSTAIN_MODE = "UTSW";
    public const string GET_SUSTAIN_MODE = "UTSR";
    public const string SET_CHORD_MODE = "EANW";
    public const string GET_CHORD_MODE = "EANR";
    public const string SET_IGNORE_MODE = "ISCW";
    public const string GET_IGNORE_MODE = "ISCR";
    public const string SET_PROPER_OMNICHORD = "OM5W";
    public const string GET_PROPER_OMNICHORD = "OM5R";
    public const string SET_OMNICHORD_NOTE_MODE = "ONMW";
    public const string GET_OMNICHORD_NOTE_MODE = "ONMR";
    public const string SET_STRUM_STYLE = "SCSW";
    public const string GET_STRUM_STYLE = "SCSR";
    public const string SET_STRUM_SEPARATION = "STSW";
    public const string GET_STRUM_SEPARATION = "STSR";
    public const string SET_MUTE_SEPARATION = "GMSW";
    public const string GET_MUTE_SEPARATION = "GMSR";
    public const string GET_CAPO = "GTRR";
    public const string SET_CAPO = "GTRW";
    public const string GET_KB_TRANSPOSE = "OTOR";
    public const string SET_KB_TRANSPOSE = "OTOW";
    public const string SET_BASS_ENABLE = "BENW";
    public const string GET_BASS_ENABLE = "BENR";
    public const string SET_ACCOMPANIMENT_ENABLE = "AENW";
    public const string GET_ACCOMPANIMENT_ENABLE = "AENR";
    public const string SET_DRUMS_ENABLE = "DENW";
    public const string GET_DRUMS_ENABLE = "DENR";
    public const string GET_DEVICE_ID = "DEVI";
    public const string GET_DEVICE_MODE = "OMMR";
    public const string SET_DEVICE_MODE = "OMMW";
    public const string GET_PRESET = "PRSR";
    public const string SET_PRESET = "PRSW";
    public const string GET_ISKB = "ISKB";
    public const string SET_BPM = "BPMW";
    public const string GET_BPM = "BPMR";
    public const string SET_SIMPLE_CHORD_MODE = "SCMW";
    public const string GET_SIMPLE_CHORD_MODE = "SCMR";
    public const string GET_NECK_ASSIGNMENT = "NASR";
    public const string SET_NECK_ASSIGNMENT = "NASW";
    public const string GET_NECK_PATTERN = "NAPR";
    public const string SET_NECK_PATTERN = "NAPW";
    public const string STOP_ALL_NOTES = "STOP";
    public const string GET_BACKING_ID = "AIDR";
    public const string SET_BACKING_ID = "AIDW";
    public const string GET_DRUM_ID = "DIDR";
    public const string SET_DRUM_ID = "DIDW";
    public const string GET_BASS_ID = "BIDR";
    public const string SET_BASS_ID = "BIDW";
    public const string GET_GUITAR_ID = "GIDR";
    public const string SET_GUITAR_ID = "GIDW";
    public const string GET_BACKING_STATE = "HBCR";
    public const string SET_BACKING_STATE = "HBCW";
    public const string GET_DRUM_DATA = "DPRN";
    public const string GET_GUITAR_DATA = "GPRN";
    public const string GET_BASS_DATA = "BPRN";
    public const string GET_BACKING_DATA = "APRN";
    
    public const bool ignoreGetPreset = false;
    public const bool ignoreGetKB = false;
    private bool isWaiting = false;
    private  Queue<CommandItem> _commandQueue = new Queue<CommandItem>();
    private readonly AutoResetEvent _queueSignal = new AutoResetEvent(false);
    private readonly object _queueLock = new object();
    private bool _isRunning = true;
    private Thread _workerThread;

    //private string _lastCommandSent;
    //private string _lastParamSent;
    //private readonly Queue<string> _lastCommandSent = new Queue<string>();
    //private readonly Queue<string> _lastParamSent = new Queue<string>();
    private static Queue<string> _lastCommandSent = new Queue<string>();
    private static Queue<string> _lastParamSent = new Queue<string>();
    private readonly object _lock = new object();

    public string PortName { get; set; }
    public int BaudRate { get; set; }
    public bool isUSB { get; set; }

    private static SerialPort _serialPort;

    public bool IsConnected => _serialPort?.IsOpen ?? false;
    public event EventHandler<string> DataReceived;
    public event EventHandler DeviceDisconnected;

    private SerialDataReceivedEventHandler _internalHandler;

    public void RaiseDeviceDisconnected()
    {
        OnDeviceDisconnected();
        DeviceDisconnected?.Invoke(this, EventArgs.Empty);
        
    }
    public SerialDevice(string portName, int baudRate)
    {
        PortName = portName;
        BaudRate = baudRate;

        _serialPort = new SerialPort(PortName, BaudRate);
        _serialPort.ReadTimeout = 500; // milliseconds
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
                        lock (_lock)
                        {
                            _lastCommandSent.Enqueue(item.Command);
                            _lastParamSent.Enqueue(item.Param);
                        }
                        _serialPort.Write(sent);



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

    public bool HasDisconnectHandler(EventHandler handler)
    {
        return DeviceDisconnected?.GetInvocationList().Contains(handler) ?? false;
    }
    public void NotifyDisconnect()
    {
        DeviceDisconnected?.Invoke(this, EventArgs.Empty);
    }
    public void clearLastCommand()
    {
        lock (_lock)
        {
            lock (_queueLock)
            {
                _lastParamSent.Dequeue();
                _lastCommandSent.Dequeue();
                isWaiting = false;
                _queueSignal.Set();
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
        //_queueSignal.Set();
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
                return _lastCommandSent.First();
            }
        }
    }

    public string LastParamSent
    {
        get
        {
            lock (_lock)
            {
                return _lastParamSent.First();
            }
        }
    }
    private string serialBuffer = "";


    private void OnSerialDataReceived(object sender, SerialDataReceivedEventArgs e)
    {
        try
        {
            string incoming = _serialPort.ReadExisting();
            if (string.IsNullOrEmpty(incoming))
                return;

            serialBuffer += incoming;

            // Split on full lines
            string[] lines = serialBuffer.Split(new[] { "\r\n" }, StringSplitOptions.None);

            // Keep incomplete part (last item if not ending in \r\n)
            serialBuffer = lines[^1]; // ^1 means "last item"

            for (int i = 0; i < lines.Length - 1; i++) // skip last (incomplete)
            {
                string data = lines[i].Trim();
                if (string.IsNullOrWhiteSpace(data)) continue;

                bool isDebug = false;
                string cmd = "";
                string param = "";

                lock (_lock)
                {
                    if (data.StartsWith("DB"))
                    {
                        isDebug = true;
                    }
                    else if (data.StartsWith("NG"))
                    {
                        if (_lastCommandSent.Count > 0 && _lastParamSent.Count > 0)
                        {
                            DebugLog.addToLog(debugType.sendDebug, _lastCommandSent.First() + "," + _lastParamSent.First());
                        }
                        DebugLog.addToLog(debugType.replyDebug, data);
                    }

                    if (!isDebug && _lastCommandSent.Count > 0 && _lastParamSent.Count > 0)
                    {
                        cmd = _lastCommandSent.First();
                        param = _lastParamSent.First();
                    }

                    if (!(cmd == GET_PRESET && ignoreGetPreset) &&
                        !(cmd == GET_ISKB && ignoreGetKB))
                    {
                        SerialLog.addToLog(data);
                    }
                }

                DataReceived?.Invoke(this, data);
            }
        }
        catch (TimeoutException)
        {
            // Ignore and wait for more data
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
        public string Command = "";
        public string Param = "";
    }
}

public class SerialManager
{
    private static SerialDevice _instance;

    public static SerialDevice Device => _instance;

    public static bool isInitialized() => _instance != null;

    public static void Initialize(string port, int baud, bool forceConnect = false)
    {
        if (_instance == null)
            _instance = new SerialDevice(port, baud);
        else
        {
            _instance.PortName = port;
            _instance.BaudRate = baud;
            if (forceConnect)
            {
                _instance.Disconnect();
                _instance = null;
                _instance = new SerialDevice(port, baud);
                

            }
        }
    }

    public static bool isDeviceConnected()
    {
        return isInitialized() && Device.IsConnected;
    }

    public static void Disconnect()
    {
        _instance?.Disconnect();
        _instance = null;
    }
}
