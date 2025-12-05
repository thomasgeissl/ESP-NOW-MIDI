#include <vector>
#include <Preferences.h>
#include "esp_now_midi.h"
#include "utils/mac.h"

// Detect ESP32 variant and set ADC resolution
#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#define ADC_RESOLUTION 13
#define ADC_MAX_VALUE 8191
#define TOUCH_MAX_VALUE 100 // Touch range for S2/S3
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define ADC_RESOLUTION 12
#define ADC_MAX_VALUE 4095
#define TOUCH_MAX_VALUE 100 // C3 doesn't have touch, but keeping for compatibility
#else                       // Original ESP32
#define ADC_RESOLUTION 12
#define ADC_MAX_VALUE 4095
#define TOUCH_MAX_VALUE 100
#endif

namespace enomik
{
    enum class PinType : uint8_t
    {
        INPUT_PIN,
        OUTPUT_PIN
    };

    // Pin mode constants for clarity
    static constexpr uint8_t ENOMIK_INPUT = 0x00;
    static constexpr uint8_t ENOMIK_OUTPUT = 0x01;
    static constexpr uint8_t ENOMIK_INPUT_PULLUP = 0x02; // INPUT_PULLUP == 5
    static constexpr uint8_t ENOMIK_ANALOG_INPUT = 0x03;
    static constexpr uint8_t ENOMIK_ANALOG_OUTPUT = 0x04;
    static constexpr uint8_t ENOMIK_INPUT_TOUCH = 0x05;

    struct PinConfig
    {
        uint8_t pin;
        uint8_t mode;
        uint8_t threshold = 0;
        uint8_t midi_channel = 1;
        MidiStatus midi_type = MidiStatus::MIDI_CONTROL_CHANGE;
        uint8_t midi_cc = 0;
        uint8_t midi_note = 0;
        uint8_t min_midi_value = 0;
        uint8_t max_midi_value = 127;

        PinConfig(uint8_t p, uint8_t m)
            : pin(p), mode(m) {}
    };

    struct PinState
    {
        int lastValue = -1;
        unsigned long lastChangeTime = 0;
        unsigned long lastSendTime = 0;
        float smoothedValue = 0;
        bool touched = false;
    };

    enum class SysExCommand : uint8_t
    {
        SET_PIN_CONFIG = 0x01,
        GET_PIN_CONFIG = 0x02,
        CLEAR_PIN_CONFIGS = 0x03,
        GET_ALL_PIN_CONFIGS = 0x04,
        DELETE_PIN_CONFIG = 0x05,
        GET_MAC = 0x06,
        ADD_PEER = 0x07,
        GET_PEERS = 0x08,
        RESET = 0x09,
        GET_PIN_CONFIG_RESPONSE = 64 + GET_PIN_CONFIG,
        GET_ALL_PIN_CONFIGS_RESPONSE = 64 + GET_ALL_PIN_CONFIGS,
        GET_PEERS_RESPONSE = 64 + GET_PEERS
    };

    class IO
    {
    public:
        static constexpr unsigned long DEBOUNCE_MS = 50;
        static constexpr int ANALOG_THRESHOLD = 4;
        static constexpr unsigned long ANALOG_MIN_INTERVAL = 10;
        static constexpr float SMOOTHING_FACTOR = 0.3f;

        void begin()
        {
            analogReadResolution(ADC_RESOLUTION);
            _pinConfigs = loadPinConfigsFromPrefs();
            _pinStates.clear();

            for (const auto &config : _pinConfigs)
            {
                initializePinHardware(config);
                _pinStates.push_back(PinState());
            }
        }

