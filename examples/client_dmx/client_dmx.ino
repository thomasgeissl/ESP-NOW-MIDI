#include "enomik_client.h"
#include <SparkFunDMX.h>  //https://github.com/sparkfun/SparkFunDMX/

// on the dongle: run the print_mac firmware and paste it here
uint8_t peerMacAddress[6] = { 0x84, 0xF7, 0x03, 0xF2, 0x54, 0x62 };
enomik::Client _client;

SparkFunDMX dmx;
HardwareSerial dmxSerial(2);
uint8_t enPin = 21;
uint8_t numChannels = 512;

struct ChannelState {
  uint8_t msb = 0;
  uint8_t lsb = 0;
  bool msbReceived = false;
  bool lsbReceived = false;
  unsigned long lastUpdateTime = 0;
};

ChannelState channelStates[512];
const unsigned long PAIR_TIMEOUT = 20;  // ms - if both halves arrive within this window, combine them


// there has been a change in the callback signature with esp32 board version 3.3.0, hence this is here for backwards compatibility
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 3, 0)
void customOnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failure");
}
#else
void customOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failure");
}
#endif

void customOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Serial.print("Custom Callback - Status: ");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failure");
}

void onNoteOn(byte channel, byte note, byte velocity) {
  // Use first 4 MIDI channels for note control
  if (channel >= 1 && channel <= 4) {
    int dmxChannel = channel;         // MIDI channel 1-4 maps to DMX channel 1-4
    uint8_t dmxValue = velocity * 2;  // Scale 0-127 to 0-254
    dmx.writeByte(dmxChannel, dmxValue);
  }
}

void onNoteOff(byte channel, byte note, byte velocity) {}
void onControlChange(byte channel, byte control, byte value) {
  // Determine if MSB (CC 0-31) or LSB (CC 32-63)
  bool isMSB = (control < 32);
  int baseControl = isMSB ? control : (control - 32);
  int dmxChannel = (channel - 1) * 32 + baseControl;

  if (dmxChannel >= 0 && dmxChannel < numChannels) {
    unsigned long now = millis();

    // Check if we should reset state due to timeout
    if (now - channelStates[dmxChannel].lastUpdateTime > PAIR_TIMEOUT) {
      channelStates[dmxChannel].msbReceived = false;
      channelStates[dmxChannel].lsbReceived = false;
    }

    if (isMSB) {
      channelStates[dmxChannel].msb = value;
      channelStates[dmxChannel].msbReceived = true;
      channelStates[dmxChannel].lastUpdateTime = now;
    } else {
      channelStates[dmxChannel].lsb = value;
      channelStates[dmxChannel].lsbReceived = true;
      channelStates[dmxChannel].lastUpdateTime = now;
    }

    // If we have both MSB and LSB, update DMX
    if (channelStates[dmxChannel].msbReceived && channelStates[dmxChannel].lsbReceived) {
      // Combine into 14-bit value
      uint16_t fullValue = (channelStates[dmxChannel].msb << 7) | channelStates[dmxChannel].lsb;

      // Scale to 8-bit DMX (0-16383 → 0-255)
      uint8_t dmxValue = fullValue >> 6;

      // Write to DMX (channels are 1-indexed in SparkFunDMX)
      dmx.writeByte(dmxChannel + 1, dmxValue);

      // Clear flags for next pair
      channelStates[dmxChannel].msbReceived = false;
      channelStates[dmxChannel].lsbReceived = false;
    }
  }
}

void onProgramChange(byte channel, byte program) {
}

void onPitchBend(byte channel, int value) {}
void onAfterTouch(byte channel, byte value) {}
void onPolyAfterTouch(byte channel, byte note, byte value) {}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);


  _client.begin();
  _client.addPeer(peerMacAddress);

  _client.midi.setHandleNoteOn(onNoteOn);
  _client.midi.setHandleNoteOff(onNoteOff);
  _client.midi.setHandleControlChange(onControlChange);
  _client.midi.setHandleProgramChange(onProgramChange);
  _client.midi.setHandlePitchBend(onPitchBend);
  _client.midi.setHandleAfterTouchChannel(onAfterTouch);
  _client.midi.setHandleAfterTouchPoly(onPolyAfterTouch);

  dmx.begin(dmxSerial, enPin, numChannels);
  dmx.setComDir(DMX_WRITE_DIR);

  // register as a client by sending any message
  // this is needed in this case, as the client will stay unkown to the dongle until the first message is sent.
  _client.midi.sendControlChange(127, 127, 16);
}

void loop() {
  dmx.update();
}
