#include <esp_now.h>
#include <WiFi.h>
#include "esp_now_midi.h"


// run the print_mac firmware and adjust the mac address
uint8_t broadcastAddress[6] = { 0xCC, 0x8D, 0xA2, 0x8B, 0x85, 0x1C };

esp_now_midi ESP_NOW_MIDI;
void customOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Serial.print("Custom Callback - Status: ");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failure");
}


void onStart() {
  esp_err_t result = ESP_NOW_MIDI.sendNoteOn(60, 127, 1);
  Serial.println("start");
}
void onStop() {
  esp_err_t result = ESP_NOW_MIDI.sendNoteOn(60, 0, 1);
  Serial.println("stop");
}
void onContinue() {
  esp_err_t result = ESP_NOW_MIDI.sendNoteOn(60, 127, 1);
  Serial.println("continue");
}

void onClock() {
  Serial.printf("Clock");
}

void onNoteOn(byte channel, byte note, byte velocity) {
  Serial.printf("Note On - Channel: %d, Note: %d, Velocity: %d\n", channel, note, velocity);
}

void setup() {
  Serial.begin(115200);
  delay(1000);  // Small delay to ensure Serial is ready
  Serial.println("Serial started");


  WiFi.mode(WIFI_STA);
  ESP_NOW_MIDI.setup(broadcastAddress, customOnDataSent);
  // ESP_NOW_MIDI.setup(broadcastAddress); //or get rid of the custom send function and use the default one

  ESP_NOW_MIDI.setHandleStart(onStart);
  ESP_NOW_MIDI.setHandleStop(onStop);
  ESP_NOW_MIDI.setHandleContinue(onContinue);
  ESP_NOW_MIDI.setHandleClock(onClock);
  ESP_NOW_MIDI.setHandleNoteOn(onNoteOn);

  //send any midi message to register at the dongle
  delay(1000);
  esp_err_t result = ESP_NOW_MIDI.sendControlChange(127, 127, 16);
}

void loop() {
}