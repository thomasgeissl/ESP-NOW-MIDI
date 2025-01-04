#pragma once
#include <esp_now.h>
#include "./midiHelpers.h"

class esp_now_midi
{
public:
  // Typedef for the sent callback function signature
  typedef void (*DataSentCallback)(const uint8_t *mac_addr, esp_now_send_status_t status);

  // typedef void (*MidiCallback)(byte channel, byte firstByte, byte secondByte);

  // callback when data is sent (must be static for esp_now_register_send_cb)
  static void DefaultOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
  {
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  }
  static void OnDataRecvStatic(const uint8_t *mac, const uint8_t *incomingData, int len)
  {
    if (_instance)
    {                                                // Ensure the instance pointer is not null
      _instance->OnDataRecv(mac, incomingData, len); // Call the member function
    }
  }

  void setup(const uint8_t broadcastAddress[6], DataSentCallback callback = DefaultOnDataSent)
  {
    _instance = this;
    // Copy the broadcast address into the class member variable
    memcpy(_broadcastAddress, broadcastAddress, sizeof(_broadcastAddress));

    if (esp_now_init() != ESP_OK)
    {
      Serial.println("Error initializing ESP-NOW");
      return;
    }

    // Once ESPNow is successfully initialized, register Send callback to get the status of the transmitted packet
    esp_now_register_send_cb(callback);
    esp_now_midi::_instance = this;
    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecvStatic));

    // Register peer
    memcpy(_peerInfo.peer_addr, _broadcastAddress, sizeof(_broadcastAddress));
    _peerInfo.channel = 0;
    _peerInfo.encrypt = false;

    // Add peer
    if (esp_now_add_peer(&_peerInfo) != ESP_OK)
    {
      Serial.println("Failed to add peer");
      return;
    }
  }

  esp_err_t sendNoteOn(byte note, byte velocity, byte channel)
  {
    midi_message message;
    message.channel = channel;
    message.status = MIDI_NOTE_ON;
    message.firstByte = note;
    message.secondByte = velocity;
    return esp_now_send(_broadcastAddress, (uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendNoteOff(byte note, byte velocity, byte channel)
  {
    midi_message message;
    message.channel = channel;
    message.status = MIDI_NOTE_OFF;
    message.firstByte = note;
    message.secondByte = velocity;
    return esp_now_send(_broadcastAddress, (uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendControlChange(byte control, byte value, byte channel)
  {
    midi_message message;
    message.channel = channel;
    message.status = MIDI_CONTROL_CHANGE;
    message.firstByte = control;
    message.secondByte = value;
    return esp_now_send(_broadcastAddress, (uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendProgramChange(byte program, byte channel)
  {
    midi_message message;
    message.channel = channel;
    message.status = MIDI_PROGRAM_CHANGE;
    message.firstByte = program;
    return esp_now_send(_broadcastAddress, (uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendAfterTouch(byte pressure, byte channel)
  {
    midi_message message;
    message.channel = channel;
    message.status = MIDI_AFTERTOUCH;
    message.firstByte = pressure;
    return esp_now_send(_broadcastAddress, (uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendAfterTouch(byte note, byte pressure, byte channel)
  {
    midi_message message;
    message.channel = channel;
    message.status = MIDI_POLY_AFTERTOUCH;
    message.firstByte = note;
    message.secondByte = pressure;
    return esp_now_send(_broadcastAddress, (uint8_t *)&message, sizeof(message));
  }
  // for those using the teensy library
  esp_err_t sendAfterTouchPoly(byte note, byte pressure, byte channel)
  {
    return sendAfterTouch(note, pressure, channel);
  }

  esp_err_t sendPitchBend(uint16_t value, byte channel)
  {
    midi_message message;
    message.channel = channel;
    message.status = MIDI_PITCH_BEND;

    // Ensure value is within the valid 14-bit range (0 - 16383)
    value = value & 0x3FFF; // Mask to ensure it's 14 bits (0x3FFF = 16383 in decimal)

    // Split the 14-bit value into LSB and MSB (each 7 bits)
    message.firstByte = value & 0x7F;         // LSB: lower 7 bits of the pitch bend value
    message.secondByte = (value >> 7) & 0x7F; // MSB: upper 7 bits of the pitch bend value

    return esp_now_send(_broadcastAddress, (uint8_t *)&message, sizeof(message));
  }

  void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
  {
    midi_message message;
    memcpy(&message, incomingData, sizeof(message));

    auto status = message.status;
    auto channel = message.channel;
    Serial.println("got message");

    switch (message.status)
    {
    case MIDI_NOTE_ON:
      if (onNoteOnHandler)
        onNoteOnHandler(message.channel, message.firstByte, message.secondByte);
      break;
    case MIDI_NOTE_OFF:
      if (onNoteOffHandler)
        onNoteOffHandler(message.channel, message.firstByte, message.secondByte);
      break;
    case MIDI_CONTROL_CHANGE:
      if (onControlChangeHandler)
        onControlChangeHandler(message.channel, message.firstByte, message.secondByte);
      break;
    case MIDI_PROGRAM_CHANGE:
      if (onProgramChangeHandler)
        onProgramChangeHandler(message.channel, message.firstByte);
      break;
    case MIDI_AFTERTOUCH:
      if (onAfterTouchChannelHandler)
        onAfterTouchChannelHandler(message.channel, message.firstByte);
      break;
    case MIDI_POLY_AFTERTOUCH:
      if (onAfterTouchPolyHandler)
        onAfterTouchPolyHandler(message.channel, message.firstByte, message.secondByte);
      break;
    case MIDI_PITCH_BEND:
      int pitchBendValue = (message.secondByte << 7) | message.firstByte;
      if (onPitchBendHandler)
        onPitchBendHandler(message.channel, pitchBendValue);
      break;
    }
  }
  void setHandleNoteOn(void (*callback)(byte channel, byte note, byte velocity))
  {
    onNoteOnHandler = callback;
  }

  void setHandleNoteOff(void (*callback)(byte channel, byte note, byte velocity))
  {
    onNoteOffHandler = callback;
  }

  void setHandleControlChange(void (*callback)(byte channel, byte control, byte value))
  {
    onControlChangeHandler = callback;
  }

  void setHandleProgramChange(void (*callback)(byte channel, byte program))
  {
    onProgramChangeHandler = callback;
  }

  void setHandlePitchBend(void (*callback)(byte channel, int value))
  {
    onPitchBendHandler = callback;
  }

  void setHandleAfterTouchChannel(void (*callback)(byte channel, byte pressure))
  {
    onAfterTouchChannelHandler = callback;
  }

  void setHandleAfterTouchPoly(void (*callback)(byte channel, byte note, byte pressure))
  {
    onAfterTouchPolyHandler = callback;
  }

private:
  uint8_t _broadcastAddress[6];
  esp_now_peer_info_t _peerInfo;
  static esp_now_midi *_instance; // Static pointer to hold the instance

  // MIDI Handlers
  void (*onNoteOnHandler)(byte channel, byte note, byte velocity) = nullptr;
  void (*onNoteOffHandler)(byte channel, byte note, byte velocity) = nullptr;
  void (*onControlChangeHandler)(byte channel, byte control, byte value) = nullptr;
  void (*onProgramChangeHandler)(byte channel, byte program) = nullptr;
  void (*onPitchBendHandler)(byte channel, int value) = nullptr;
  void (*onAfterTouchChannelHandler)(byte channel, byte value) = nullptr;
  void (*onAfterTouchPolyHandler)(byte channel, byte note, byte value) = nullptr;
};

esp_now_midi* esp_now_midi::_instance = nullptr;