        void loop()
        {
            unsigned long now = millis();

            for (size_t i = 0; i < _pinConfigs.size(); i++)
            {
                auto &config = _pinConfigs[i];
                auto &state = _pinStates[i];

                if (config.mode == ENOMIK_OUTPUT || config.mode == ENOMIK_ANALOG_OUTPUT)
                    continue;

                int currentValue = 0;
                bool shouldSend = false;

                if (config.mode == ENOMIK_INPUT || config.mode == ENOMIK_INPUT_PULLUP)
                {
                    currentValue = digitalRead(config.pin);
                    if (currentValue != state.lastValue)
                    {
                        Serial.println("Pin " + String(config.pin) + " changed to " + String(currentValue));
                        // if (now - state.lastChangeTime < DEBOUNCE_MS)
                        //     continue;

                        state.lastChangeTime = now;
                        shouldSend = true;
                    }
                }
                else if (config.mode == ENOMIK_ANALOG_INPUT)
                {
                    int rawValue = analogRead(config.pin);

                    if (state.lastValue == -1)
                        state.smoothedValue = rawValue;
                    else
                        state.smoothedValue = (SMOOTHING_FACTOR * rawValue) +
                                              (1.0f - SMOOTHING_FACTOR) * state.smoothedValue;

                    // For pitch bend, preserve full ADC resolution
                    if (config.midi_type == MidiStatus::MIDI_PITCH_BEND)
                    {
                        // Map directly to 14-bit pitch bend range (0-16383)
                        currentValue = map((int)state.smoothedValue, 0, ADC_MAX_VALUE, 0, 16383);
                        currentValue = constrain(currentValue, 0, 16383);

                        // Use higher threshold for pitch bend since we have more resolution
                        if (state.lastValue != -1 && abs(currentValue - state.lastValue) < (ANALOG_THRESHOLD * 4))
                            continue;
                    }
                    else
                    {
                        // For other MIDI types, use standard 7-bit range
                        int mappedValue = map((int)state.smoothedValue, 0, ADC_MAX_VALUE,
                                              config.min_midi_value, config.max_midi_value);
                        mappedValue = constrain(mappedValue, 0, 127);
                        currentValue = mappedValue;

                        if (state.lastValue != -1 && abs(currentValue - state.lastValue) < ANALOG_THRESHOLD)
                            continue;
                    }

                    if (now - state.lastSendTime < ANALOG_MIN_INTERVAL)
                        continue;

                    shouldSend = true;
                }
                else if (config.mode == ENOMIK_INPUT_TOUCH)
                {
                    int touchValue = touchRead(config.pin);

                    if (config.threshold == 0)
                    {
                        // No threshold set, send scaled touch values with smoothing
                        // Touch values typically range from ~10 (strong touch) to ~80+ (no touch)

                        // Initialize smoothed value on first read
                        if (state.lastValue == -1)
                            state.smoothedValue = touchValue;
                        else
                            state.smoothedValue = (SMOOTHING_FACTOR * touchValue) +
                                                  (1.0f - SMOOTHING_FACTOR) * state.smoothedValue;

                        // Invert mapping: lower touch value (stronger touch) = higher MIDI value
                        int mappedValue = map((int)state.smoothedValue, 0, 100, 127, 0);
                        mappedValue = constrain(mappedValue, 0, 127);

                        // Then apply user's min/max range
                        currentValue = map(mappedValue, 0, 127,
                                           config.min_midi_value, config.max_midi_value);
                        currentValue = constrain(currentValue, config.min_midi_value, config.max_midi_value);

                        if (state.lastValue != -1 && abs(currentValue - state.lastValue) < ANALOG_THRESHOLD)
                            continue;

                        if (now - state.lastSendTime < ANALOG_MIN_INTERVAL)
                            continue;

                        shouldSend = true;
                    }
                    else if (config.threshold > 0) // if threshold is set
                    {
                        // Lower values = stronger touch on ESP32
                        bool isTouched = touchValue < config.threshold;

                        if (isTouched != state.touched)
                        {
                            if (now - state.lastChangeTime < DEBOUNCE_MS)
                                continue;

                            state.lastChangeTime = now;
                            state.touched = isTouched;
                            currentValue = isTouched ? config.max_midi_value : config.min_midi_value;
                            shouldSend = true;

                            Serial.println("Touch " + String(config.pin) +
                                           " value: " + String(touchValue) +
                                           " touched: " + String(isTouched));
                        }
                    }
                }

                if (shouldSend)
                {
                    state.lastValue = currentValue;
                    state.lastSendTime = now;
                    sendMidiMessage(config, currentValue);
                }
            }
        }

