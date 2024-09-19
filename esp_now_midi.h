#pragma once
#include <esp_now.h>
#include "./midiHelpers.h"

class esp_now_midi {
public:
  // callback when data is sent (must be static for esp_now_register_send_cb)
  static void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  }

  void setup(const uint8_t broadcastAddress[6]) {
    // Copy the broadcast address into the class member variable
    memcpy(_broadcastAddress, broadcastAddress, sizeof(_broadcastAddress));

    if (esp_now_init() != ESP_OK) {
      Serial.println("Error initializing ESP-NOW");
      return;
    }

    // Once ESPNow is successfully initialized, register Send callback to get the status of the transmitted packet
    esp_now_register_send_cb(OnDataSent);

    // Register peer
    memcpy(_peerInfo.peer_addr, _broadcastAddress, sizeof(_broadcastAddress));
    _peerInfo.channel = 0;
    _peerInfo.encrypt = false;

    // Add peer
    if (esp_now_add_peer(&_peerInfo) != ESP_OK) {
      Serial.println("Failed to add peer");
      return;
    }
  }

  esp_err_t sendNoteOn(byte note, byte velocity, byte channel) {
    midi_message message;
    message.channel = channel;
    message.status = MIDI_NOTE_ON;
    message.firstByte = note;
    message.secondByte = velocity;
    return esp_now_send(_broadcastAddress, (uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendNoteOff(byte note, byte velocity, byte channel) {
    midi_message myData;
    myData.channel = channel;
    myData.status = MIDI_NOTE_OFF;
    myData.firstByte = note;
    myData.secondByte = velocity;
    return esp_now_send(_broadcastAddress, (uint8_t *)&myData, sizeof(myData));
  }

  esp_err_t sendControlChange(byte control, byte value, byte channel) {
    midi_message myData;
    myData.channel = channel;
    myData.status = MIDI_CONTROL_CHANGE;
    myData.firstByte = control;
    myData.secondByte = value;
    return esp_now_send(_broadcastAddress, (uint8_t *)&myData, sizeof(myData));
  }

  esp_err_t sendProgramChange(byte program, byte channel) {
    midi_message myData;
    myData.channel = channel;
    myData.status = MIDI_PROGRAM_CHANGE;
    myData.firstByte = program;
    return esp_now_send(_broadcastAddress, (uint8_t *)&myData, sizeof(myData));
  }

  esp_err_t sendAfterTouch(byte pressure, byte channel) {
    midi_message myData;
    myData.channel = channel;
    myData.status = MIDI_AFTERTOUCH;
    myData.firstByte = pressure;
    return esp_now_send(_broadcastAddress, (uint8_t *)&myData, sizeof(myData));
  }

  esp_err_t sendPitchBend(byte value, byte channel) {
    midi_message myData;
    myData.channel = channel;
    myData.status = MIDI_PITCH_BEND;
    // TODO
    myData.firstByte = value;  // Typically, this would be a 14-bit value, but simplified here
    return esp_now_send(_broadcastAddress, (uint8_t *)&myData, sizeof(myData));
  }

private:
  uint8_t _broadcastAddress[6];
  esp_now_peer_info_t _peerInfo;
};

