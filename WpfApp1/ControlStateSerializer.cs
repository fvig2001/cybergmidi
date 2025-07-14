using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace CyberG
{
    using System.IO;
    using System.Text.Json;
    using System.Windows;
    using System.Windows.Controls;
    using System.Collections.Generic;
    using System.Windows.Media;

    public static class ControlStateSerializer
    {
        public static void SaveControlStates(Window window, string filePath, int configId)
        {
            Dictionary<string, object> allData;

            // Safely load the existing JSON file if it exists
            try
            {
                if (File.Exists(filePath))
                {
                    string json = File.ReadAllText(filePath);
                    allData = JsonSerializer.Deserialize<Dictionary<string, object>>(json) ?? new Dictionary<string, object>();
                }
                else
                {
                    allData = new Dictionary<string, object>();
                }
            }
            catch
            {
                // fallback in case of invalid or corrupt file
                allData = new Dictionary<string, object>();
            }

            var state = new Dictionary<string, object>();

            // Save non-radio controls first
            foreach (var control in GetAllControls(window))
            {
                if (control is RadioButton) continue;

                if (control is TextBox tb && !string.IsNullOrEmpty(tb.Name))
                    state[tb.Name] = tb.Text;
                else if (control is CheckBox cb && !string.IsNullOrEmpty(cb.Name))
                    state[cb.Name] = cb.IsChecked;
                else if (control is ComboBox combo && !string.IsNullOrEmpty(combo.Name))
                    state[combo.Name] = combo.SelectedIndex;
                else if (control is Slider slider && !string.IsNullOrEmpty(slider.Name))
                    state[slider.Name] = slider.Value;
            }

            // Handle RadioButtons separately
            var radiosUnchecked = new List<RadioButton>();
            var radiosChecked = new List<RadioButton>();

            foreach (var control in GetAllControls(window))
            {
                if (control is RadioButton rb && !string.IsNullOrEmpty(rb.Name))
                {
                    if (rb.IsChecked == true)
                        radiosChecked.Add(rb);
                    else
                        radiosUnchecked.Add(rb);
                }
            }

            foreach (var rb in radiosUnchecked)
                state[rb.Name] = rb.IsChecked;
            foreach (var rb in radiosChecked)
                state[rb.Name] = rb.IsChecked;

            // Deserialize or create the ControlStates section
            Dictionary<string, Dictionary<string, object>> controlStatesDict = new();

            if (allData.TryGetValue("ControlStates", out var controlStatesObj))
            {
                if (controlStatesObj is JsonElement element && element.ValueKind == JsonValueKind.Object)
                {
                    try
                    {
                        controlStatesDict = JsonSerializer.Deserialize<Dictionary<string, Dictionary<string, object>>>(element.GetRawText())
                                            ?? new Dictionary<string, Dictionary<string, object>>();
                    }
                    catch
                    {
                        controlStatesDict = new Dictionary<string, Dictionary<string, object>>();
                    }
                }
                else if (controlStatesObj is Dictionary<string, Dictionary<string, object>> typedDict)
                {
                    controlStatesDict = typedDict;
                }
            }

            // Add or update this config ID
            controlStatesDict[configId.ToString()] = state;

            // Replace in full structure
            allData["ControlStates"] = controlStatesDict;

            // Save to disk
            File.WriteAllText(filePath, JsonSerializer.Serialize(allData, new JsonSerializerOptions { WriteIndented = true }));
        }

        public static bool GetBoolResult(object value)
        {
            bool res = false;
            if (value is JsonElement je && je.ValueKind == JsonValueKind.True)
            {
                res = true;
            }
            else if (value is JsonElement je2 && je2.ValueKind == JsonValueKind.False)
            {
                res = false;
            }
            else if (value is bool b)
            {
                res = b;
            }
            else
            {
                res = false; // fallback
            }
            return res;
        }
        public static bool LoadControlStates(Window window, string filePath, int configId)
        {
            if (!File.Exists(filePath)) return false;

            var allData = JsonSerializer.Deserialize<Dictionary<string, object>>(File.ReadAllText(filePath));
            if (!allData.TryGetValue("ControlStates", out var controlStatesObj)) return false;

            var controlStatesJson = (JsonElement)controlStatesObj;
            var controlStatesDict = JsonSerializer.Deserialize<Dictionary<string, Dictionary<string, object>>>(controlStatesJson.GetRawText());

            if (!controlStatesDict.TryGetValue(configId.ToString(), out var state)) return false;

            bool res = false;
            string temp = "";

            foreach (var control in GetAllControls(window))
            {
                if (control is FrameworkElement fe && state.TryGetValue(fe.Name, out var value))
                {
                    switch (control)
                    {
                        case TextBox tb:
                            temp = value?.ToString();
                            tb.Text = temp + " ";
                            tb.Text = temp;
                            break;
                        case CheckBox cb:
                            res = GetBoolResult(value);
                            cb.IsChecked = !res;
                            cb.IsChecked = res;
                            break;
                        case ComboBox combo when int.TryParse(value?.ToString(), out int index):
                            combo.SelectedIndex = index == -1 ? 0 : -1;
                            combo.SelectedIndex = index;
                            break;
                        case RadioButton rb:
                            res = GetBoolResult(value);
                            rb.IsChecked = !res;
                            rb.IsChecked = res;
                            break;
                        case Slider slider when double.TryParse(value?.ToString(), out double val):
                            if (val != slider.Maximum) slider.Value = slider.Maximum;
                            else if (val != slider.Minimum) slider.Value = slider.Minimum;
                            slider.Value = val;
                            break;
                    }
                }
            }

            return true;
        }

        public static Dictionary<string, string> LoadManualSettings(string filePath)
        {
            if (!File.Exists(filePath)) return new Dictionary<string, string>();

            var allData = JsonSerializer.Deserialize<Dictionary<string, object>>(File.ReadAllText(filePath));
            if (!allData.TryGetValue("ManualConfig", out var configObj)) return new Dictionary<string, string>();

            var configElement = (JsonElement)configObj;
            var result = new Dictionary<string, string>();

            foreach (var prop in configElement.EnumerateObject())
            {
                //if (int.TryParse(prop.Value.GetRawText(), out string val))
                    //result[prop.Name] = val;
                result[prop.Name] = prop.Value.GetRawText();
            }

            return result;
        }

        public static void SaveManualSettings(string filePath, Dictionary<string, string> settings)
        {
            Dictionary<string, object> allData = File.Exists(filePath)
                ? JsonSerializer.Deserialize<Dictionary<string, object>>(File.ReadAllText(filePath))
                : new Dictionary<string, object>();

            allData["ManualConfig"] = settings;
            File.WriteAllText(filePath, JsonSerializer.Serialize(allData, new JsonSerializerOptions { WriteIndented = true }));
        }

        private static IEnumerable<Control> GetAllControls(DependencyObject parent)
        {
            if (parent == null) yield break;

            for (int i = 0; i < VisualTreeHelper.GetChildrenCount(parent); i++)
            {
                var child = VisualTreeHelper.GetChild(parent, i);

                if (child is Control control)
                    yield return control;

                foreach (var descendant in GetAllControls(child))
                    yield return descendant;
            }
        }
    }
}
