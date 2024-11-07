// https://randomnerdtutorials.com/esp-now-esp32-arduino-ide/
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include <esp_now_midi.h>

Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

midi_message message;

void readMacAddress()
{
  uint8_t baseMac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK)
  {
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                  baseMac[0], baseMac[1], baseMac[2],
                  baseMac[3], baseMac[4], baseMac[5]);
  }
  else
  {
    Serial.println("Failed to read MAC address");
  }
}

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
  memcpy(&message, incomingData, sizeof(message));
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
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("status: ");
  Serial.println(message.status);
  Serial.print("first: ");
  Serial.println(message.firstByte);
  Serial.print("second: ");
  Serial.println(message.secondByte);
  Serial.println();
}

void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  readMacAddress();

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  MIDI.begin(MIDI_CHANNEL_OMNI);
  // If already enumerated, additional class driverr begin() e.g msc, hid, midi won't take effect until re-enumeration
  if (TinyUSBDevice.mounted())
  {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
}

void loop()
{
#ifdef TINYUSB_NEED_POLLING_TASK
  // Manual call tud_task since it isn't called by Core's background
  TinyUSBDevice.task();
#endif
}