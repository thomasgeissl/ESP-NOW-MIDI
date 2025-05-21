#include "config.h"
#include <esp_now.h>
#include <WiFi.h>
#include "esp_now_midi.h"
#include <AceButton.h>
using namespace ace_button;

// on the dongle: run the print_mac firmware and paste it here
uint8_t peerMacAddress[6] = {0xCC, 0x8D, 0xA2, 0x8B, 0x85, 0x1C};
esp_now_midi ESP_NOW_MIDI;

AceButton _buttons[NUMBER_OF_BUTTONS];

void customOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  // Serial.print("Custom Callback - Status: ");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failure");
}
void handleEvent(AceButton *, uint8_t, uint8_t);

void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  ESP_NOW_MIDI.setup(peerMacAddress, customOnDataSent);
  // ESP_NOW_MIDI.setup(destinationMacAddress); //or get rid of the custom send function and use the default one

  for (auto i = 0; i < NUMBER_OF_BUTTONS; i++)
  {
    pinMode(buttonPins[i], INPUT_PULLUP);
    _buttons[i].init(buttonPins[i], HIGH, i);
  }
  ButtonConfig *buttonConfig = ButtonConfig::getSystemButtonConfig();
  buttonConfig->setEventHandler(handleEvent);
  buttonConfig->setFeature(ButtonConfig::kFeatureClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureDoubleClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
  buttonConfig->setFeature(ButtonConfig::kFeatureRepeatPress);
}

void loop()
{
  for (auto i = 0; i < NUMBER_OF_BUTTONS; i++)
  {
    _buttons[i].check();
  }
}

void handleEvent(AceButton *button, uint8_t eventType, uint8_t buttonState)
{
  Serial.print(F("handleEvent(): eventType: "));
  Serial.print(AceButton::eventName(eventType));
  Serial.print(F("; buttonState: "));
  Serial.println(buttonState);

  uint8_t id = button->getId();

  switch (eventType)
  {
  case AceButton::kEventPressed:
    ESP_NOW_MIDI.sendNoteOn(MIDI_NOTE + id, 127, 1);
    break;
  case AceButton::kEventReleased:
    ESP_NOW_MIDI.sendNoteOff(MIDI_NOTE + id, 0, 1);
    break;
  }
}