#include "./config.h"
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include <esp_now_midi.h>

#if HAS_DISPLAY == 1
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
unsigned long updateDisplayTimeStamp = 0;
const long updateDisplayInterval = 1000;
#endif

Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

midi_message message;
uint8_t baseMac[6];
String macStr;

// storing mac addresses of connected peers
uint8_t peers[MAX_PEERS][MAC_ADDR_LEN];
int peerCount = 0;

typedef void (*DataSentCallback)(const uint8_t *mac_addr, esp_now_send_status_t status);
static void DefaultOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  // Serial.print("\r\nLast Packet Send Status:\t");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

struct MidiMessageHistory
{
  midi_message message;

  unsigned long timestamp; // Time the message was received
  bool outgoing;
};

MidiMessageHistory messageHistory[MAX_HISTORY]; // Array to store the message history
int messageIndex = 0;                           // Index to keep track of the last message in the array

// Function to add a new message to the history
void addToHistory(const midi_message &msg, bool outgoing = false)
{
  messageHistory[messageIndex].message = msg;
  messageHistory[messageIndex].outgoing = outgoing;
  messageHistory[messageIndex].timestamp = millis(); // Store the current time
  messageIndex = (messageIndex + 1) % MAX_HISTORY;   // Circular buffer logic
}

void readMacAddress()
{
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK)
  {
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                  baseMac[0], baseMac[1], baseMac[2],
                  baseMac[3], baseMac[4], baseMac[5]);
    macStr = String(baseMac[0], HEX) + ":" + String(baseMac[1], HEX) + ":" + String(baseMac[2], HEX) + ":" + String(baseMac[3], HEX) + ":" + String(baseMac[4], HEX) + ":" + String(baseMac[5], HEX);
  }
  else
  {
    Serial.println("Failed to read MAC address");
  }
}

bool isMacStored(const uint8_t *mac)
{
  for (int i = 0; i < peerCount; i++)
  {
    if (memcmp(peers[i], mac, MAC_ADDR_LEN) == 0)
    {
      return true;
    }
  }
  return false;
}

bool addMacAddress(const uint8_t *mac)
{
  if (peerCount >= MAX_PEERS)
  {
    Serial.println("MAC address list is full!");
    return false;
  }

  if (!isMacStored(mac))
  {
    memcpy(peers[peerCount], mac, MAC_ADDR_LEN);
    peerCount++;
    Serial.print("Added MAC: ");
    for (int i = 0; i < MAC_ADDR_LEN; i++)
    {
      Serial.printf("%02X", mac[i]);
      if (i < MAC_ADDR_LEN - 1)
        Serial.print(":");
    }
    Serial.println();
    // Register peer
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, mac, MAC_ADDR_LEN);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
      Serial.println("Failed to add peer");
    }
    else
    {
      Serial.println("successfully added peer");
    }
    return true;
  }
  return false;
}

#if HAS_DISPLAY == 1
void updateDisplay();
#endif
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len);
void onNoteOn(byte channel, byte pitch, byte velocity);
void onNoteOff(byte channel, byte pitch, byte velocity);
void onControlChange(byte channel, byte controller, byte value);
void onProgramChange(byte channel, byte program);
void onAfterTouch(byte channel, byte pressure);
void onPolyAfterTouch(byte channel, byte note, byte pressure);
void onPitchBend(byte channel, int value);

esp_err_t send(const uint8_t mac[MAC_ADDR_LEN], midi_message message);
void send(midi_message message);

void setup()
{
  Serial.begin(115200);

  // Init ESP-NOW
  WiFi.mode(WIFI_STA);
  readMacAddress();
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }else{
    Serial.println("successfully initialized ESP-NOW");
  }
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  esp_now_register_send_cb(DefaultOnDataSent);

  // HERE YOU COULD MANUALLY ENTER THE MAC ADDRESS OF THE RECEIVER
  // uint8_t broadcastAddress[6] = { 0xCC, 0x8D, 0xA2, 0x8B, 0x85, 0x1C };
  // addMacAddress(broadcastAddress);

  // Init MIDI
  MIDI.begin(MIDI_CHANNEL_OMNI);
  // If already enumerated, additional class driverr begin() e.g msc, hid, midi won't take effect until re-enumeration
  if (TinyUSBDevice.mounted())
  {
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

  // Init display
#if HAS_DISPLAY == 1
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }
  display.clearDisplay();
#endif
}

void loop()
{
  unsigned long currentMillis = millis();
  MIDI.read();
#if HAS_DISPLAY == 1
  if (currentMillis - updateDisplayTimeStamp >= UPDATE_DISPLAY_INTERVAL)
  {
    updateDisplayTimeStamp = currentMillis;
    updateDisplay();
  }
#endif
}

