#include "enomik_client.h"


// on the dongle: run the print_mac firmware and paste it here
uint8_t peerMacAddress[6] = { 0x84, 0xF7, 0x03, 0xF2, 0x54, 0x62 };
enomik::Client _client;


void onStart() {
  esp_err_t result = _client.midi.sendNoteOn(60, 127, 1);
  Serial.println("start");
}
void onStop() {
  esp_err_t result = _client.midi.sendNoteOff(60, 0, 1);
  Serial.println("stop");
}
void onContinue() {
  esp_err_t result = _client.midi.sendNoteOn(60, 127, 1);
  Serial.println("continue");
}

void onClock() {
  Serial.println("Clock");
}

void onSongPosition(int value) {
  esp_err_t result = _client.midi.sendNoteOn(127, 127, 1);
  Serial.print("song position: ");
  Serial.print(value);
}

void onSongSelect(int value) {
  esp_err_t result = _client.midi.sendNoteOn(126, 127, 1);
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


  _client.begin();
  _client.addPeer(peerMacAddress);

  _client.midi.setHandleStart(onStart);
  _client.midi.setHandleStop(onStop);
  _client.midi.setHandleContinue(onContinue);
  _client.midi.setHandleClock(onClock);
  _client.midi.setHandleNoteOn(onNoteOn);
  _client.midi.setHandleSongPosition(onSongPosition);
  _client.midi.setHandleSongSelect(onSongSelect);

  //send any midi message to register at the dongle
  delay(1000);
  esp_err_t result = _client.midi.sendControlChange(127, 127, 16);
}

void loop() {
  _client.loop();
}