#pragma once
#define MAX_PEERS 20
#include "./version.h"
#include <esp_now.h>
#include <esp_wifi.h> // Needed for wifi_tx_info_t in newer versions
#include "./midiHelpers.h"

// Version detection
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 3, 0)
#define ESP_NOW_NEW_CALLBACK_SIGNATURE 1
#endif

class esp_now_midi
{
public:
// Conditional callback typedef
#ifdef ESP_NOW_NEW_CALLBACK_SIGNATURE
  typedef void (*DataSentCallback)(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status);
#else
  typedef void (*DataSentCallback)(const uint8_t *mac_addr, esp_now_send_status_t status);
#endif

// Static callback adapter
#ifdef ESP_NOW_NEW_CALLBACK_SIGNATURE
  static void DefaultOnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
  {
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
    // Note: MAC address not directly available in wifi_tx_info_t structure
  }
#else
  static void DefaultOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
  {
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  }
#endif

// Adapter function to handle both versions
#ifdef ESP_NOW_NEW_CALLBACK_SIGNATURE
  static void SendCallbackAdapter(const wifi_tx_info_t *info, esp_now_send_status_t status)
  {
    if (_instance && _instance->userDataSentCallback)
    {
      _instance->userDataSentCallback(info, status);
    }
  }
#else
  static void SendCallbackAdapter(const uint8_t *mac_addr, esp_now_send_status_t status)
  {
    if (_instance && _instance->userDataSentCallback)
    {
      _instance->userDataSentCallback(mac_addr, status);
    }
  }
#endif

// Fixed: Updated receive callback signature for new version
#ifdef ESP_NOW_NEW_CALLBACK_SIGNATURE
  static void OnDataRecvStatic(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len)
  {
    if (_instance)
    {
      _instance->OnDataRecv(recv_info->src_addr, incomingData, len);
    }
  }
#else
  static void OnDataRecvStatic(const uint8_t *mac, const uint8_t *incomingData, int len)
  {
    if (_instance)
    {
      _instance->OnDataRecv(mac, incomingData, len);
    }
  }
#endif

  void setup(DataSentCallback callback = DefaultOnDataSent)
  {
    _instance = this;
    userDataSentCallback = callback;

    // Initialize peers array
    _peersCount = 0;

    // Initialize ESP-NOW FIRST
    if (esp_now_init() != ESP_OK)
    {
      Serial.println("Error initializing ESP-NOW");
      return;
    }

    // Register callbacks AFTER initialization
    esp_now_register_send_cb(SendCallbackAdapter);

// Fixed: Cast based on version
#ifdef ESP_NOW_NEW_CALLBACK_SIGNATURE
    esp_now_register_recv_cb(OnDataRecvStatic);
#else
    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecvStatic));
