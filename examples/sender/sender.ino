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

void onNoteOn(byte channel, byte note, byte velocity) {
  Serial.printf("Note On - Channel: %d, Note: %d, Velocity: %d\n", channel, note, velocity);
}

void onNoteOff(byte channel, byte note, byte velocity) {
  Serial.printf("Note Off - Channel: %d, Note: %d, Velocity: %d\n", channel, note, velocity);
}

void onControlChange(byte channel, byte control, byte value) {
  Serial.printf("Control Change - Channel: %d, Control: %d, Value: %d\n", channel, control, value);
}

void onProgramChange(byte channel, byte program) {
  Serial.printf("Program Change - Channel: %d, Program: %d\n", channel, program);
}

void onPitchBend(byte channel, int value) {
  Serial.printf("Pitch Bend - Channel: %d, Value: %d\n", channel, value);
}
void onAfterTouch(byte channel, byte value) {
  Serial.printf("After Touch - Channel: %d, Value: %d\n", channel, value);
}
void onPolyAfterTouch(byte channel, byte note, byte value) {
  Serial.printf("Poly After Touch - Channel: %d, note: %d, Value: %d\n", channel, note, value);
}
void onStart() {
  Serial.printf("Start");
}
void onStop() {
  Serial.printf("Stop");
}
void onContinue() {
  Serial.printf("Continue");
}
void onClock() {
  Serial.printf("Clock");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  ESP_NOW_MIDI.setup(peerMacAddress, customOnDataSent);
  // ESP_NOW_MIDI.setup(destinationMacAddress); //or get rid of the custom send function and use the default one

  // all of these midi handlers are optional, depends on the usecase, very often you just wanna send data and not receive
  // e.g. this can be used for calibration, or maybe you wanna connect an amp via i2s and render some sound
  ESP_NOW_MIDI.setHandleNoteOn(onNoteOn);
  ESP_NOW_MIDI.setHandleNoteOff(onNoteOff);
  ESP_NOW_MIDI.setHandleControlChange(onControlChange);
  ESP_NOW_MIDI.setHandleProgramChange(onProgramChange);
  ESP_NOW_MIDI.setHandlePitchBend(onPitchBend);
  ESP_NOW_MIDI.setHandleAfterTouchChannel(onAfterTouch);
  ESP_NOW_MIDI.setHandleAfterTouchPoly(onPolyAfterTouch);
  ESP_NOW_MIDI.setHandleStart(onStart);
  ESP_NOW_MIDI.setHandleStop(onStop);
  ESP_NOW_MIDI.setHandleContinue(onContinue);
  ESP_NOW_MIDI.setHandleClock(onClock);
}

void loop() {
  esp_err_t result = ESP_NOW_MIDI.sendNoteOn(60, 127, 1);

  if (result != ESP_OK) {
    Serial.println("Error sending the data");
  }
  delay(100);
  result = ESP_NOW_MIDI.sendNoteOff(60, 0, 1);
  delay(100);
  result = ESP_NOW_MIDI.sendControlChange(1, 127, 1);
  delay(100);
  result = ESP_NOW_MIDI.sendControlChange(1, 0, 1);
  delay(100);
  result = ESP_NOW_MIDI.sendPitchBend(16383, 1);
  delay(100);
  result = ESP_NOW_MIDI.sendPitchBend(0, 1);

  delay(2000);
}