# ESP-NOW-MIDI

This is an Arduino library for sending MIDI messages via the ESP-NOW protocol.
A typical setup requires two ESP-NOW capable boards, where the receiver needs to be MIDI-capable and detectable by a computer or other MIDI-compatible hardware.

The ESP32-S2 Mini (Lolin S2 Mini) can act as both a receiver and a sender.
Any other ESP board with Wi-Fi capabilities should work as a sender.