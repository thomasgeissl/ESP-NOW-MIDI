#include <esp_now.h>
#include <WiFi.h>
#include "esp_now_midi.h"


// on the dongle: run the print_mac firmware and paste it here
uint8_t peerMacAddress[6] = { 0xCC, 0x8D, 0xA2, 0x8B, 0x85, 0x1C };

esp_now_midi ESP_NOW_MIDI;

// there has been a change in the callback signature with esp32 board version 3.3.0, hence this is here for backwards compatibility
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 3, 0)
void customOnDataSent(const wifi_tx_info_t* info, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failure");
}
#else
void customOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failure");
}
#endif


void onStart() {
  esp_err_t result = ESP_NOW_MIDI.sendNoteOn(60, 127, 1);
  Serial.println("start");
}
void onStop() {
  esp_err_t result = ESP_NOW_MIDI.sendNoteOff(60, 0, 1);
  Serial.println("stop");
}
void onContinue() {
  esp_err_t result = ESP_NOW_MIDI.sendNoteOn(60, 127, 1);
  Serial.println("continue");
}

void onClock() {
  Serial.println("Clock");
}

void onSongPosition(int value) {
  esp_err_t result = ESP_NOW_MIDI.sendNoteOn(127, 127, 1);
  Serial.print("song position: ");
  Serial.print(value);
}

void onSongSelect(int value) {
  esp_err_t result = ESP_NOW_MIDI.sendNoteOn(126, 127, 1);
  Serial.print("song: ");
  Serial.print(value);
}

void onNoteOn(byte channel, byte note, byte velocity) {
  Serial.printf("Note On - Channel: %d, Note: %d, Velocity: %d\n", channel, note, velocity);
}

void setup() {
  Serial.begin(115200);
  delay(1000);  // Small delay to ensure Serial is ready
  Serial.println("Serial started");


  WiFi.mode(WIFI_STA);
  ESP_NOW_MIDI.setup(peerMacAddress, customOnDataSent);
  // ESP_NOW_MIDI.setup(destinationMacAddress); //or get rid of the custom send function and use the default one

  ESP_NOW_MIDI.setHandleStart(onStart);
  ESP_NOW_MIDI.setHandleStop(onStop);
  ESP_NOW_MIDI.setHandleContinue(onContinue);
  ESP_NOW_MIDI.setHandleClock(onClock);
  ESP_NOW_MIDI.setHandleNoteOn(onNoteOn);
  ESP_NOW_MIDI.setHandleSongPosition(onSongPosition);
  ESP_NOW_MIDI.setHandleSongSelect(onSongSelect);

  //send any midi message to register at the dongle
  delay(1000);
  esp_err_t result = ESP_NOW_MIDI.sendControlChange(127, 127, 16);
}

void loop() {
}