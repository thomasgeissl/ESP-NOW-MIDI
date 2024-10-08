#include <esp_now.h>
#include <WiFi.h>
#include "esp_now_midi.h"


// REPLACE WITH YOUR RECEIVER MAC Address
uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

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
  esp_err_t result = ESP_NOW_MIDI.sendNoteOn(60,127,1);
   
  if (result == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }
  delay(2000);
}