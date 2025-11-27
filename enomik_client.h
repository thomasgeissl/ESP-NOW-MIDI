#include "EEPROM.h"
#include "esp_now_midi.h"
#include <esp_now.h>
#include <WiFi.h>
#include "espHelpers.h"
#include "enomik_io.h"

#ifdef HAS_USB_MIDI
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>

// Global USB MIDI objects - MUST be at file scope
Adafruit_USBD_MIDI g_usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, g_usb_midi, USBMIDI);
#endif

// EEPROM configuration
#define EEPROM_SIZE 512
#define MAC_ADDRESS_SIZE 6
#define MAC_EEPROM_ADDR 0
#define MAC_VALID_FLAG 0xAB

// Structure to store peer list
struct PeerStorage
{
    uint8_t validFlag;
    uint8_t peerCount;
    uint8_t peers[MAX_PEERS][MAC_ADDRESS_SIZE];
};

namespace enomik
{
    class Client
    {
    private:
        PeerStorage peerStorage;
        bool isInitialized;

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 3, 0)
        static void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
        {
            Serial.println(status == ESP_NOW_SEND_SUCCESS ? "MIDI Success" : "MIDI Failure");
        }
#else
        static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
        {
            Serial.println(status == ESP_NOW_SEND_SUCCESS ? "MIDI Success" : "MIDI Failure");
        }
#endif

        void loadPeersFromEEPROM()
        {
            EEPROM.get(MAC_EEPROM_ADDR, peerStorage);

            if (peerStorage.validFlag != MAC_VALID_FLAG)
            {
                peerStorage.validFlag = MAC_VALID_FLAG;
                peerStorage.peerCount = 0;
                memset(peerStorage.peers, 0, sizeof(peerStorage.peers));
                savePeersToEEPROM();
            }
            else
            {
                for (int i = 0; i < peerStorage.peerCount; i++)
                {
                    midi.addPeer(peerStorage.peers[i]);
                    Serial.print("Restored peer: ");
                    printMac(peerStorage.peers[i]);
                    Serial.println();
                }
            }
        }

        void savePeersToEEPROM()
        {
            EEPROM.put(MAC_EEPROM_ADDR, peerStorage);
            EEPROM.commit();
        }

        bool macExists(const uint8_t mac[6])
        {
            for (int i = 0; i < peerStorage.peerCount; i++)
            {
                if (memcmp(peerStorage.peers[i], mac, MAC_ADDRESS_SIZE) == 0)
                {
                    return true;
                }
            }
            return false;
        }

        void printMac(const uint8_t mac[6])
        {
            for (int i = 0; i < MAC_ADDRESS_SIZE; i++)
            {
                if (mac[i] < 16)
                    Serial.print("0");
                Serial.print(mac[i], HEX);
                if (i < MAC_ADDRESS_SIZE - 1)
                    Serial.print(":");
            }
        }

        void onSystemExclusive(uint8_t *data, unsigned int length)
        {
            Serial.println("got sysex message");
            if (length < 7)
            {
                Serial.println("sysex message too short");
                return;
            }
            uint8_t manufacturerId = data[1];
            uint8_t inputOutput = data[2];
            uint8_t pin = data[3];
            uint8_t pinMode = data[4];
            uint8_t midiType = data[5] *2;
            Serial.print("Manufacturer ID: ");
            Serial.println(manufacturerId, HEX);
            Serial.print("Input/Output: ");
            Serial.println(inputOutput);
            Serial.print("Pin: ");
            Serial.println(pin);
            Serial.print("Pin Mode: ");
            Serial.println(pinMode);
            Serial.print("MIDI Type: ");
            Serial.println(midiType);

            enomik::PinConfig config(pin, pinMode);
            io.addPinConfig(config);
        }

        static void handleNoteOnStatic(byte channel, byte note, byte velocity)
        {
            if (Client::instancePtr)
            {
                Client::instancePtr->io.onNoteOn(channel, note, velocity);
            }
        }
        
        static void handleNoteOffStatic(byte channel, byte note, byte velocity)
        {
            if (Client::instancePtr)
            {
                Client::instancePtr->io.onNoteOff(channel, note, velocity);
            }
        }
        
        static void handleControlChangeStatic(byte channel, byte control, byte value)
        {
            if (Client::instancePtr)
            {
                Client::instancePtr->io.onControlChange(channel, control, value);
            }
        }
        
        static void handleProgramChangeStatic(byte channel, byte program)
        {
            if (Client::instancePtr)
            {
                Client::instancePtr->io.onProgramChange(channel, program);
            }
        }
        
