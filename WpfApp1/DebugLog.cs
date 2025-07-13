using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;

namespace CyberG
{
    public enum debugType
    {
        sendDebug = 0,
        replyDebug,
        miscDebug,
        deviceDebug
    }
    class DebugLog
    {
        private static readonly object _lock = new object();
        //private static List<string> debugLog = new List<string>();
        public static ObservableCollection<string> LogEntries { get; } = new ObservableCollection<string>();

        public static void addToLog(debugType type, string message)
        {
            lock (_lock)
            {
                Application.Current.Dispatcher.BeginInvoke(new Action(() =>
                {
                    LogEntries.Add(type + " : " + message);
                }));
            }
        }
        public static void clearLog()
        {
            lock (_lock)
            {
                LogEntries.Clear();
            }
        }

    }

}