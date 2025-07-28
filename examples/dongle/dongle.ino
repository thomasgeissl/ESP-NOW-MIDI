#include "./config.h"
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include <esp_now_midi.h>
#include <esp_system.h>



#if HAS_DISPLAY == 1
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
unsigned long updateDisplayTimeStamp = 0;
const long updateDisplayInterval = 1000;
#endif

String version = getVersion();

Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

midi_message message;
uint8_t baseMac[6];
String macStr;



uint8_t peerMacAddresses[DONGLE_MAX_PEERS][6];
int peerCount = 0;  // Keeps track of how many peers have been added

typedef void (*DataSentCallback)(const uint8_t *mac_addr, esp_now_send_status_t status);
static void DefaultOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Serial.print("\r\nLast Packet Send Status:\t");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

struct MACAddress {
  uint8_t bytes[MAC_ADDR_LEN];
};

struct MidiMessageHistory {
  midi_message message;

  unsigned long timestamp;  // Time the message was received
  bool outgoing;
};

MidiMessageHistory messageHistory[MAX_HISTORY];  // Array to store the message history
int messageIndex = 0;                            // Index to keep track of the last message in the array

// Function to add a new message to the history
void addToHistory(const midi_message &msg, bool outgoing = false) {
  messageHistory[messageIndex].message = msg;
  messageHistory[messageIndex].outgoing = outgoing;
  messageHistory[messageIndex].timestamp = millis();  // Store the current time
  messageIndex = (messageIndex + 1) % MAX_HISTORY;    // Circular buffer logic
}

void readMacAddress() {
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK) {
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                  baseMac[0], baseMac[1], baseMac[2],
                  baseMac[3], baseMac[4], baseMac[5]);
    macStr = String(baseMac[0], HEX) + ":" + String(baseMac[1], HEX) + ":" + String(baseMac[2], HEX) + ":" + String(baseMac[3], HEX) + ":" + String(baseMac[4], HEX) + ":" + String(baseMac[5], HEX);
  } else {
    Serial.println("Failed to read MAC address");
  }
}
// Helper function to print MAC addresses
void printMacAddress(const uint8_t *mac) {
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
}


#if HAS_DISPLAY == 1
void updateDisplay();
#endif
void onDataRecv(const esp_now_recv_info_t *messageInfo, const uint8_t *incomingData, int len);
void onNoteOn(byte channel, byte pitch, byte velocity);
void onNoteOff(byte channel, byte pitch, byte velocity);
void onControlChange(byte channel, byte controller, byte value);
void onProgramChange(byte channel, byte program);
void onAfterTouch(byte channel, byte pressure);
void onPolyAfterTouch(byte channel, byte note, byte pressure);
void onPitchBend(byte channel, int value);
void onStart();
void onStop();
void onContinue();
void onClock();
void onSongSelect(byte value);
void onSongPosition(unsigned int value);

esp_err_t send(const uint8_t mac[MAC_ADDR_LEN], midi_message message);
void send(midi_message message);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.printf("ESP-IDF Version: %s\n", esp_get_idf_version());


  // Init ESP-NOW
  WiFi.mode(WIFI_STA);
  readMacAddress();
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  } else {
    Serial.println("successfully initialized ESP-NOW");
  }
  esp_now_register_recv_cb(esp_now_recv_cb_t(onDataRecv));
  esp_now_register_send_cb(DefaultOnDataSent);

  // HERE YOU COULD MANUALLY ENTER THE MAC ADDRESS OF THE RECEIVER
  // uint8_t broadcastAddress[6] = { 0xCC, 0x8D, 0xA2, 0x8B, 0x85, 0x1C };
  // addMacAddress(broadcastAddress);

  // Init MIDI
  MIDI.begin(MIDI_CHANNEL_OMNI);

  TinyUSBDevice.setManufacturerDescriptor("grantler instruments");
  TinyUSBDevice.setProductDescriptor("enomik3000_dongle");

  // If already enumerated, additional class driverr begin() e.g msc, hid, midi won't take effect until re-enumeration
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }

  MIDI.setHandleNoteOn(onNoteOn);
  MIDI.setHandleNoteOff(onNoteOff);
  MIDI.setHandleControlChange(onControlChange);
  MIDI.setHandleProgramChange(onProgramChange);
  MIDI.setHandlePitchBend(onPitchBend);
  MIDI.setHandleAfterTouchChannel(onAfterTouch);
  MIDI.setHandleAfterTouchPoly(onPolyAfterTouch);
  MIDI.setHandleStart(onStart);
  MIDI.setHandleStop(onStop);
  MIDI.setHandleContinue(onContinue);
  MIDI.setHandleClock(onClock);
  MIDI.setHandleSongPosition(onSongPosition);
  MIDI.setHandleSongSelect(onSongSelect);

  // Init display