        void addPinConfig(const PinConfig &config)
        {
            initializePinHardware(config);
            _pinConfigs.push_back(config);
            _pinStates.push_back(PinState());
            savePinConfigsToPrefs(_pinConfigs);

            Serial.println("Pin config added");
            Serial.println("size: " + String(_pinConfigs.size()));
            printPinConfigs();
        }
        void upsertPinConfig(const PinConfig &config)
        {
            Serial.println("=== UPSERT START ===");
            Serial.print("Upserting pin: ");
            Serial.println(config.pin);
            Serial.print("Current config count: ");
            Serial.println(_pinConfigs.size());

            // Debug: print all current pins
            for (size_t i = 0; i < _pinConfigs.size(); i++)
            {
                Serial.print("  Existing pin: ");
                Serial.println(_pinConfigs[i].pin);
            }

            bool removed = false;
            for (size_t i = 0; i < _pinConfigs.size(); i++)
            {
                if (_pinConfigs[i].pin == config.pin)
                {
                    Serial.print("Removing existing config at index: ");
                    Serial.println(i);
                    _pinConfigs.erase(_pinConfigs.begin() + i);
                    _pinStates.erase(_pinStates.begin() + i);
                    removed = true;
                    break;
                }
            }

            if (!removed)
            {
                Serial.println("No existing config found - adding new");
            }

            Serial.print("Adding new config for pin: ");
            Serial.println(config.pin);
            _pinConfigs.push_back(config);
            _pinStates.push_back(PinState());
            initializePinHardware(config);

            Serial.print("Config count after: ");
            Serial.println(_pinConfigs.size());
            savePinConfigsToPrefs(_pinConfigs);
            Serial.println("=== UPSERT END ===");
        }

        void printPinConfigs()
        {
            Serial.println("=== Pin Configurations ===");
            for (size_t i = 0; i < _pinConfigs.size(); i++)
            {
                const auto &cfg = _pinConfigs[i];
                Serial.print("Pin: ");
                Serial.print(cfg.pin);
                Serial.print(" | Mode: ");
                Serial.print(cfg.mode);
                Serial.print(" | MIDI Channel: ");
                Serial.print(cfg.midi_channel);
                Serial.print(" | MIDI Type: ");
                Serial.print(static_cast<uint8_t>(cfg.midi_type));
                Serial.print(" | CC: ");
                Serial.print(cfg.midi_cc);
                Serial.print(" | Note: ");
                Serial.print(cfg.midi_note);
                Serial.print(" | Min MIDI: ");
                Serial.print(cfg.min_midi_value);
                Serial.print(" | Max MIDI: ");
                Serial.println(cfg.max_midi_value);
            }
            Serial.println("==========================");
        }

        void setOnMIDISendRequest(std::function<void(midi_message)> callback)
        {
            _onMIDISendRequest = callback;
        }
        void setOnAddPeerRequest(std::function<void(uint8_t mac[])> callback)
        {
            _onAddPeerRequest = callback;
        }
        void setOnGetPeersRequest(std::function<void()> callback)
        {
            _onGetPeersRequest = callback;
        }
        void setOnResetRequest(std::function<void()> callback)
        {
            _onResetRequest = callback;
        }

        void onNoteOn(byte channel, byte note, byte velocity)
        {
            for (auto &config : _pinConfigs)
            {
                if (config.midi_type == MidiStatus::MIDI_NOTE_ON &&
                    config.midi_channel == channel &&
                    config.midi_note == note)
                {
                    if (config.mode == ENOMIK_OUTPUT)
                    {
                        digitalWrite(config.pin, velocity > 0 ? HIGH : LOW);
                    }
                    else if (config.mode == ENOMIK_ANALOG_OUTPUT)
                    {
                        int mappedValue = map(velocity, 0, 127,
                                              config.min_midi_value, config.max_midi_value);
                        analogWrite(config.pin, mappedValue);
                    }
                }
            }
        }

        void onNoteOff(byte channel, byte note, byte velocity)
        {
            for (auto &config : _pinConfigs)
            {
                if (config.midi_type == MidiStatus::MIDI_NOTE_OFF &&
                    config.midi_channel == channel &&
                    config.midi_note == note)
                {
                    if (config.mode == ENOMIK_OUTPUT)
                    {
                        digitalWrite(config.pin, LOW);
                    }
                    else if (config.mode == ENOMIK_ANALOG_OUTPUT)
                    {
                        analogWrite(config.pin, 0);
                    }
                }
            }
        }

