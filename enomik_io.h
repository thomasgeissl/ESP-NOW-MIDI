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
        uint8_t min_midi_value = 0;
        uint8_t max_midi_value = 127;

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
                if(config.mode == OUTPUT || config.mode == 0x04) //digital out or pwm out
                {
                    continue; // skip outputs
                }

                if (config.mode == INPUT || config.mode == INPUT_PULLUP)
                {
                    value = digitalRead(config.pin);
                }
                else if (config.mode == 0x03)
                { // analog input
                    value = analogRead(config.pin);
                }

                if (config.midi_type == MidiStatus::MIDI_NOTE_OFF)
                { // CC
                    message.firstByte = config.midi_note;
                    // TODO: map value to 0-127
                    message.secondByte = value & 0x7F;
                    _onMIDISendRequest(message);
                }
                else if (config.midi_type == MidiStatus::MIDI_NOTE_ON)
                { // Note On
                    message.firstByte = config.midi_note;
                    // TODO: map value to 0-127
                    message.secondByte = value & 0x7F;
                    _onMIDISendRequest(message);
                }
                else if (config.midi_type == MidiStatus::MIDI_CONTROL_CHANGE)
                { // CC
                    message.firstByte = config.midi_cc;
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

        void onNoteOn(byte channel, byte note, byte velocity)
        {
            for (auto &config : _pinConfigs)
            {
                if (config.midi_type == MidiStatus::MIDI_NOTE_ON && config.midi_channel == channel && config.midi_note == note)
                {
                    if (config.mode == OUTPUT)
                    {
                        digitalWrite(config.pin, velocity > 0 ? HIGH : LOW);
                    }
                    else if (config.mode == 0x04)
                    {                                          // analog output/PWM
                        analogWrite(config.pin, velocity * 2); // TODO: map correctly
                    }
                }
            }
        }
        void onNoteOff(byte channel, byte note, byte velocity)
        {
            for (auto &config : _pinConfigs)
            {
                if (config.midi_type == MidiStatus::MIDI_NOTE_OFF && config.midi_channel == channel && config.midi_note == note)
                {
                    if (config.mode == OUTPUT)
                    {
                        digitalWrite(config.pin, LOW);
                    }
                    else if (config.mode == 0x04)
                    { // analog output/PWM
                        analogWrite(config.pin, LOW);
                    }
                }
            }
        }
        void onPitchBend(byte channel, int bend)
        {
            for (auto &config : _pinConfigs)
            {
                if (config.midi_type == MidiStatus::MIDI_PITCH_BEND && config.midi_channel == channel)
                {
                    if (config.mode == OUTPUT)
                    {
                        // Map bend value to 0-255 for 8-bit resolution
                        analogWrite(config.pin, bend < 8192 ? LOW : HIGH);
                    }
                    else if (config.mode == 0x04)
                    {                                                  // analog output/PWM
                        int mappedValue = map(bend, 0, 16383, 0, 255); // TODO: map to correct range based on config
                        analogWrite(config.pin, mappedValue);
                    }
                }
            }
        }
        void onControlChange(byte channel, byte control, byte value)
        {
            for (auto &config : _pinConfigs)
            {
                if (config.midi_type == MidiStatus::MIDI_CONTROL_CHANGE && config.midi_channel == channel && config.midi_cc == control)
                {
                    if (config.mode == OUTPUT)
                    {
                        analogWrite(config.pin, value); // assuming value is 0-127, may need mapping
                    }
                    else if (config.mode == 0x04)
                    {                                                 // analog output/PWM
                        int mappedValue = map(value, 0, 127, 0, 255); // TODO: map to correct range based on config
                        analogWrite(config.pin, mappedValue);
                    }
                }
            }
        }
        void onProgramChange(byte channel, byte program)
        {
            // TODO:
        }

    private:
        std::vector<PinConfig> _pinConfigs;
        std::function<void(midi_message)> _onMIDISendRequest;
    };
};