#if HAS_DISPLAY == 1
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }
  display.clearDisplay();
#endif
}

void loop() {
  unsigned long currentMillis = millis();
  MIDI.read();
#if HAS_DISPLAY == 1
  if (currentMillis - updateDisplayTimeStamp >= UPDATE_DISPLAY_INTERVAL) {
    updateDisplayTimeStamp = currentMillis;
    updateDisplay();
  }
#endif
}

#if HAS_DISPLAY == 1
void updateDisplay() {
  esp_now_peer_num_t peerInfo;
  esp_now_get_peer_num(&peerInfo);

  //auto macStr = String(baseMac[0], HEX) + ":" + String(baseMac[1], HEX) + ":" + String(baseMac[2], HEX) + ":" + String(baseMac[3], HEX) + ":" + String(baseMac[4], HEX) + ":" + String(baseMac[5], HEX);
  char macStr[18]; // 6 bytes × 2 chars + 5 colons + null terminator
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
        baseMac[0], baseMac[1], baseMac[2], 
        baseMac[3], baseMac[4], baseMac[5]);


  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(String("mac:") + macStr);
  display.print("v");
  display.print(version);
  display.print(String(" con:") + String(peerCount));
  display.println(String(" t:") + String(millis() / 1000));

  int lineY = 18;
  display.drawLine(0, lineY, SCREEN_WIDTH, lineY, WHITE);

  int yOffset = 22;  // Start rendering messages below the MAC address
  for (int i = 0; i < MAX_HISTORY; i++) {
    int index = (messageIndex + i) % MAX_HISTORY;
    auto statusString = String("n/a");
    auto channel = String(messageHistory[index].message.channel, HEX);
    channel.toUpperCase();
    if(channel == "10"){
      channel = "G";
    }

    char firstByte[4];  // Buffer to store formatted byte (3 characters + null terminator)
    sprintf(firstByte, "%3d", messageHistory[index].message.firstByte);
    char secondByte[4];  // Buffer to store formatted byte (3 characters + null terminator)
    sprintf(secondByte, "%3d", messageHistory[index].message.secondByte);

    switch (messageHistory[index].message.status) {
      case MIDI_NOTE_ON:
        {
          statusString = "N_ON";
          break;
        }
      case MIDI_NOTE_OFF:
        {
          statusString = "N_OFF";
          break;
        }
      case MIDI_CONTROL_CHANGE:
        {
          statusString = "CC";
          break;
        }
      case MIDI_PROGRAM_CHANGE:
        {
          statusString = "PC";
          break;
        }
      case MIDI_PITCH_BEND:
        {
          statusString = "PBEND";
          break;
        }
      case MIDI_AFTERTOUCH:
        {
          statusString = "AFTER";
          break;
        }
      case MIDI_POLY_AFTERTOUCH:
        {
          statusString = "PAFTER";
          break;
        }
      case MIDI_START:
        {
          statusString = "START";
          channel = "";
          firstByte[0] = '\0';
          secondByte[0] = '\0';
          break;
        }
      case MIDI_STOP:
        {
          statusString = "STOP";
          channel = "";
          firstByte[0] = '\0';
          secondByte[0] = '\0';
          break;
        }
      case MIDI_CONTINUE:
        {
          statusString = "CONT";
          channel = "";
          firstByte[0] = '\0';
          secondByte[0] = '\0';
          break;
        }

      default:
        break;
    }
    while (statusString.length() < 6) {
      statusString += " ";
    }

    // Only display non-empty messages
    if (messageHistory[index].timestamp > 0) {
      display.setCursor(0, yOffset);
      if (messageHistory[index].outgoing) {
        display.print("-> ");
      } else {
        display.print("<- ");
      }
      display.print(channel);
      display.print(" ");
      display.print(statusString);
      display.print(" ");

      display.print(firstByte);
      display.print(" ");
      display.print(secondByte);

      yOffset += 8;  // Move to the next line (8 pixels down)

      // Check if there's space left on the display
      if (yOffset > SCREEN_HEIGHT - 8) {
        break;  // Prevent writing off the screen
      }
    }
  }

  display.display();
}
#endif


