#pragma once
#include "midiHelpers.h"

struct MidiMessageHistory {
  midi_message message;
  unsigned long timestamp;
  bool outgoing;
};