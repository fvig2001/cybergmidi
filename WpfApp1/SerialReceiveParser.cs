using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
namespace CyberG
{
    class SerialReceiveParser
    {
        public static List<string> Parse(string received)
        {
            List<string> myList = new List<string>();

            if (string.IsNullOrWhiteSpace(received) || received.Length < 4)
            {
                //DebugLog.addToLog(debugType.miscDebug, "Received Serial Data is invalid!");

                return myList; // or throw exception if invalid format
            }

            // Extract first 2 chars (e.g., "OK" or "ER")
            string prefix = received.Substring(0, 2);
            // Extract next 2 chars (e.g., "00", "01")
            string status = received.Substring(2, 2);

            myList.Add(prefix);
            myList.Add(status);

            // If there is more (e.g., ",1,2"), split by commas
            if (received.Length > 4 && received[4] == ',')
            {
                string[] parts = received.Substring(5).Split(',');
                myList.AddRange(parts);
            }

            return myList;
        }
    }

}