        void onPitchBend(byte channel, int bend)
        {
            for (auto &config : _pinConfigs)
            {
                if (config.midi_type == MidiStatus::MIDI_PITCH_BEND &&
                    config.midi_channel == channel)
                {
                    if (config.mode == OUTPUT)
                    {
                        digitalWrite(config.pin, bend >= 8192 ? HIGH : LOW);
                    }
                    else if (config.mode == ENOMIK_ANALOG_OUTPUT)
                    {
                        int mappedValue = map(bend, 0, 16383,
                                              config.min_midi_value, config.max_midi_value);
                        analogWrite(config.pin, constrain(mappedValue, 0, 255));
                    }
                }
            }
        }

        void onControlChange(byte channel, byte control, byte value)
        {
            for (auto &config : _pinConfigs)
            {
                if (config.midi_type == MidiStatus::MIDI_CONTROL_CHANGE &&
                    config.midi_channel == channel &&
                    config.midi_cc == control)
                {
                    if (config.mode == ENOMIK_OUTPUT)
                    {
                        digitalWrite(config.pin, value > 63 ? HIGH : LOW);
                    }
                    else if (config.mode == ENOMIK_ANALOG_OUTPUT)
                    {
                        int mappedValue = map(value, 0, 127,
                                              config.min_midi_value, config.max_midi_value);
                        analogWrite(config.pin, constrain(mappedValue, 0, 255));
                    }
                }
            }
        }

        void onProgramChange(byte channel, byte program)
        {
        }

        void onSysEx(const uint8_t *data, uint16_t length)
        {
            if (length < 4)
                return;

            SysExCommand command = static_cast<SysExCommand>(data[2]);
            uint16_t offset = 3;

            switch (command)
            {
            case SysExCommand::ADD_PEER:
                handleAddPeer(data + offset, length - offset - 1);
                break;
            case SysExCommand::GET_PEERS:
                handleGetPeers();
                break;
            case SysExCommand::RESET:
                handleReset();
                break;
            case SysExCommand::GET_MAC:
                handleGetMac();
                break;
            case SysExCommand::SET_PIN_CONFIG:
                handleSetPinConfig(data + offset, length - offset - 1);
                break;
            case SysExCommand::GET_PIN_CONFIG:
                handleGetPinConfig(data + offset, length - offset - 1);
                break;
            case SysExCommand::CLEAR_PIN_CONFIGS:
                handleClearPinConfigs();
                break;
            case SysExCommand::GET_ALL_PIN_CONFIGS:
                handleGetAllPinConfigs();
                break;
            case SysExCommand::DELETE_PIN_CONFIG:
                handleDeletePinConfig(data + offset, length - offset - 1);
                break;
            }
        }

        void setOnSysExSendRequest(std::function<void(midi_sysex_message)> callback)
        {
            _onSysExSendRequest = callback;
        }

        void savePinConfigs()
        {
            savePinConfigsToPrefs(_pinConfigs);
        }

        void loadPinConfigs()
        {
            _pinConfigs = loadPinConfigsFromPrefs();
            _pinStates.assign(_pinConfigs.size(), PinState());
        }

    private:
        std::vector<PinConfig> _pinConfigs;
        std::vector<PinState> _pinStates;
        std::function<void(midi_message)> _onMIDISendRequest;
        std::function<void(midi_sysex_message)> _onSysExSendRequest;
        std::function<void(uint8_t mac[])> _onAddPeerRequest;
        std::function<void()> _onGetPeersRequest;
        std::function<void()> _onResetRequest;
        Preferences _preferences;

        void initializePinHardware(const PinConfig &c)
        {
            if (c.mode == ENOMIK_OUTPUT)
            {
                pinMode(c.pin, OUTPUT);
            }
            else if (c.mode == ENOMIK_INPUT)
            {
                pinMode(c.pin, INPUT);
            }
            else if (c.mode == ENOMIK_INPUT_PULLUP)
            {
                pinMode(c.pin, INPUT_PULLUP);
            }
            else if (c.mode == ENOMIK_INPUT_TOUCH)
            {
                // Touch pins: T0-T9 (GPIO 4, 0, 2, 15, 13, 12, 14, 27, 33, 32)
                // Set a threshold - you may need to calibrate this
                touchAttachInterrupt(c.pin, nullptr, 40); // 40 is a typical threshold
            }
        }

