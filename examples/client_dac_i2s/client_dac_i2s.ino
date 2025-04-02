#define MOZZI_OUTPUT_I2S_DAC  // Enable I2S output for MAX98357A
#include <esp_now.h>
#include <WiFi.h>
#include "esp_now_midi.h"
#include <MozziGuts.h>
#include <Oscil.h>
#include <tables/sin2048_int8.h>
#include <ADSR.h>

uint8_t broadcastAddress[6] = { 0xCC, 0x8D, 0xA2, 0x8B, 0x85, 0x1C };

esp_now_midi ESP_NOW_MIDI;
void customOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Optional: Add any send status handling if needed
}

Oscil<2048, AUDIO_RATE> aSin(SIN2048_DATA);

ADSR<CONTROL_RATE, AUDIO_RATE> envelope;

byte currentNote = 0;
float currentFrequency = 0.0f;

void onNoteOn(byte channel, byte note, byte velocity) {
  Serial.printf("Note On - Channel: %d, Note: %d, Velocity: %d\n", channel, note, velocity);

  currentNote = note;
  currentFrequency = mtof(note);

  aSin.setFreq(currentFrequency);
  envelope.noteOn();
}

void onNoteOff(byte channel, byte note, byte velocity) {
  Serial.printf("Note Off - Channel: %d, Note: %d, Velocity: %d\n", channel, note, velocity);

  if (note == currentNote) {
    envelope.noteOff();
  }
}

float mtof(byte note) {
  return 440.0f * pow(2.0f, (note - 69) / 12.0f);
}

void setup() {
  WiFi.mode(WIFI_STA);
  ESP_NOW_MIDI.setup(broadcastAddress, customOnDataSent);

  ESP_NOW_MIDI.setHandleNoteOn(onNoteOn);
  ESP_NOW_MIDI.setHandleNoteOff(onNoteOff);  // Fixed: Changed from setHandleNoteOn to setHandleNoteOff

  envelope.setADLevels(255, 64);         // Attack/decay levels (max, sustain)
  envelope.setTimes(50, 200, 100, 200);  // Attack, Decay, Sustain, Release times

  startMozzi();

  // register at the dongle
  delay(1000);
  esp_err_t result = ESP_NOW_MIDI.sendControlChange(127, 127, 16);
}

void updateControl() {
  envelope.update();
}

int updateAudio() {
  return (int)aSin.next() * envelope.next() >> 8;
}

void loop() {
  audioHook();
}