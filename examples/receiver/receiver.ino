//https://randomnerdtutorials.com/esp-now-esp32-arduino-ide/
#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include <esp_now_midi.h>

Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);



midi_message message;

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

  // MIDI.sendControlChange(ID*10 + 3, myData.touch * 127, ID); //touch is only a trigger 0 or 1
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
}

void loop() {
}