void onDataRecv(const esp_now_recv_info_t *messageInfo, const uint8_t *incomingData, int len) {
  // printMacAddress(messageInfo->src_addr);

  bool peerExists = false;
  for (int i = 0; i < peerCount; i++) {
    if (memcmp(peerMacAddresses[i], messageInfo->src_addr, 6) == 0) {
      peerExists = true;
      break;
    }
  }
  if (!peerExists && peerCount < DONGLE_MAX_PEERS) {
    memcpy(peerMacAddresses[peerCount], messageInfo->src_addr, 6);
    //regiester peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, peerMacAddresses[peerCount], 6);
    peerInfo.channel = 0;      // Use the default Wi-Fi channel
    peerInfo.encrypt = false;  // Use unencrypted communication, or change as needed

    esp_err_t result = esp_now_add_peer(&peerInfo);
    if (result == ESP_OK) {
      Serial.println("Peer added successfully.");
    } else {
      Serial.print("Failed to add peer, error: ");
      Serial.println(result);
    }
    peerCount++;
  }

  memcpy(&message, incomingData, sizeof(message));
  addToHistory(message);

  auto status = message.status;
  auto channel = message.channel;
  switch (status) {
    case MIDI_NOTE_ON:
      {
        MIDI.sendNoteOn(message.firstByte, message.secondByte, channel);
        break;
      }
    case MIDI_NOTE_OFF:
      {
        MIDI.sendNoteOff(message.firstByte, message.secondByte, channel);
        break;
      }
    case MIDI_CONTROL_CHANGE:
      {
        MIDI.sendControlChange(message.firstByte, message.secondByte, channel);
        break;
      }
    case MIDI_PROGRAM_CHANGE:
      {
        MIDI.sendProgramChange(message.firstByte, channel);
        break;
      }
    case MIDI_AFTERTOUCH:
      {
        MIDI.sendAfterTouch(message.firstByte, channel);
        break;
      }
    case MIDI_POLY_AFTERTOUCH:
      {
        MIDI.sendAfterTouch(message.firstByte, message.secondByte, channel);
        break;
      }
    case MIDI_PITCH_BEND:
      {
        int pitchBendValue = (message.secondByte << 7) | message.firstByte;

        MIDI.sendPitchBend(pitchBendValue, channel);
        break;
      }
    case MIDI_START:
      {
        MIDI.sendStart();
        break;
      }
    case MIDI_STOP:
      {
        MIDI.sendStop();
        break;
      }
    case MIDI_CONTINUE:
      {
        MIDI.sendContinue();
        break;
      }
    case MIDI_TIME_CLOCK:
      {
        MIDI.sendClock();
        break;
      }
  }
}

