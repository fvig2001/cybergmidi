using System;
using System.Collections.Generic;
using System.Collections.Specialized;
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
using System.Windows.Threading;

namespace CyberG
{
    /// <summary>
    /// Interaction logic for DebugLogWindow.xaml
    /// </summary>
    public partial class DebugLogWindow : Window
    {
        public DebugLogWindow()
        {
            InitializeComponent();
            ((INotifyCollectionChanged)DebugLog.LogEntries).CollectionChanged += LogListBox_OnCollectionChanged;

        }
        private void LogListBox_OnCollectionChanged(object sender, NotifyCollectionChangedEventArgs e)
        {
            if (e.Action == NotifyCollectionChangedAction.Add)
            {
                if (IsScrolledToBottom())
                    LogListBox.ScrollIntoView(LogListBox.Items[LogListBox.Items.Count - 1]);
            }
        }

        private bool IsScrolledToBottom()
        {
            if (VisualTreeHelper.GetChild(LogListBox, 0) is Border border &&
                border.Child is ScrollViewer scrollViewer)
            {
                return scrollViewer.VerticalOffset == scrollViewer.ScrollableHeight;
            }
            return false;
        }

        private void clearButton_Click(object sender, RoutedEventArgs e)
        {
            DebugLog.clearLog();
        }
    }
}
