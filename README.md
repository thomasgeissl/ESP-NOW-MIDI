# ESP-NOW-MIDI

This is an Arduino library for sending MIDI messages via the ESP-NOW protocol.
A typical setup requires two ESP-NOW capable boards, where the receiver needs to be MIDI-capable and detectable by a computer or other MIDI-compatible hardware.

The ESP32-S2 Mini (Lolin S2 Mini) can act as both a receiver and a sender.
Any other ESP board with Wi-Fi capabilities should work as a sender.


## usage

1. Upload the provided receiver example to an ESP32-S2 Mini board. The MAC address will be printed during setup.
1. Copy the MAC address and paste it into the sender's setup arguments.
1. Integrate your sensor reading code into the sender.
1. Use the provided API to send MIDI messages, similar to the original MIDI library.

## current state
This has not yet been tested.
If you find any bugs feel free to submit an issue on github, also PRs are very welcome.