void onNoteOn(byte channel, byte pitch, byte velocity) {
  midi_message message;
  message.status = MIDI_NOTE_ON;
  message.channel = channel;
  message.firstByte = pitch;
  message.secondByte = velocity;
  send(message);
}
void onNoteOff(byte channel, byte pitch, byte velocity) {
  midi_message message;
  message.status = MIDI_NOTE_OFF;
  message.channel = channel;
  message.firstByte = pitch;
  message.secondByte = velocity;
  send(message);
}
void onControlChange(byte channel, byte controller, byte value) {
  midi_message message;
  message.status = MIDI_CONTROL_CHANGE;
  message.channel = channel;
  message.firstByte = controller;
  message.secondByte = value;
  send(message);
}
void onProgramChange(byte channel, byte program) {
  midi_message message;
  message.status = MIDI_PROGRAM_CHANGE;
  message.channel = channel;
  message.firstByte = program;
  send(message);
}
void onAfterTouch(byte channel, byte pressure) {
  midi_message message;
  message.status = MIDI_AFTERTOUCH;
  message.channel = channel;
  message.firstByte = pressure;
  send(message);
}
void onPolyAfterTouch(byte channel, byte note, byte pressure) {
  midi_message message;
  message.status = MIDI_POLY_AFTERTOUCH;
  message.channel = channel;
  message.firstByte = note;
  message.secondByte = pressure;
  send(message);
}
void onPitchBend(byte channel, int value) {
  midi_message message;
  message.status = MIDI_PITCH_BEND;
  message.channel = channel;
  // Ensure value is within the valid 14-bit range (0 - 16383)
  value = value & 0x3FFF;  // Mask to ensure it's 14 bits (0x3FFF = 16383 in decimal)

  // Split the 14-bit value into LSB and MSB (each 7 bits)
  message.firstByte = value & 0x7F;          // LSB: lower 7 bits of the pitch bend value
  message.secondByte = (value >> 7) & 0x7F;  // MSB: upper 7 bits of the pitch bend value
  send(message);
}

void onStart() {
  midi_message message;
  message.status = MIDI_START;
  send(message);
}
void onStop() {
  midi_message message;
  message.status = MIDI_STOP;
  send(message);
}
void onContinue() {
  midi_message message;
  message.status = MIDI_CONTINUE;
  send(message);
}
void onClock() {
  midi_message message;
  message.status = MIDI_TIME_CLOCK;
  send(message);
}

void onSongPosition(unsigned int value){
    midi_message message;
  message.status = MIDI_SONG_POS_POINTER;
  // Ensure value is within the valid 14-bit range (0 - 16383)
  value = value & 0x3FFF;  // Mask to ensure it's 14 bits (0x3FFF = 16383 in decimal)

  // Split the 14-bit value into LSB and MSB (each 7 bits)
  message.firstByte = value & 0x7F;          // LSB: lower 7 bits of the pitch bend value
  message.secondByte = (value >> 7) & 0x7F;  // MSB: upper 7 bits of the pitch bend value
  send(message);
}
void onSongSelect(byte value){
  message.status = MIDI_SONG_SELECT;
  message.firstByte = value;
  send(message);
}

esp_err_t send(const uint8_t mac[MAC_ADDR_LEN], midi_message message) {
  esp_err_t result = esp_now_send(mac, (uint8_t *)&message, sizeof(message));
  return result;
}
void send(midi_message message) {
  bool shouldAddToHistory = true;  // should go later into function params to filter out clock messages
  switch (message.status) {
    case MIDI_TIME_CLOCK:
    case MIDI_SONG_POS_POINTER:
      {
        shouldAddToHistory = false;
        break;
      }
    default:
      break;
  }


  for (int i = 0; i < peerCount; i++) {
    // printMacAddress(peerMacAddresses[i]);
    esp_err_t result = esp_now_send(peerMacAddresses[i], (uint8_t *)&message, sizeof(message));
    if (result != ESP_OK) {
      Serial.print("Failed to send to peer ");
      Serial.print(" Error code: ");
      Serial.println(result);
    }
  }

  if (shouldAddToHistory) {
    addToHistory(message, true);
  }
}