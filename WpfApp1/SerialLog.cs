using CyberG;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
namespace CyberG
{
    class SerialLog
    {
        private static readonly object _lock = new object();
        public static ObservableCollection<string> LogEntries { get; } = new ObservableCollection<string>();

        public static void addToLog(string message)
        {
            lock (_lock)
            {
                //debugLog.Add(type.ToString() + " : " + message);
                LogEntries.Add(message);
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