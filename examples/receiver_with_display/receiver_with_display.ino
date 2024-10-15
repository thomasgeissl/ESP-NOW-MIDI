// https://randomnerdtutorials.com/esp-now-esp32-arduino-ide/
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include <esp_now_midi.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library.
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3D  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
unsigned long updateDisplayTimeStamp = 0;
const long updateDisplayInterval = 1000;


Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

midi_message message;
uint8_t baseMac[6];
String macStr;

#define MAX_HISTORY 10  // Maximum number of messages to store

struct MidiMessageHistory {
  midi_message message;
  unsigned long timestamp;  // Time the message was received
};

MidiMessageHistory messageHistory[MAX_HISTORY];  // Array to store the message history
int messageIndex = 0;                            // Index to keep track of the last message in the array

// Function to add a new message to the history
void addToHistory(const midi_message &msg) {
  messageHistory[messageIndex].message = msg;
  messageHistory[messageIndex].timestamp = millis();  // Store the current time
  messageIndex = (messageIndex + 1) % MAX_HISTORY;    // Circular buffer logic
}


void readMacAddress() {
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK) {
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                  baseMac[0], baseMac[1], baseMac[2],
                  baseMac[3], baseMac[4], baseMac[5]);
    macStr = String(baseMac[0], HEX) + ":" + String(baseMac[1], HEX) + ":" + String(baseMac[2], HEX) + ":" + String(baseMac[3], HEX) + ":" + String(baseMac[4], HEX) + ":" + String(baseMac[5], HEX);

  } else {
    Serial.println("Failed to read MAC address");
  }
}

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  memcpy(&message, incomingData, sizeof(message));
  addToHistory(message);

  auto status = message.status;
  auto channel = message.channel;
  switch (status) {
    case MIDI_NOTE_ON:
      {
        MIDI.sendNoteOn(message.firstByte, message.secondByte, channel);
        break;
      }
    case MIDI_NOTE_OFF:
      {
        MIDI.sendNoteOff(message.firstByte, message.secondByte, channel);
        break;
      }
    case MIDI_CONTROL_CHANGE:
      {
        MIDI.sendControlChange(message.firstByte, message.secondByte, channel);
        break;
      }
    case MIDI_PROGRAM_CHANGE:
      {
        MIDI.sendProgramChange(message.firstByte, channel);
        break;
      }
    case MIDI_AFTERTOUCH:
      {
        MIDI.sendAfterTouch(message.firstByte, channel);
        break;
      }
    case MIDI_POLY_AFTERTOUCH:
      {
        MIDI.sendAfterTouch(message.firstByte, message.secondByte, channel);
        break;
      }
    case MIDI_PITCH_BEND:
      {
        int pitchBendValue = (message.secondByte << 7) | message.firstByte;

        MIDI.sendPitchBend(pitchBendValue, channel);
        break;
      }
  }
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("status: ");
  Serial.println(message.status);
  Serial.print("first: ");
  Serial.println(message.firstByte);
  Serial.print("second: ");
  Serial.println(message.secondByte);
  Serial.println();
}

void updateDiplay() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(macStr);

  int yOffset = 10;  // Start rendering messages below the MAC address
  for (int i = 0; i < MAX_HISTORY; i++) {
    int index = (messageIndex + i) % MAX_HISTORY;
    
    // Only display non-empty messages
    if (messageHistory[index].timestamp > 0) {
      display.setCursor(0, yOffset);
      display.print("S:");   // Status
      display.print(messageHistory[index].message.status, HEX);
      display.print(" F:");
      display.print(messageHistory[index].message.firstByte);
      display.print(" S:");
      display.print(messageHistory[index].message.secondByte);
      
      yOffset += 8;  // Move to the next line (8 pixels down)
      
      // Check if there's space left on the display
      if (yOffset > SCREEN_HEIGHT - 8) {
        break;  // Prevent writing off the screen
      }
    }
  }

  display.display();
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  readMacAddress();

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  MIDI.begin(MIDI_CHANNEL_OMNI);



  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }
  display.clearDisplay();
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - updateDisplayTimeStamp >= updateDisplayInterval) {
    updateDisplayTimeStamp = currentMillis;
    updateDiplay();
  }
}