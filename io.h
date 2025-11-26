#include <vector>
#include "esp_now_midi.h"

namespace enomik
{
    enum class PinType : uint8_t
    {
        INPUT_PIN,
        OUTPUT_PIN
    };

    struct PinConfig
    {
        uint8_t pin;
        uint8_t mode; // INPUT, OUTPUT, INPUT_PULLUP (Arduino constants)
        uint8_t midi_channel = 1;
        MidiStatus midi_type = MidiStatus::MIDI_CONTROL_CHANGE;
        uint8_t midi_cc = 0;
        uint8_t midi_note = 0;

        PinConfig(uint8_t p, uint8_t m)
            : pin(p), mode(m)
        {
        }
    };
    class IO
    {
    public:
        void begin()
        {
        }
        void loop()
        {
            for (auto &config : _pinConfigs)
            {
                auto value = 0;
                midi_message message;
                message.channel = config.midi_channel;
                message.status = config.midi_type;

                if (!_onMIDISendRequest)
                {
                    return;
                }

                if (config.mode == INPUT || config.mode == INPUT_PULLUP)
                {
                    value = digitalRead(config.pin);
                }
                else if (config.mode == 0x03)
                { // analog input
                    value = analogRead(config.pin);
                }

                if (config.midi_type == 176)
                { // CC
                    message.firstByte = config.midi_cc;
                    // TODO: map value to 0-127
                    message.secondByte = value & 0x7F;
                    _onMIDISendRequest(message);
                }
                else if (config.midi_type == 144)
                { // Note On
                    message.firstByte = config.midi_note;
                    // TODO: map value to 0-127
                    message.secondByte = value & 0x7F;
                    _onMIDISendRequest(message);
                }
                else if (config.midi_type == 128)
                { // Note Off
                    message.firstByte = config.midi_note;
                    // TODO: map value to 0-127
                    message.secondByte = value & 0x7F;
                    _onMIDISendRequest(message);
                }
                else if (config.midi_type == 224)
                { // Pitch Bend
                  // _client.midi.sendPitchBend(value, config.midi_channel);
                }
            }
        }
        void addPinConfig(const PinConfig &config)
        {
            if (config.mode == OUTPUT)
            {
                pinMode(config.pin, OUTPUT);
            }
            else if (config.mode == INPUT || config.mode == INPUT_PULLUP)
            {
                pinMode(config.pin, config.mode);
            }
            else if (config.mode == 0x03)
            { // analog input
              // no pinMode needed
            }
            else if (config.mode == 0x04)
            { // analog output/PWM output
              // ledcSetup(config.pin, 5000, 8); // 5kHz, 8-bit resolution
              // ledcAttachPin(config.pin, config.pin);
            }
            _pinConfigs.push_back(config);
            // TODO: does this need a short delay to settle?
        }
        void setOnMIDISendRequest(std::function<void(midi_message)> callback)
        {
            _onMIDISendRequest = callback;
        }

    private:
        std::vector<PinConfig> _pinConfigs;
        std::function<void(midi_message)> _onMIDISendRequest;
    };
};