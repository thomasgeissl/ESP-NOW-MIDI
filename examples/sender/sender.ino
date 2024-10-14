#include <esp_now.h>
#include <WiFi.h>
#include "esp_now_midi.h"


// run the print_mac firmware and adjust the mac address
uint8_t broadcastAddress[6] = { 0xCC, 0x8D, 0xA2, 0x8B, 0x85, 0x1C };

esp_now_midi ESP_NOW_MIDI;
void customOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Custom Callback - Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failure");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  ESP_NOW_MIDI.setup(broadcastAddress, customOnDataSent);
}

void loop() {
  esp_err_t result = ESP_NOW_MIDI.sendNoteOn(60, 127, 1);

  if (result != ESP_OK) {
    Serial.println("Error sending the data");
  }
  delay(100);
  result = ESP_NOW_MIDI.sendNoteOff(60, 0, 1);
  delay(100);
  result = ESP_NOW_MIDI.sendControlChange(1, 127, 1);
  delay(100);
  result = ESP_NOW_MIDI.sendControlChange(1, 0, 1);
  delay(100);
  result = ESP_NOW_MIDI.sendPitchBend(16383, 1);
  delay(100);
  result = ESP_NOW_MIDI.sendPitchBend(0, 1);

  delay(2000);
}