        void sendMidiMessage(const PinConfig &config, int value)
        {
            Serial.println("Sending MIDI message for pin " + String(config.pin) +
                           " value: " + String(value) +
                           " type: " + String(static_cast<uint8_t>(config.midi_type)));
            if (!_onMIDISendRequest)
                return;

            midi_message msg;
            msg.channel = config.midi_channel;
            msg.status = config.midi_type;

            switch (config.midi_type)
            {
            case MidiStatus::MIDI_NOTE_ON:
            case MidiStatus::MIDI_NOTE_OFF:
                msg.firstByte = config.midi_note;
                msg.secondByte = value & 0x7F;
                break;

            case MidiStatus::MIDI_CONTROL_CHANGE:
                msg.firstByte = config.midi_cc;
                msg.secondByte = value > 0 ? config.min_midi_value : config.max_midi_value;
                break;

            case MidiStatus::MIDI_PITCH_BEND:
            {
                int pb = map(value, 0, 127, 0, 16383);
                msg.firstByte = pb & 0x7F;
                msg.secondByte = (pb >> 7) & 0x7F;
                break;
            }
            }

            _onMIDISendRequest(msg);
        }

        void savePinConfigsToPrefs(const std::vector<PinConfig> &configs)
        {
            _preferences.begin("pinconfigs", false);

            for (size_t i = 0; i < configs.size(); i++)
            {
                String key = "cfg" + String(i);
                uint8_t buf[8] = {
                    configs[i].pin,
                    configs[i].mode,
                    configs[i].midi_channel,
                    static_cast<uint8_t>(configs[i].midi_type),
                    configs[i].midi_cc,
                    configs[i].midi_note,
                    configs[i].min_midi_value,
                    configs[i].max_midi_value};
                _preferences.putBytes(key.c_str(), buf, sizeof(buf));
            }

            _preferences.putUInt("count", configs.size());
            _preferences.end();
        }

        std::vector<PinConfig> loadPinConfigsFromPrefs()
        {
            std::vector<PinConfig> out;

            _preferences.begin("pinconfigs", true);
            size_t count = _preferences.getUInt("count", 0);

            for (size_t i = 0; i < count; i++)
            {
                String key = "cfg" + String(i);
                uint8_t buf[8];

                if (_preferences.getBytes(key.c_str(), buf, 8) == 8)
                {
                    PinConfig cfg(buf[0], buf[1]);
                    cfg.midi_channel = buf[2];
                    cfg.midi_type = static_cast<MidiStatus>(buf[3]);
                    cfg.midi_cc = buf[4];
                    cfg.midi_note = buf[5];
                    cfg.min_midi_value = buf[6];
                    cfg.max_midi_value = buf[7];
                    out.push_back(cfg);
                }
            }

            _preferences.end();
            return out;
        }

        void handleAddPeer(const uint8_t *d, uint16_t len)
        {
            Serial.println("Handling Add Peer request...");
            if (len < 12) // Need 12 nibbles for 6 MAC bytes
                return;

            uint8_t mac[6];

            // Decode nibbles back to bytes
            for (int i = 0; i < 6; i++)
            {
                uint8_t high_nibble = d[i * 2];
                uint8_t low_nibble = d[i * 2 + 1];
                mac[i] = (high_nibble << 4) | low_nibble;
            }

            if (_onAddPeerRequest)
            {
                _onAddPeerRequest(mac);
            }
        }

        void handleGetPeers()
        {
            if (_onGetPeersRequest)
            {
                _onGetPeersRequest();
            }
        }
        void handleReset()
        {
            Serial.println("Performing system reset...");

            // Clear in-memory pin configurations
            _pinConfigs.clear();
            _pinStates.clear();

            // Clear stored preferences
            _preferences.begin("pinconfigs", false);
            _preferences.clear(); // Erases all keys in the namespace
            _preferences.end();

            if (_onResetRequest)
            {
                _onResetRequest();
            }
            Serial.println("All pin configurations and preferences cleared.");

            // Optionally, notify via SysEx
            // if (_onSysExSendRequest)
            // {
            //     uint8_t resp[] = {0xF0, 0x7D,
            //                       static_cast<uint8_t>(SysExCommand::RESET),
            //                       0x00, 0xF7};
            //     _onSysExSendRequest(resp, sizeof(resp));
            // }
        }
        void handleGetMac()
        {
            if (!_onSysExSendRequest)
                return;

            uint8_t mac[6];
            esp_read_mac(mac, ESP_MAC_WIFI_STA); // Use ESP_MAC_WIFI_STA instead

            midi_sysex_message msg;
            msg.data[0] = 0xF0; // SysEx start
            msg.data[1] = 0x7D; // Manufacturer ID (non-commercial)
            // msg.data[2] = SysExCommand::GET_MAC; // Command
            msg.data[2] = static_cast<uint8_t>(SysExCommand::GET_MAC);

            int idx = 3;
            for (int i = 0; i < 6; ++i)
            {
                uint8_t b = mac[i];
                uint8_t hi = (b >> 4) & 0x0F;
                uint8_t lo = b & 0x0F;

                msg.data[idx++] = hi; // safe 7-bit value
                msg.data[idx++] = lo; // safe 7-bit value
            }

            msg.data[idx++] = 0xF7;
            msg.length = idx;

            Serial.println("Sending MAC address via SysEx:");
            _onSysExSendRequest(msg);
        }