        static void handlePitchBendStatic(byte channel, int value)
        {
            if (Client::instancePtr)
            {
                Client::instancePtr->io.onPitchBend(channel, value);
            }
        }

    public:
        static Client *instancePtr;
        esp_now_midi midi;
        enomik::IO io;

#ifdef HAS_USB_MIDI
        static void handleSysExStatic(uint8_t *data, unsigned int length)
        {
            if (instancePtr)
            {
                instancePtr->onSystemExclusive(data, length);
            }
        }
#endif

        Client() : isInitialized(false)
        {
            instancePtr = this;
            peerStorage.validFlag = 0;
            peerStorage.peerCount = 0;
            memset(peerStorage.peers, 0, sizeof(peerStorage.peers));
        }

        void begin()
        {
            io.begin();
            io.setOnMIDISendRequest([this](midi_message msg)
                                    { 
                                        // Serial.println
                                        // this->midi.sendToAllPeers((uint8_t *)&msg, sizeof(msg)); 
                                    });

#ifdef HAS_USB_MIDI
            // Initialize USB MIDI using global instance
            USBMIDI.begin(MIDI_CHANNEL_OMNI);

            // Set USB descriptors
            TinyUSBDevice.setManufacturerDescriptor("grantler instruments");
            TinyUSBDevice.setProductDescriptor("enomik3000_client");

            // If already enumerated, re-enumerate
            if (TinyUSBDevice.mounted())
            {
                TinyUSBDevice.detach();
                delay(10);
                TinyUSBDevice.attach();
            }

            USBMIDI.setHandleSystemExclusive(handleSysExStatic);
            Serial.println("USB MIDI initialized");
#endif

            // Initialize EEPROM
            if (!EEPROM.begin(EEPROM_SIZE))
            {
                Serial.println("Failed to initialize EEPROM");
                return;
            }

            // Initialize WiFi
            WiFi.mode(WIFI_STA);
            Serial.print("MAC Address: ");
            Serial.println(WiFi.macAddress());

            // Initialize ESP-NOW MIDI
            midi.setup();
            
            // Set handlers for ESP-NOW
            midi.setHandleNoteOn(handleNoteOnStatic);
            midi.setHandleNoteOff(handleNoteOffStatic);
            midi.setHandleControlChange(handleControlChangeStatic);
            midi.setHandleProgramChange(handleProgramChangeStatic);
            midi.setHandlePitchBend(handlePitchBendStatic);

#ifdef HAS_USB_MIDI
            // Set handlers for USB MIDI
            USBMIDI.setHandleNoteOn(handleNoteOnStatic);
            USBMIDI.setHandleNoteOff(handleNoteOffStatic);
            USBMIDI.setHandleControlChange(handleControlChangeStatic);
            USBMIDI.setHandleProgramChange(handleProgramChangeStatic);
            USBMIDI.setHandlePitchBend(handlePitchBendStatic);
#endif

            isInitialized = true;

            Serial.print("Loaded ");
            Serial.print(peerStorage.peerCount);
            Serial.println(" peers from EEPROM");
        }

        void loop()
        {
#ifdef HAS_USB_MIDI
            USBMIDI.read();
#endif
            io.loop();
        }

        bool addPeer(const uint8_t mac[6])
        {
            if (!isInitialized)
            {
                Serial.println("Client not initialized. Call begin() first.");
                return false;
            }

            if (macExists(mac))
            {
                Serial.println("Peer already exists");
                return false;
            }

            if (peerStorage.peerCount >= MAX_PEERS)
            {
                Serial.println("Maximum peers reached");
                return false;
            }

            if (!midi.addPeer(mac))
            {
                Serial.println("Failed to add peer to ESP-NOW");
                return false;
            }

            memcpy(peerStorage.peers[peerStorage.peerCount], mac, MAC_ADDRESS_SIZE);
            peerStorage.peerCount++;

            savePeersToEEPROM();

            Serial.print("Added peer: ");
            printMac(mac);
            Serial.print(" (Total: ");
            Serial.print(peerStorage.peerCount);
            Serial.println(")");

            return true;
        }

        uint8_t *getPeer(int index)
        {
            if (index < 0 || index >= peerStorage.peerCount)
            {
                return nullptr;
            }
            return peerStorage.peers[index];
        }

        int getPeerCount()
        {
            return peerStorage.peerCount;
        }

