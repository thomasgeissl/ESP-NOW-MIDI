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