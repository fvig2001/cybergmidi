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
    /// Interaction logic for SerialLogWindow.xaml
    /// </summary>
    public partial class SerialLogWindow : Window
    {
        private ScrollViewer _scrollViewer;
        private bool _autoScroll = true;
        public SerialLogWindow()
        {
            InitializeComponent();
        }

        private void clearButton_Click(object sender, RoutedEventArgs e)
        {
            SerialLog.clearLog();
        }


        private void MyListBox_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.C && Keyboard.Modifiers.HasFlag(ModifierKeys.Control))
            {
                if (LogListBox.SelectedItems != null && LogListBox.SelectedItems.Count > 0)
                {
                    string copiedText = string.Join(
                        System.Environment.NewLine,
                        LogListBox.SelectedItems.Cast<string>()
                    );

                    Clipboard.SetText(copiedText);
                    e.Handled = true;
                }
            }
        }

        private void LogListBox_Loaded(object sender, RoutedEventArgs e)
        {
            _scrollViewer = GetScrollViewer(LogListBox);

            if (_scrollViewer != null)
            {
                _scrollViewer.ScrollChanged += ScrollViewer_ScrollChanged;

                if (LogListBox.ItemsSource is INotifyCollectionChanged obs)
                {
                    obs.CollectionChanged += Items_CollectionChanged;
                }
            }
        }

        private void ScrollViewer_ScrollChanged(object sender, ScrollChangedEventArgs e)
        {
            // Check if user is at the bottom
            _autoScroll = _scrollViewer.VerticalOffset >= _scrollViewer.ScrollableHeight - 1;
        }

        private void Items_CollectionChanged(object sender, NotifyCollectionChangedEventArgs e)
        {
            if (e.Action == NotifyCollectionChangedAction.Add && _autoScroll)
            {
                // Delay scroll until layout has updated
                Dispatcher.InvokeAsync(() =>
                {
                    LogListBox.UpdateLayout(); // Force layout pass
                    if (LogListBox.Items.Count > 0)
                    {
                        LogListBox.ScrollIntoView(LogListBox.Items[LogListBox.Items.Count - 1]);
                    }
                }, DispatcherPriority.Background);
            }
        }
        private void InputBox_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
            {

                sendButton.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
                e.Handled = true; // Optional: prevent beep or other handling
            }
        }

        private ScrollViewer GetScrollViewer(DependencyObject o)
        {
            if (o is ScrollViewer) return o as ScrollViewer;

            for (int i = 0; i < VisualTreeHelper.GetChildrenCount(o); i++)
            {
                var child = VisualTreeHelper.GetChild(o, i);
                var result = GetScrollViewer(child);
                if (result != null) return result;
            }
            return null;
        }

        private void sendButton_Click(object sender, RoutedEventArgs e)
        {

            string[] parts = cmdTextbox.Text.Split(',');
            
            if (parts.Length > 0)
            {
                string cmd = parts[0];
                string param = "";
                for (int i = 1; i < parts.Length; i++)
                {
                    if (i > 1)
                    {
                        param = param + ",";
                    }
                    param = param + parts[i];
                }
                SerialManager.Device.Send(cmd, param);
            }

        }
    }

}