        bool removePeer(uint8_t *mac)
        {
            for (int i = 0; i < peerStorage.peerCount; i++)
            {
                if (memcmp(peerStorage.peers[i], mac, MAC_ADDRESS_SIZE) == 0)
                {
                    for (int j = i; j < peerStorage.peerCount - 1; j++)
                    {
                        memcpy(peerStorage.peers[j], peerStorage.peers[j + 1], MAC_ADDRESS_SIZE);
                    }
                    peerStorage.peerCount--;
                    memset(peerStorage.peers[peerStorage.peerCount], 0, MAC_ADDRESS_SIZE);
                    savePeersToEEPROM();

                    Serial.print("Removed peer: ");
                    printMac(mac);
                    Serial.println();
                    return true;
                }
            }
            return false;
        }

        bool removePeer(int index)
        {
            if (index < 0 || index >= peerStorage.peerCount)
            {
                return false;
            }
            return removePeer(peerStorage.peers[index]);
        }

        void clearAllPeers()
        {
            peerStorage.peerCount = 0;
            memset(peerStorage.peers, 0, sizeof(peerStorage.peers));
            savePeersToEEPROM();
            Serial.println("All peers cleared");
        }

        void listPeers()
        {
            Serial.print("Stored peers (");
            Serial.print(peerStorage.peerCount);
            Serial.println("):");

            for (int i = 0; i < peerStorage.peerCount; i++)
            {
                Serial.print("  [");
                Serial.print(i);
                Serial.print("]: ");
                printMac(peerStorage.peers[i]);
                Serial.println();
            }

            if (peerStorage.peerCount == 0)
            {
                Serial.println("  No peers stored");
            }
        }

        String getMacString(int index)
        {
            if (index < 0 || index >= peerStorage.peerCount)
            {
                return "";
            }

            String macStr = "";
            for (int i = 0; i < MAC_ADDRESS_SIZE; i++)
            {
                if (peerStorage.peers[index][i] < 16)
                    macStr += "0";
                macStr += String(peerStorage.peers[index][i], HEX);
                if (i < MAC_ADDRESS_SIZE - 1)
                    macStr += ":";
            }
            macStr.toUpperCase();
            return macStr;
        }

        bool addPeerFromString(String macStr)
        {
            macStr.trim();
            macStr.toUpperCase();

            if (macStr.length() != 17)
            {
                Serial.println("Invalid MAC length. Expected format: XX:XX:XX:XX:XX:XX");
                return false;
            }

            uint8_t mac[6];
            int bytePositions[6] = {0, 3, 6, 9, 12, 15};

            for (int i = 0; i < 6; i++)
            {
                int pos = bytePositions[i];
                if (pos + 1 >= macStr.length())
                {
                    Serial.println("MAC string too short");
                    return false;
                }

                char char1 = macStr.charAt(pos);
                char char2 = macStr.charAt(pos + 1);

                if (!((char1 >= '0' && char1 <= '9') || (char1 >= 'A' && char1 <= 'F')) ||
                    !((char2 >= '0' && char2 <= '9') || (char2 >= 'A' && char2 <= 'F')))
                {
                    Serial.print("Invalid hex characters at position ");
                    Serial.print(pos);
                    Serial.print(": ");
                    Serial.print(char1);
                    Serial.println(char2);
                    return false;
                }

                uint8_t nibble1 = (char1 >= '0' && char1 <= '9') ? (char1 - '0') : (char1 - 'A' + 10);
                uint8_t nibble2 = (char2 >= '0' && char2 <= '9') ? (char2 - '0') : (char2 - 'A' + 10);

                mac[i] = (nibble1 << 4) | nibble2;

                if (i < 5)
                {
                    if (pos + 2 >= macStr.length() || macStr.charAt(pos + 2) != ':')
                    {
                        Serial.print("Missing colon after byte ");
                        Serial.println(i);
                        return false;
                    }
                }
            }

            Serial.print("Parsed MAC bytes: ");
            for (int i = 0; i < 6; i++)
            {
                Serial.print("0x");
                if (mac[i] < 16)
                    Serial.print("0");
                Serial.print(mac[i], HEX);
                if (i < 5)
                    Serial.print(", ");
            }
            Serial.println();

            Serial.print("Formatted MAC: ");
            for (int i = 0; i < 6; i++)
            {
                if (mac[i] < 16)
                    Serial.print("0");
                Serial.print(mac[i], HEX);
                if (i < 5)
                    Serial.print(":");
            }
            Serial.println();

            return addPeer(mac);
        }
    };

    Client *Client::instancePtr = nullptr;
};