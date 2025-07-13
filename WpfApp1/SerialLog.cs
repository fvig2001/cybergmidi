using CyberG;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
namespace CyberG
{
    class SerialLog
    {
        private static readonly object _lock = new object();
        public static ObservableCollection<string> LogEntries { get; } = new ObservableCollection<string>();
        SerialLog()
        {
        }

        public static void addToLog(string message)
        {
            lock (_lock)
            {
                Application.Current.Dispatcher.BeginInvoke(new Action(() =>
                {
                    LogEntries.Add(message);
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