#if HAS_DISPLAY == 1
void updateDisplay()
{
  auto macStr = String(baseMac[0], HEX) + ":" + String(baseMac[1], HEX) + ":" + String(baseMac[2], HEX) + ":" + String(baseMac[3], HEX) + ":" + String(baseMac[4], HEX) + ":" + String(baseMac[5], HEX);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(String("mac:") + macStr);
  display.print(String("connections:") + String(peerCount));
  display.println(String(" t:") + String(millis() / 1000));

  int lineY = 18;
  display.drawLine(0, lineY, SCREEN_WIDTH, lineY, WHITE);

  int yOffset = 22; // Start rendering messages below the MAC address
  for (int i = 0; i < MAX_HISTORY; i++)
  {
    int index = (messageIndex + i) % MAX_HISTORY;
    auto statusString = String("n/a");
    switch (messageHistory[index].message.status)
    {
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

    default:
      break;
    }
    while (statusString.length() < 6)
    {
      statusString += " ";
    }

    // Only display non-empty messages
    if (messageHistory[index].timestamp > 0)
    {
      display.setCursor(0, yOffset);
      if (messageHistory[index].outgoing)
      {
        display.print("-> ");
      }
      else
      {
        display.print("<- ");
      }
      display.print(String(messageHistory[index].message.channel, HEX));
      display.print(" ");
      display.print(statusString);
      display.print(" ");

      char formattedByte[4]; // Buffer to store formatted byte (3 characters + null terminator)
      // Format and print the first byte with spaces for padding
      sprintf(formattedByte, "%3d", messageHistory[index].message.firstByte);
      display.print(formattedByte);
      display.print(" ");

      // Format and print the second byte with spaces for padding
      sprintf(formattedByte, "%3d", messageHistory[index].message.secondByte);
      display.print(formattedByte);

      yOffset += 8; // Move to the next line (8 pixels down)

      // Check if there's space left on the display
      if (yOffset > SCREEN_HEIGHT - 8)
      {
        break; // Prevent writing off the screen
      }
    }
  }

  display.display();
}
#endif

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
  addMacAddress(mac);
  memcpy(&message, incomingData, sizeof(message));
  addToHistory(message);

  auto status = message.status;
  auto channel = message.channel;
  switch (status)
  {
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
  }
}

void onNoteOn(byte channel, byte pitch, byte velocity)
{
  midi_message message;
  message.status = MIDI_NOTE_ON;
  message.channel = channel;
  message.firstByte = pitch;
  message.secondByte = velocity;
  send(message);
}
void onNoteOff(byte channel, byte pitch, byte velocity)
{
  midi_message message;
  message.status = MIDI_NOTE_OFF;
  message.channel = channel;
  message.firstByte = pitch;
  message.secondByte = velocity;
  send(message);
}
void onControlChange(byte channel, byte controller, byte value)
{
  midi_message message;
  message.status = MIDI_CONTROL_CHANGE;
  message.channel = channel;
  message.firstByte = controller;
  message.secondByte = value;
  send(message);
}
void onProgramChange(byte channel, byte program)
{
  midi_message message;
  message.status = MIDI_PROGRAM_CHANGE;
  message.channel = channel;
  message.firstByte = program;
  send(message);
}
void onAfterTouch(byte channel, byte pressure)
{
  midi_message message;
  message.status = MIDI_AFTERTOUCH;
  message.channel = channel;
  message.firstByte = pressure;
  send(message);
}
void onPolyAfterTouch(byte channel, byte note, byte pressure)
{
  midi_message message;
  message.status = MIDI_POLY_AFTERTOUCH;
  message.channel = channel;
  message.firstByte = note;
  message.secondByte = pressure;
  send(message);
}
void onPitchBend(byte channel, int value)
{
  midi_message message;
  message.status = MIDI_PITCH_BEND;
  message.channel = channel;
  // Ensure value is within the valid 14-bit range (0 - 16383)
  value = value & 0x3FFF; // Mask to ensure it's 14 bits (0x3FFF = 16383 in decimal)

  // Split the 14-bit value into LSB and MSB (each 7 bits)
  message.firstByte = value & 0x7F;         // LSB: lower 7 bits of the pitch bend value
  message.secondByte = (value >> 7) & 0x7F; // MSB: upper 7 bits of the pitch bend value
  send(message);
}

esp_err_t send(const uint8_t mac[MAC_ADDR_LEN], midi_message message)
{

  esp_err_t result = esp_now_send(mac, (uint8_t *)&message, sizeof(message));
  return result;
}
void send(midi_message message)
{
  for (int i = 0; i < peerCount; i++)
  {
    send(peers[i], message);
  }
  addToHistory(message, true);
}