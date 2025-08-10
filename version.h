#define ESP_NOW_MIDI_VERSION_MAJOR 0
#define ESP_NOW_MIDI_VERSION_MINOR 6
#define ESP_NOW_MIDI_VERSION_PATCH 0

inline String getVersion() {
    return String(ESP_NOW_MIDI_VERSION_MAJOR) + "." + 
           String(ESP_NOW_MIDI_VERSION_MINOR) + "." + 
           String(ESP_NOW_MIDI_VERSION_PATCH);
}