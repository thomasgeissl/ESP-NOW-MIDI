#include <WiFi.h>
#include <esp_wifi.h>

void setup(){
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin();

  uint8_t mac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, mac);
  if (ret == ESP_OK) {
    Serial.print("MAC address: ");
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                  mac[0], mac[1], mac[2],
                  mac[3], mac[4], mac[5]);
  } else {
    Serial.println("could not get MAC address");
  }
}
void loop(){}