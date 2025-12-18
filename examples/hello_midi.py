import time
import network
import espnow
from esp_now_midi import ESPNowMidi

peer_mac = b'\x84\xF7\x03\xF2\x54\x62'  # Dongle MAC
channel = 1

wlan = network.WLAN(network.STA_IF)  # station interface
wlan.active(True)                     # activate WiFi
midi = ESPNowMidi()
midi.setup()
midi.add_peer(peer_mac)

# Show peers for verification
midi.print_peers()

# ---------------------------
# Main loop - sending MIDI messages
# ---------------------------

while True:
    # Note On
    if not midi.send_note_on(60, 127, channel):
        print("Error sending Note On")
    time.sleep(0.1)

    # Note Off
    midi.send_note_off(60, 0, channel)
    time.sleep(0.1)

    # Control Change
    midi.send_control_change(1, 127, channel)
    time.sleep(0.1)
    midi.send_control_change(1, 0, channel)
    time.sleep(0.1)

    # Pitch Bend
    midi.send_pitch_bend(-8192, channel)
    time.sleep(0.1)
    midi.send_pitch_bend(0, channel)
    time.sleep(0.1)
    midi.send_pitch_bend(8191, channel)
    
    # Wait before repeating
    time.sleep(2)
