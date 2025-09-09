#include "EEPROM.h"
#include "esp_now_midi.h"
#include <esp_now.h>
#include <WiFi.h>

// EEPROM configuration
#define EEPROM_SIZE 512     // Total EEPROM size (can be 1-4096 bytes)
#define MAC_ADDRESS_SIZE 6  // MAC address is 6 bytes
#define MAC_EEPROM_ADDR 0   // Starting address for MAC storage
#define MAC_VALID_FLAG 0xAB // Flag to check if MAC is valid

// Structure to store peer list
struct PeerStorage
{
    uint8_t validFlag;                          // Validation flag
    uint8_t peerCount;                          // Number of stored peers
    uint8_t peers[MAX_PEERS][MAC_ADDRESS_SIZE]; // Array of MAC addresses
};

namespace enomik
{
    class Client
    {
    private:
        PeerStorage peerStorage;
        bool isInitialized;

// ESP-NOW callback for backwards compatibility
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

            // If no valid data, initialize empty storage
            if (peerStorage.validFlag != MAC_VALID_FLAG)
            {
                peerStorage.validFlag = MAC_VALID_FLAG;
                peerStorage.peerCount = 0;
                memset(peerStorage.peers, 0, sizeof(peerStorage.peers));
                savePeersToEEPROM();
            }
            else
            {
                // If we have valid data, add all stored peers to ESP-NOW
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

        bool macExists(const uint8_t mac[6] )
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

        void printMac(const uint8_t mac[6] )  // Fixed: changed to void
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

    public:
        esp_now_midi midi;

        Client() : isInitialized(false) 
        {
            // Initialize peerStorage to safe defaults
            peerStorage.validFlag = 0;
            peerStorage.peerCount = 0;
            memset(peerStorage.peers, 0, sizeof(peerStorage.peers));
        }

        void begin()
        {
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

            // Load existing peers from EEPROM - FIXED: uncommented
            // loadPeersFromEEPROM();

            isInitialized = true;

            Serial.print("Loaded ");
            Serial.print(peerStorage.peerCount);
            Serial.println(" peers from EEPROM");
        }

        bool addPeer(const uint8_t mac[6] )
        {
            // Check if client is initialized
            if (!isInitialized) {
                Serial.println("Client not initialized. Call begin() first.");
                return false;
            }

            // Check if peer already exists
            if (macExists(mac))
            {
                Serial.println("Peer already exists");
                return false;
            }

            // Check if we have space for more peers
            if (peerStorage.peerCount >= MAX_PEERS)
            {
                Serial.println("Maximum peers reached");
                return false;
            }

            // Add peer to ESP-NOW midi system first
            if (!midi.addPeer(mac))
            {
                Serial.println("Failed to add peer to ESP-NOW");
                return false;
            }

            // Add peer to storage
            memcpy(peerStorage.peers[peerStorage.peerCount], mac, MAC_ADDRESS_SIZE);
            peerStorage.peerCount++;

            // Save to EEPROM
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

                    // Shift remaining peers down
                    for (int j = i; j < peerStorage.peerCount - 1; j++)
                    {
                        memcpy(peerStorage.peers[j], peerStorage.peers[j + 1], MAC_ADDRESS_SIZE);
                    }
                    peerStorage.peerCount--;

                    // Clear the last slot
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

        // Helper function to get MAC as string for web interface
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

        // Helper function to add peer from string (for web interface)
        bool addPeerFromString(String macStr)
        {
            // Remove any spaces and convert to uppercase
            macStr.trim();
            macStr.toUpperCase();

            if (macStr.length() != 17)
            {
                Serial.println("Invalid MAC length. Expected format: XX:XX:XX:XX:XX:XX");
                return false;
            }

            uint8_t mac[6];

            // Parse each byte manually with explicit position checking
            int bytePositions[6] = {0, 3, 6, 9, 12, 15}; // Start position of each byte

            for (int i = 0; i < 6; i++)
            {
                // Check that we have two hex digits at this position
                int pos = bytePositions[i];
                if (pos + 1 >= macStr.length())
                {
                    Serial.println("MAC string too short");
                    return false;
                }

                // Get the two characters for this byte
                char char1 = macStr.charAt(pos);
                char char2 = macStr.charAt(pos + 1);

                // Validate hex characters
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

                // Convert hex characters to byte value
                uint8_t nibble1 = (char1 >= '0' && char1 <= '9') ? (char1 - '0') : (char1 - 'A' + 10);
                uint8_t nibble2 = (char2 >= '0' && char2 <= '9') ? (char2 - '0') : (char2 - 'A' + 10);

                mac[i] = (nibble1 << 4) | nibble2;

                // Check for colon separator (except after last byte)
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

            // Debug: Print parsed MAC with explicit byte values
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

            // Debug: Print formatted MAC
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
};