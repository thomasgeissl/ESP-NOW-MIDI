# ESP-NOW-MIDI
This is an Arduino library for sending and receiving MIDI messages via the ESP-NOW protocol.
A typical setup requires two ESP-NOW capable boards, where the board connected to your computer needs to be MIDI-capable 

The ESP32-S2 Mini (Lolin S2 Mini) can act as both a receiver and a sender. An S3 should also work as a receiver.
Any ESP board with Wi-Fi capabilities should work as a sender.

## use it basically everywhere
* the dongle shows up as a class compliant midi controller
* use it with MAX, pd, any DAW, processing, openFrameworks, any game engine that supports MIDI, ... or even in the browser or mobile phone

## Features
* **enomik::Client I/O** MIDI sysex configuratable client: zero programming for basic I/O, e.g. digital input 3 -> MIDI CC
* **enomik::Client MIDI wrapper** Helper that takes care of the ESP-NOW setup, provides a common interface for USB and ESP-NOW MIDI
* **examples**
  * **print_mac**: periodically prints the mac address to the serial monitor
  * **dongle**: this is your esp now midi interface to your computer or any other usb midi host. it converts esp now message to midi messages, requires a midi capable board, e.g. esp32 s2 mini.
  the config.h you can disable the display - in case you are not using one
  if you wanna put them into a case, you can probably find 3d models online, here is one for an esp32 s2 mini: https://www.thingiverse.com/thing:5427531
  * **client_hello_midi**: simple demo firmware that periodically sends midi messages via esp now
  * **client (wip)**: fully configurable client that includes already a couple of common sensors and exposes touch, digital and analog pins
  * mpu6050 (gy521) - accelerometer, gyro, temperature
  * vl53l0x - time of flight distance sensor
  * **client_dac_i2s (wip)** - synth that can be controlled via dongle, e.g. send midi notes from a DAW to the dongle midi device - ** this is currently broken **
  * **client_waveshare-esp32-s3-relay-6ch** - simple relay controller that listens to note on/off messages, e.g. control solenoids 
  * **client_buttons** - reads button press/release and sends note on/off accordingly
  * **client_dmx** - control your dmx fixtures wirelessly
    * CC MSB/LSB mapping
      * MIDI Ch 1, CC 0 (MSB) + CC 32 (LSB) -> DMX Channel 1
      * MIDI Ch 1, CC 1 (MSB) + CC 33 (LSB) -> DMX Channel 2
      * MIDI Ch 1, CC 31 (MSB) + CC 63 (LSB) -> DMX Channel 32
      * MIDI Ch 2, CC 0 (MSB) + CC 32 (LSB) -> DMX Channel 33
      * MIDI Ch 16, CC 31 (MSB) + CC 63 (LSB) -> DMX Channel 512
    * NOTEON direct mapping
      * MIDI Ch 1, NOTE 0, VEL: 127 -> DMX Channel 1, value: 127*2
      * MIDI Ch 4, NOTE 127, VEL: 127 -> DMX Channel 512, value: 127*2

## breaking changes
ESP32 Boards version 3.3.0 require library version 0.6.0 or higher for compatibility.
Please note that ESP-NOW function signatures have changed in this version.


## usage
1. Upload the print_mac example to an ESP32-S2 Mini board. The MAC address will be printed to via serial, or if you have a display connect to the dongle, you can skip this step, as the mac address will be printed on the display.
1. Upload the dongle example to an ESP32-S2 Mini board.
1. Copy the MAC address and paste it into the sender's setup arguments.
1. Integrate your sensor reading code into the sender.
1. Use the provided API to send MIDI messages, similar to the original MIDI library.

if you are planning to send midi messages from your computer to your other esp32 micro controller, the dongle firmware will forward midi messages received via usb midi, the sender first needs to register its mac address by sending any midi message to the dongle via esp now midi.

if you are using a display then make sure you have set `HAS_DISPLAY 1` in config.h 

## sysex interface
1. start: 0xF0
1. manufacturer id: 0x7D
1. command id: 0x01 (set pin config), 0x09 (reset)
1. pin: any valid pin number
1. pin_mode: 0x00 (INPUT), 0x01 (OUTPUT), 0x02 (INPUT_PULLUP), 0x03(ANALOG_INPUT), 0x04(ANALOG_OUTPUT)
1. midi channel: 1-16
1. midi type: midi status byte divided by 2, e.g. CC (0xB0, 176) => (0x58, 88)
1. end: 0xF7

* e.g. reset and clear pin configs: `0xF0, 0x7D, 0x09, 0xF7`
* e.g. configure pin 3 to read digital values and send out CC: `0xF0, 0x7D, 0x01, 0x03, 0x02, 0x58, 0xF7`


## dependencies
* dependencies for the library should be automatically installed
* examples/dongle additionally depends on
  * Adafruit GFX Library
  * Adafruit SSD1306
* examples/client depends on
  * Adafruit MPU6050
  * Adafruit VL53L0x
* examples/client_dac_i2s depends on mozzi
* examples/client_dmx depends on SparkfunDMX 

## contributing
If you find any bugs feel free to submit an issue on github, also PRs are very welcome.