#endif
  }

  // Add a new peer
  bool addPeer(const uint8_t macAddress[6])
  {
    if (_peersCount >= MAX_PEERS)
    {
      Serial.println("Maximum number of peers reached");
      return false;
    }

    // Debug print
    Serial.print("Adding peer: ");
    for (int i = 0; i < 6; i++)
    {
      Serial.print(macAddress[i], HEX);
      if (i < 5)
        Serial.print(":");
    }
    Serial.println();

    // Create the peer info structure
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, macAddress, 6); // Always use exact size (6 bytes)
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    // Add the peer to ESP-NOW
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
      Serial.println("Failed to add peer");
      return false;
    }

    // Store the peer in our array AFTER successful ESP-NOW registration
    memcpy(_peers[_peersCount], macAddress, 6);
    _peersCount++;
    Serial.print("Peer added successfully. Total peers: ");
    Serial.println(_peersCount);
    return true;
  }

  // Send to all peers
  esp_err_t sendToAllPeers(const uint8_t *data, size_t len)
  {
    esp_err_t result = ESP_OK;

    if (_peersCount == 0)
    {
      Serial.println("No peers registered!");
      return ESP_FAIL;
    }

    for (int i = 0; i < _peersCount; i++)
    {
      esp_err_t err = esp_now_send(_peers[i], data, len);
      if (err != ESP_OK)
      {
        result = err; // Return last error if any
      }
    }
    return result;
  }

  esp_err_t sendNoteOn(byte note, byte velocity, byte channel)
  {
    midi_message message;
    message.channel = channel;
    message.status = MIDI_NOTE_ON;
    message.firstByte = note;
    message.secondByte = velocity;
    return sendToAllPeers((uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendNoteOff(byte note, byte velocity, byte channel)
  {
    midi_message message;
    message.channel = channel;
    message.status = MIDI_NOTE_OFF;
    message.firstByte = note;
    message.secondByte = velocity;
    return sendToAllPeers((uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendControlChange(byte control, byte value, byte channel)
  {
    midi_message message;
    message.channel = channel;
    message.status = MIDI_CONTROL_CHANGE;
    message.firstByte = control;
    message.secondByte = value;
    return sendToAllPeers((uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendProgramChange(byte program, byte channel)
  {
    midi_message message;
    message.channel = channel;
    message.status = MIDI_PROGRAM_CHANGE;
    message.firstByte = program;
    return sendToAllPeers((uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendAfterTouch(byte pressure, byte channel)
  {
    midi_message message;
    message.channel = channel;
    message.status = MIDI_AFTERTOUCH;
    message.firstByte = pressure;
    return sendToAllPeers((uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendAfterTouch(byte note, byte pressure, byte channel)
  {
    midi_message message;
    message.channel = channel;
    message.status = MIDI_POLY_AFTERTOUCH;
    message.firstByte = note;
    message.secondByte = pressure;
    return sendToAllPeers((uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendAfterTouchPoly(byte note, byte pressure, byte channel)
  {
    return sendAfterTouch(note, pressure, channel);
  }

  esp_err_t sendPitchBendRaw(int value, byte channel)
  { // uint16_t
    midi_message message;
    message.channel = channel;
    message.status = MIDI_PITCH_BEND;
    value = value & 0x3FFF;
    message.firstByte = value & 0x7F;
    message.secondByte = (value >> 7) & 0x7F;
    return sendToAllPeers((uint8_t *)&message, sizeof(message));
  }
  esp_err_t sendPitchBend(int16_t value, byte channel)
  {
    // clamp to signed 14-bit range
    if (value < -8192)
      value = -8192;
    if (value > 8191)
      value = 8191;

    // translate signed (-8192..8191) -> unsigned (0..16383)
    uint16_t raw = value + 8192;
    return sendPitchBendRaw(raw, channel);
  }

  esp_err_t sendStart()
  {
    midi_message message;
    message.status = MIDI_START;
    return sendToAllPeers((uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendStop()
  {
    midi_message message;
    message.status = MIDI_STOP;
    return sendToAllPeers((uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendContinue()
  {
    midi_message message;
    message.status = MIDI_CONTINUE;
    return sendToAllPeers((uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendClock()
  {
    midi_message message;
    message.status = MIDI_TIME_CLOCK;
    return sendToAllPeers((uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendSongPosition(uint16_t value)
  {
    midi_message message;
    message.status = MIDI_SONG_POS_POINTER;
    value = value & 0x3FFF;
    message.firstByte = value & 0x7F;
    message.secondByte = (value >> 7) & 0x7F;
    return sendToAllPeers((uint8_t *)&message, sizeof(message));
  }

  esp_err_t sendSongSelect(uint8_t value)
  {
    midi_message message;
    message.status = MIDI_SONG_SELECT;
    value = value & 0x7F;
    message.firstByte = value;
    return sendToAllPeers((uint8_t *)&message, sizeof(message));
  }
  esp_err_t sendSysex(uint8_t data[128], uint8_t length)
  {
    midi_sysex_message sysexMessage;
    sysexMessage.length = length;
    memcpy(sysexMessage.data, data, length);
    return sendToAllPeers((uint8_t *)&sysexMessage, sizeof(sysexMessage));
  }

  void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
  {
    if (len > sizeof(midi_message))
    {
      midi_sysex_message sysexMessage;
      memcpy(&sysexMessage, incomingData, sizeof(midi_sysex_message));
      // TODO: Handle SysEx message if needed
      return;
    }
    midi_message message;
    memcpy(&message, incomingData, sizeof(message));

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
    {
      int pitchBendValue = (message.secondByte << 7) | message.firstByte;
      int16_t signedValue = pitchBendValue - 8192;
      if (onPitchBendHandler)
        onPitchBendHandler(message.channel, signedValue);
      break;
    }
    case MIDI_START:
      if (onStartHandler)
        onStartHandler();
      break;
    case MIDI_STOP:
      if (onStopHandler)
        onStopHandler();
      break;
    case MIDI_CONTINUE:
      if (onContinueHandler)
        onContinueHandler();
      break;
    case MIDI_TIME_CLOCK:
      if (onClockHandler)
        onClockHandler();
      break;
    case MIDI_SONG_POS_POINTER:
    {
      int songPosValue = (message.secondByte << 7) | message.firstByte;
      if (onSongPositionHandler)
        onSongPositionHandler(songPosValue);
      break;
    }
    case MIDI_SONG_SELECT:
      if (onSongSelectHandler)
        onSongSelectHandler(message.firstByte);
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

  void setHandleStart(void (*callback)())
  {
    onStartHandler = callback;
  }

  void setHandleStop(void (*callback)())
  {
    onStopHandler = callback;
  }

  void setHandleContinue(void (*callback)())
  {
    onContinueHandler = callback;
  }

  void setHandleClock(void (*callback)())
  {
    onClockHandler = callback;
  }

  void setHandleSongPosition(void (*callback)(uint16_t value))
  {
    onSongPositionHandler = callback;
  }

  void setHandleSongSelect(void (*callback)(byte value))
  {
    onSongSelectHandler = callback;
  }

private:
  uint8_t _peers[MAX_PEERS][6];   // Array to store MAC addresses of peers
  int _peersCount;                // Current number of peers
  static esp_now_midi *_instance; // Static pointer to hold the instance
  DataSentCallback userDataSentCallback = nullptr;

  // MIDI Handlers
  void (*onNoteOnHandler)(byte channel, byte note, byte velocity) = nullptr;
  void (*onNoteOffHandler)(byte channel, byte note, byte velocity) = nullptr;
  void (*onControlChangeHandler)(byte channel, byte control, byte value) = nullptr;
  void (*onProgramChangeHandler)(byte channel, byte program) = nullptr;
  void (*onPitchBendHandler)(byte channel, int value) = nullptr;
  void (*onAfterTouchChannelHandler)(byte channel, byte value) = nullptr;
  void (*onAfterTouchPolyHandler)(byte channel, byte note, byte value) = nullptr;
  void (*onStartHandler)() = nullptr;
  void (*onStopHandler)() = nullptr;
  void (*onContinueHandler)() = nullptr;
  void (*onClockHandler)() = nullptr;
  void (*onSongPositionHandler)(uint16_t value) = nullptr;
  void (*onSongSelectHandler)(byte value) = nullptr;
};

esp_now_midi *esp_now_midi::_instance = nullptr;