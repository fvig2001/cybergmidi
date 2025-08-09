#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <deque>
#include <vector>

std::deque<std::vector<uint8_t>> midiQueue;
unsigned long lastMidiNotifyTime = 0;
const int midiNotifyIntervalMs = 5;
const int bleMaxPayload = 20;
#define DEBUG true
#define MIDI_SERVICE_UUID "03B80E5A-EDE8-4B33-A751-6CE34EC4C700"
#define MIDI_CHARACTERISTIC_UUID "7772E5DB-3868-4112-A1A9-F2669D106BF3"

#define TEENSY_RX_2 16  //commands
#define TEENSY_TX_2 17
#define TEENSY_RX_1 18  //BLE
#define TEENSY_TX_1 19

BLECharacteristic* midiCharacteristic;
BLEServer* pServer;
BLEAdvertising* advertising;

bool deviceConnected = false;
bool bleEnabled = true;
bool bleDiscoverable = true;

HardwareSerial& CommandSerial = Serial1;
HardwareSerial& MidiSerial = Serial2;

// BLE server callbacks
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    if (DEBUG)
      Serial.println("BLE client connected.");
    CommandSerial.write("CONN,0\r\n");
  }

  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    if (DEBUG)
      Serial.println("BLE client disconnected.");
    if (bleDiscoverable)
      advertising->start();
    CommandSerial.write("DCDC,0\r\n");
  }
};

