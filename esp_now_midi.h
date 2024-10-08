#pragma once
#include <esp_now.h>
#include "./midiHelpers.h"

class esp_now_midi {
public:
  // Typedef for the callback function signature
  typedef void (*DataSentCallback)(const uint8_t *mac_addr, esp_now_send_status_t status);

  // callback when data is sent (must be static for esp_now_register_send_cb)
  static void DefaultOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  }

  void setup(const uint8_t broadcastAddress[6], DataSentCallback callback = DefaultOnDataSent) {
    // Copy the broadcast address into the class member variable
    memcpy(_broadcastAddress, broadcastAddress, sizeof(_broadcastAddress));

    if (esp_now_init() != ESP_OK) {
      Serial.println("Error initializing ESP-NOW");
      return;
    }

    // Once ESPNow is successfully initialized, register Send callback to get the status of the transmitted packet
    esp_now_register_send_cb(callback);

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
    midi_message message;
    message.channel = channel;
    message.status = MIDI_NOTE_OFF;
    message.firstByte = note;
    message.secondByte = velocity;
    return esp_now_send(_broadcastAddress, (uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendControlChange(byte control, byte value, byte channel) {
    midi_message message;
    message.channel = channel;
    message.status = MIDI_CONTROL_CHANGE;
    message.firstByte = control;
    message.secondByte = value;
    return esp_now_send(_broadcastAddress, (uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendProgramChange(byte program, byte channel) {
    midi_message message;
    message.channel = channel;
    message.status = MIDI_PROGRAM_CHANGE;
    message.firstByte = program;
    return esp_now_send(_broadcastAddress, (uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendAfterTouch(byte pressure, byte channel) {
    midi_message message;
    message.channel = channel;
    message.status = MIDI_AFTERTOUCH;
    message.firstByte = pressure;
    return esp_now_send(_broadcastAddress, (uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendAfterTouch(byte note, byte pressure, byte channel) {
    midi_message message;
    message.channel = channel;
    message.status = MIDI_POLY_AFTERTOUCH;
    message.firstByte = note;
    message.secondByte = pressure;
    return esp_now_send(_broadcastAddress, (uint8_t *)&message, sizeof(message));
  }
  // for those using the teensy library
  esp_err_t sendAfterTouchPoly(byte note, byte pressure, byte channel) {
    return sendAfterTouch(note, pressure, channel);
  }

  esp_err_t sendPitchBend(uint16_t value, byte channel) {
    midi_message message;
    message.channel = channel;
    message.status = MIDI_PITCH_BEND;
    
    // Ensure value is within the valid 14-bit range (0 - 16383)
    value = value & 0x3FFF; // Mask to ensure it's 14 bits (0x3FFF = 16383 in decimal)

    // Split the 14-bit value into LSB and MSB (each 7 bits)
    message.firstByte = value & 0x7F;        // LSB: lower 7 bits of the pitch bend value
    message.secondByte = (value >> 7) & 0x7F; // MSB: upper 7 bits of the pitch bend value

    return esp_now_send(_broadcastAddress, (uint8_t *)&message, sizeof(message));
}

private:
  uint8_t _broadcastAddress[6];
  esp_now_peer_info_t _peerInfo;
};
