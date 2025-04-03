#define MOZZI_OUTPUT_I2S_DAC  // Enable I2S output for MAX98357A
#include <esp_now.h>
#include <WiFi.h>
#include "esp_now_midi.h"
#include <driver/i2s.h>
// before including Mozzi.h, configure external audio output mode:
#include "MozziConfigValues.h"  // for named option values
#define MOZZI_AUDIO_MODE MOZZI_OUTPUT_EXTERNAL_TIMED
#define MOZZI_AUDIO_BITS 24
#define MOZZI_CONTROL_RATE 256 // Hz, powers of 2 are most reliable
#include <Mozzi.h>
#include <Oscil.h>
#include <tables/sin2048_int8.h>
#include <ADSR.h>

#define I2S_BCLK 37        // Recommended BCLK pin
#define I2S_WCLK 38        // Recommended LRCLK pin
#define I2S_DOUT 17        // Recommended Data out pin
#define I2S_NUM I2S_NUM_0  // I2S peripheral
#define SAMPLE_RATE 44100  // Standard 44.1kHz sample rate
#define BUFFER_SIZE 128    // I2S buffer size

// on the dongle: run the print_mac firmware and paste it here
uint8_t peerMacAddress[6] = { 0xCC, 0x8D, 0xA2, 0x8B, 0x85, 0x1C };

esp_now_midi ESP_NOW_MIDI;
void customOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {}

Oscil<2048, AUDIO_RATE> aSin(SIN2048_DATA);
ADSR<CONTROL_RATE, AUDIO_RATE> envelope;
byte currentNote = 0;
float currentFrequency = 0.0f;

void onNoteOn(byte channel, byte note, byte velocity) {
  currentNote = note;
  currentFrequency = 440.0f * pow(2.0f, (note - 69) / 12.0f);
  aSin.setFreq(currentFrequency);
  envelope.noteOn();
}

void onNoteOff(byte channel, byte note, byte velocity) {
  if (note == currentNote) {
    envelope.noteOff();
  }
}

void setupI2S() {
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_I2S,
      .intr_alloc_flags = 0,
      .dma_buf_count = 8,
      .dma_buf_len = BUFFER_SIZE,
      .use_apll = false,
      .tx_desc_auto_clear = true
  };

  i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_BCLK,
      .ws_io_num = I2S_WCLK,
      .data_out_num = I2S_DOUT,
      .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM);
}

void audioOutput(const AudioOutput f) // f is a structure containing both channels
{
  Serial.println("TODO: write data to i2s amp");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  //setup esp now midi
  ESP_NOW_MIDI.setup(peerMacAddress, customOnDataSent);
  ESP_NOW_MIDI.setHandleNoteOn(onNoteOn);
  ESP_NOW_MIDI.setHandleNoteOff(onNoteOff);

  //setup audio
  envelope.setADLevels(255, 64);
  envelope.setTimes(50, 200, 100, 200);
  setupI2S();
  startMozzi();

  //register esp now midi client
  delay(1000);
  ESP_NOW_MIDI.sendControlChange(127, 127, 16);
}

void updateControl() {
  envelope.update();
}

AudioOutput updateAudio() {
  return MonoOutput::fromNBit(24, (int32_t)aSin.next() * envelope.next());
}

void loop() {
  Serial.println("loop");
  audioHook();
}