void setupBLE() {
  if (!bleEnabled) {
    if (DEBUG)
      Serial.println("BLE not enabled.");
    return;
  }
  if (DEBUG)
    Serial.println("Starting BLE init...");
  BLEDevice::init("CyberG-BLE-MIDI");

  if (DEBUG)
    Serial.println("Creating BLE server...");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  if (DEBUG)
    Serial.println("Creating MIDI service...");
  BLEService* midiService = pServer->createService(MIDI_SERVICE_UUID);

  if (DEBUG)
    Serial.println("Creating MIDI characteristic...");
  midiCharacteristic = midiService->createCharacteristic(
    MIDI_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE_NR);
  if (DEBUG)
    Serial.println("Adding descriptor...");
  midiCharacteristic->addDescriptor(new BLE2902());

  if (DEBUG)
    Serial.println("Starting service...");
  midiService->start();

  if (DEBUG)
    Serial.println("Setting up advertising...");
  advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(MIDI_SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMinPreferred(0x12);

  if (bleDiscoverable) {
    if (DEBUG)
      Serial.println("Starting advertising...");
    advertising->start();
  }
  if (DEBUG)
    Serial.println("BLE MIDI ready.");
}

void disableBLE() {
  BLEDevice::deinit(true);  // fully reset BLE stack
  if (DEBUG)
    Serial.println("BLE disabled.");
}

void handleCommand(const String& line) {
  Serial.printf("Command received is %s\n", line.c_str());
  if (line.startsWith("BTMW,")) {
    bleDiscoverable = line.endsWith("1");
    if (bleEnabled) {
      if (bleDiscoverable)
        advertising->start();
      else
        advertising->stop();
    }
    CommandSerial.print("OK00\r\n");

  } else if (line.startsWith("BTMR")) {
    CommandSerial.printf("OK00,%d\r\n", bleDiscoverable ? 1 : 0);

  } else if (line.startsWith("BTRA")) {
    if (pServer)
      pServer->disconnect(0);  // force disconnect
    CommandSerial.print("OK00\r\n");

  } else if (line.startsWith("BTOW,")) {
    bool newState = line.endsWith("1");
    if (newState != bleEnabled) {
      bleEnabled = newState;
      if (bleEnabled) {

        //setupBLE();
        Serial.printf("Restarting ESP32 to recover\n");
        ESP.restart(); // Force reboot to fully reinit BLE
      } else {
        disableBLE();
      }
    }
    CommandSerial.print("OK00\r\n");

  } else if (line.startsWith("BTOR")) {
    CommandSerial.printf("OK00,%d\r\n", bleEnabled ? 1 : 0);

  } else if (line.startsWith("BTCR")) {
    CommandSerial.printf("OK00,%d\r\n", deviceConnected ? 1 : 0);

  } else {
    CommandSerial.printf("ER99\r\n");
  }
}

void setup() {
  Serial.begin(115200);
  MidiSerial.begin(500000, SERIAL_8N1, TEENSY_RX_1, TEENSY_TX_1);   // RX=18, TX=19
  CommandSerial.begin(9600, SERIAL_8N1, TEENSY_RX_2, TEENSY_TX_2);  // RX = GPIO16, TX = GPIO17
  delay(2000);  // Give serial time to initialize
  Serial.println("Paco 1\n");
  setupBLE();
  if (DEBUG)
    Serial.println("System initialized.");
}

int getMIDIMessageLength(uint8_t statusByte) {
  uint8_t type = statusByte & 0xF0;
  if (statusByte < 0x80) return 0;                         // invalid
  if (type == 0xC0 || type == 0xD0) return 2;              // Program change, channel pressure
  if (statusByte >= 0xF8) return 1;                        // Real-time
  if (statusByte == 0xF6 || statusByte == 0xF8) return 1;  // Tune request, timing clock
  if (statusByte == 0xF1 || statusByte == 0xF3) return 2;
  if (statusByte == 0xF2) return 3;
  return 3;  // Most channel messages are 3 bytes
}

void loop() {
  // First check if there's a string command (ending with \r\n)
  static String lineBuffer;
  while (CommandSerial.available()) {
    char c = CommandSerial.read();

    // Line-ending logic for ASCII commands
    if (c == '\n') {
      String line = lineBuffer;
      lineBuffer = "";
      handleCommand(line);
    } else if (c != '\r') {
      lineBuffer += c;
    }

    // Optional: size limit to avoid overflow
    if (lineBuffer.length() > 128) lineBuffer = "";
  }

  // Then, check for binary MIDI passthrough mode
  if (deviceConnected && bleEnabled) 
  {
    while (MidiSerial.available()) {
  uint8_t status = MidiSerial.peek();
  int len = getMIDIMessageLength(status);

  if (len <= 0 || len > 3) {
    // Invalid message, discard one byte and try again
    MidiSerial.read(); 
    continue;
  }

  if (MidiSerial.available() < len) break; // Wait for full message

  std::vector<uint8_t> msg;
  for (int i = 0; i < len; ++i) {
    msg.push_back(MidiSerial.read());
  }

  if (!msg.empty())
  {
    midiQueue.push_back(msg);
    if (DEBUG) {
  Serial.print("MIDI: ");
  for (auto b : msg) {
    Serial.printf("%02X ", b);
  }
  Serial.println();
}

  }

}



    // Time to flush a batch?
    if (millis() - lastMidiNotifyTime >= midiNotifyIntervalMs && !midiQueue.empty()) 
    {
      std::vector<uint8_t> packet;

      while (!midiQueue.empty() && (packet.size() + midiQueue.front().size()) <= bleMaxPayload) 
      {
        const auto& msg = midiQueue.front();
        packet.insert(packet.end(), msg.begin(), msg.end());
        midiQueue.pop_front();
      }

  if (!packet.empty()) 
      {
        uint32_t lastEvent = 0;
        uint32_t nowMs = millis();
        uint8_t timestamp = 0x80 | (nowMs & 0x7F); // MSB=1, lower 7 bits = time

        std::vector<uint8_t> blePacket;
        blePacket.push_back(timestamp); // header timestamp

        for (size_t i = 0; i < packet.size(); ) {
            uint8_t evtTimestamp = 0x80 | (millis() & 0x7F); // event timestamp
            blePacket.push_back(evtTimestamp);

            uint8_t status = packet[i];
            int len = getMIDIMessageLength(status);
            for (int j = 0; j < len && i < packet.size(); ++j, ++i) {
              blePacket.push_back(packet[i]);
            }
        }


        midiCharacteristic->setValue(blePacket.data(), blePacket.size());
        midiCharacteristic->notify();
        lastMidiNotifyTime = millis();

        if (DEBUG) {
          Serial.print("BLE Sent: ");
          for (auto b : blePacket) {
            Serial.printf("%02X ", b);
          }
          Serial.println();
        }
      }
    }
  }
}
