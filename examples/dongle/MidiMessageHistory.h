#pragma once
#include <esp_now_midi.h>

struct MidiMessageHistory {
  midi_message message;
  unsigned long timestamp;
  bool outgoing;
};