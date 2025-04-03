#define HAS_DISPLAY 1
#define MAX_HISTORY 5 // Maximum number of messages to store

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; or run an i2c scanner
#define UPDATE_DISPLAY_INTERVAL 64

// DO NOT CHANGE BELOW THIS LINE
#define MAC_ADDR_LEN 6

#define DONGLE_MAX_PEERS 20