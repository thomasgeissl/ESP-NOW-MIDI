#include "config.h"
#include "enomik_client.h"

// on the dongle: run the print_mac firmware and paste it here
uint8_t peerMacAddress[6] = { 0xCC, 0x8D, 0xA2, 0x8B, 0x85, 0x1C };
enomik::Client _client;

void onNoteOn(byte channel, byte note, byte velocity) {
  Serial.printf("Note On - Channel: %d, Note: %d, Velocity: %d\n", channel, note, velocity);
  if (note >= 60 && note <= 65) {
    digitalWrite(_relayPins[note - 60], HIGH);
  }
}

void onNoteOff(byte channel, byte note, byte velocity) {
  Serial.printf("Note Off - Channel: %d, Note: %d, Velocity: %d\n", channel, note, velocity);
  if (note >= 60 && note <= 65) {
    digitalWrite(_relayPins[note - 60], LOW);
  }
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


void setup() {
  Serial.begin(115200);
  for (auto i = 0; i < 6; i++) {
    pinMode(_relayPins[i], OUTPUT);
  }

  _client.begin();
  _client.addPeer(peerMacAddress);

  _client.midi.setHandleNoteOn(onNoteOn);
  _client.midi.setHandleNoteOff(onNoteOff);
  _client.midi.setHandleControlChange(onControlChange);
  _client.midi.setHandleProgramChange(onProgramChange);
  _client.midi.setHandlePitchBend(onPitchBend);
  _client.midi.setHandleAfterTouchChannel(onAfterTouch);
  _client.midi.setHandleAfterTouchPoly(onPolyAfterTouch);

  //register at the dongle, by sending a message
  _client.midi.sendControlChange(127, 127, 16);
}

void loop() {
  _client.loop();
}