# ESP-NOW-MIDI
This is an Arduino library for sending and receiving MIDI messages via the ESP-NOW protocol.
A typical setup requires two ESP-NOW capable boards, where the board connected to your computer needs to be MIDI-capable 

The ESP32-S2 Mini (Lolin S2 Mini) can act as both a receiver and a sender. An S3 should also work as a receiver.
Any ESP board with Wi-Fi capabilities should work as a sender.

## use it basically everywhere
* the dongle shows up as a class compliant midi controller
* use it with MAX, pd, any DAW, processing, openFrameworks, any game engine that supports MIDI, ... or even in the browser

## usage
1. Upload the print_mac example to an ESP32-S2 Mini board. The MAC address will be printed to via serial, or if you have a display connect to the dongle, you can skip this step, as the mac address will be printed on the display.
1. Upload the dongle example to an ESP32-S2 Mini board.
1. Copy the MAC address and paste it into the sender's setup arguments.
1. Integrate your sensor reading code into the sender.
1. Use the provided API to send MIDI messages, similar to the original MIDI library.

if you are planning to send midi messages from your computer to your other esp32 micro controller, the dongle firmware will forward midi messages received via usb midi, the sender first needs to register its mac address by sending any midi message to the dongle via esp now midi.

if you are using a display then make sure you have set `HAS_DISPLAY 1` in config.h 

## circuit python
There is also a circuit python version of this library, only for the sender at the moment. Please be aware that it is not yet tested. Feedback is very welcome.

## examples
* **print_mac**: periodically prints the mac address to the serial monitor
* **dongle**: this is your esp now midi interface to your computer or any other usb midi host. it converts esp now message to midi messages, requires a midi capable board, e.g. esp32 s2 mini.
the config.h you can disable the display - in case you are not using one
if you wanna put them into a case, you can probably find 3d models online, here is one for an esp32 s2 mini: https://www.thingiverse.com/thing:5427531
* **sender**: simple demo firmware that periodically sends midi messages via esp now
* **client (wip)**: fully configurable client that includes already a couple of common sensors and exposes touch, digital and analog pins
  * mpu6050 (gy521) - accelerometer, gyro, temperature
  * vl53l0x - time of flight distance sensor
* **client_dac_i2s (wip)** - synth that can be controlled via dongle, e.g. send midi notes from a DAW to the dongle midi device
* **client_waveshare-esp32-s3-relay-6ch** - simple relay controller that listens to note on/off messages, e.g. control solenoids 
* **client_buttons** - reads button press/release and sends note on/off accordingly

## dependencies
* dependencies for the library should be automatically installed
* examples/dongle additionally depends on
  * Adafruit GFX Library
  * Adafruit SSD1306
* examples/client depends on
  * Adafruit MPU6050
  * Adafruit VL53L0x
* examples/client_dac_i2s depends on mozzi

## contributing
If you find any bugs feel free to submit an issue on github, also PRs are very welcome.