        void handleSetPinConfig(const uint8_t *d, uint16_t len)
        {
            if (len < 7)
                return;

            PinConfig cfg(d[0], d[1]);
            cfg.threshold = d[2];
            cfg.midi_channel = d[3];
            cfg.midi_type = static_cast<MidiStatus>(d[4] * 2);
            cfg.midi_cc = d[5];
            cfg.midi_note = d[5];
            cfg.min_midi_value = d[6];
            cfg.max_midi_value = d[7];

            upsertPinConfig(cfg);
            sendGetPinConfigResponse(cfg);
        }

        void handleGetPinConfig(const uint8_t *d, uint16_t len)
        {
            if (len < 1)
                return;

            uint8_t pin = d[0];

            for (const auto &cfg : _pinConfigs)
            {
                if (cfg.pin == pin)
                {
                    sendGetPinConfigResponse(cfg);
                    return;
                }
            }
        }

        void handleClearPinConfigs()
        {
            _pinConfigs.clear();
            _pinStates.clear();

            savePinConfigsToPrefs(_pinConfigs);

            if (_onSysExSendRequest)
            {
                uint8_t resp[] = {0xF0, 0x7D,
                                  static_cast<uint8_t>(SysExCommand::CLEAR_PIN_CONFIGS),
                                  0x00, 0xF7};
                midi_sysex_message msg;
                msg.length = sizeof(resp);
                memcpy(msg.data, resp, sizeof(resp));
                _onSysExSendRequest(msg);
            }
        }

        void handleGetAllPinConfigs()
        {
            for (const auto &cfg : _pinConfigs)
                sendGetPinConfigResponse(cfg);
        }

        void handleDeletePinConfig(const uint8_t *d, uint16_t len)
        {
            if (len < 1)
                return;

            uint8_t pin = d[0];

            for (size_t i = 0; i < _pinConfigs.size(); i++)
            {
                if (_pinConfigs[i].pin == pin)
                {
                    _pinConfigs.erase(_pinConfigs.begin() + i);
                    _pinStates.erase(_pinStates.begin() + i);
                    savePinConfigsToPrefs(_pinConfigs);

                    if (_onSysExSendRequest)
                    {
                        uint8_t resp[] = {0xF0, 0x7D,
                                          static_cast<uint8_t>(SysExCommand::DELETE_PIN_CONFIG),
                                          pin, 0x00, 0xF7};
                        midi_sysex_message msg;
                        msg.length = sizeof(resp);
                        memcpy(msg.data, resp, sizeof(resp));
                        _onSysExSendRequest(msg);
                    }

                    return;
                }
            }
        }

        void sendGetPinConfigResponse(const PinConfig &c)
        {
            if (!_onSysExSendRequest)
                return;

            uint8_t resp[] = {
                0xF0,
                0x7D,
                static_cast<uint8_t>(SysExCommand::GET_PIN_CONFIG_RESPONSE),
                c.pin,
                c.mode,
                c.threshold,
                c.midi_channel,
                static_cast<uint8_t>(c.midi_type) / 2,
                c.midi_type == MidiStatus::MIDI_CONTROL_CHANGE ? c.midi_cc : c.midi_note,
                c.min_midi_value,
                c.max_midi_value,
                0xF7};
            midi_sysex_message msg;
            msg.length = sizeof(resp);
            memcpy(msg.data, resp, sizeof(resp));

            _onSysExSendRequest(msg);
        }
    };
};
