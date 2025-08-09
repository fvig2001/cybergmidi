#define CONFIG_BT_ENABLED true
#define CONFIG_BLUEDROID_ENABLED true
#define CONFIG_BT_SPP_ENABLED true
#include "BluetoothSerial.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"

#define DEBUG true
BluetoothSerial SerialBT;

#define TEENSY_RX 16
#define TEENSY_TX 17

HardwareSerial TeensySerial(2);

// Configurable buffers
#define BT_BUF_SIZE     256
#define TEENSY_BUF_SIZE 256

static char btBuf[BT_BUF_SIZE];
static size_t btPos = 0;

static char teensyBuf[TEENSY_BUF_SIZE];
static size_t teensyPos = 0;

static uint32_t lastCheck = 0;

// Configurable flags
bool bt_enabled = true;
bool bt_discoverable = true;

// Optional: Callback to track connections
void btCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  if (event == ESP_SPP_SRV_OPEN_EVT) {
    if (DEBUG)
      Serial.println("Client Connected!");
    TeensySerial.print("CONN,1\r\n");
  } else if (event == ESP_SPP_CLOSE_EVT) {
    if (DEBUG)
      Serial.println("Client Disconnected!");
    TeensySerial.print("DCDC,1\r\n");
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  TeensySerial.begin(9600, SERIAL_8N1, TEENSY_RX, TEENSY_TX);

  if (bt_enabled) {
    SerialBT.begin("CyberG_BT_Settings");
    SerialBT.register_callback(btCallback);
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    if (DEBUG)
      Serial.println("Bluetooth device started, waiting for connection...");
  }
}

void processBluetoothLine(const char *line) {
  Serial.print("Received via BT: ");
  Serial.println(line);

  // Forward to Teensy
  if (bt_enabled && SerialBT.hasClient()) {
    TeensySerial.printf(line);
    TeensySerial.print("\r\n");
  }
}

void processTeensyLine(const char *line) {
  Serial.print("Received via Teensy: ");
  Serial.println(line);

  if (strncmp(line, "BTMW,", 5) == 0) {
    int val = atoi(line + 5);
    bt_discoverable = (val >= 1);
    if (bt_enabled) {
      if (bt_discoverable) {
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
      } else {
        esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
      }
    }
    TeensySerial.print("OK00\r\n");
  }
  else if (strcmp(line, "BTMR") == 0) {
    TeensySerial.printf("OK00,%d\r\n", bt_discoverable ? 1 : 0);
  }
  else if (strcmp(line, "BTRA") == 0) {
    if (bt_enabled && SerialBT.hasClient()) {
      SerialBT.disconnect();
    }
    TeensySerial.print("OK00\r\n");
  }
  else if (strncmp(line, "BTOW,", 5) == 0) {
    int val = atoi(line + 5);
    bool res = true;
    if (val >= 1 && !bt_enabled) {
      SerialBT.begin("CyberG_BT_Settings");
      SerialBT.setTimeout(500);
      if (bt_discoverable) {
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
      } else {
        esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
      }
      bt_enabled = true;
      if (DEBUG) Serial.println("Bluetooth enabled");
    }
    else if (val == 0 && bt_enabled) {
      SerialBT.end();
      bt_enabled = false;
      if (DEBUG) Serial.println("Bluetooth disabled");
    }
    else if (!(val >= 1 || val == 0)) {
      res = false;
    }
    TeensySerial.print(res ? "OK00\r\n" : "ER00\r\n");
  }
  else if (strcmp(line, "BTOR") == 0) {
    TeensySerial.printf("OK00,%d\r\n", bt_enabled ? 1 : 0);
  }
  else if (strcmp(line, "BTCR") == 0) {
    TeensySerial.printf("OK00,%d\r\n", SerialBT.hasClient() ? 1 : 0);
  }
  else
  {
    Serial.printf("Forward data back to BT\n");
    SerialBT.printf("%s\r\n", line);
  }
}

void handleIncomingChar(char c, char *buf, size_t &pos, size_t bufSize, void (*processLine)(const char *)) {
  if (c == '\n') {
    // Trim trailing \r
    if (pos > 0 && buf[pos - 1] == '\r') {
      buf[pos - 1] = '\0';
    } else {
      buf[pos] = '\0';
    }
    if (pos > 0) {
      processLine(buf);
    }
    pos = 0; // Reset for next line
  }
  else if (pos < bufSize - 1) {
    buf[pos++] = c;
  }
}

void loop() {
  // Relay from Bluetooth to Teensy
  while (SerialBT.available()) {
    char c = SerialBT.read();
    handleIncomingChar(c, btBuf, btPos, BT_BUF_SIZE, processBluetoothLine);
  }

  // Relay from Teensy to Bluetooth or handle commands
  while (TeensySerial.available()) {
    char c = TeensySerial.read();
    handleIncomingChar(c, teensyBuf, teensyPos, TEENSY_BUF_SIZE, processTeensyLine);
  }
/*
  // Periodic check
  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    if (bt_enabled && !SerialBT.hasClient()) {
      if (DEBUG) Serial.println("No client connected");
    }
  }
  */
}
