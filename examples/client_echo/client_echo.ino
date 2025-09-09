#include "enomik_client.h"


// on the dongle: run the print_mac firmware and paste it here
uint8_t peerMacAddress[6] = { 0x84, 0xF7, 0x03, 0xF2, 0x54, 0x62 };
enomik::Client _client;

void onNoteOn(byte channel, byte note, byte velocity) {
  _client.midi.sendNoteOn(note, velocity, channel);
}

void onNoteOff(byte channel, byte note, byte velocity) {
  _client.midi.sendNoteOff(note, velocity, channel);
}

void onControlChange(byte channel, byte control, byte value) {
  _client.midi.sendControlChange(control, value, channel);
}

void onProgramChange(byte channel, byte program) {
  _client.midi.sendProgramChange(program, channel);
}

void onPitchBend(byte channel, int value) {
  _client.midi.sendPitchBend(value, channel);
}
void onAfterTouch(byte channel, byte value) {
  _client.midi.sendAfterTouch(value, channel);
}
void onPolyAfterTouch(byte channel, byte note, byte value) {
  _client.midi.sendAfterTouch(note, value, channel);
}

void setup() {
  Serial.begin(115200);
  _client.begin();
  _client.addPeer(peerMacAddress);

  _client.midi.setHandleNoteOn(onNoteOn);
  _client.midi.setHandleNoteOff(onNoteOff);
  _client.midi.setHandleControlChange(onControlChange);
  _client.midi.setHandleProgramChange(onProgramChange);
  _client.midi.setHandlePitchBend(onPitchBend);
  _client.midi.setHandleAfterTouchChannel(onAfterTouch);
  _client.midi.setHandleAfterTouchPoly(onPolyAfterTouch);


  //register as a client by sending any message
  //this is needed in this case, as the client will stay unkown to the dongle until the first message is sent.
  _client.midi.sendControlChange(127, 127, 16);
}


void loop() {
}
