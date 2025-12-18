

"""
ESP-NOW MIDI Library for CircuitPython
Handles MIDI message transmission over ESP-NOW protocol
"""

import espnow
import struct
import time

# MIDI Status Bytes
MIDI_NOTE_OFF = 0x80
MIDI_NOTE_ON = 0x90
MIDI_POLY_AFTERTOUCH = 0xA0
MIDI_CONTROL_CHANGE = 0xB0
MIDI_PROGRAM_CHANGE = 0xC0
MIDI_AFTERTOUCH = 0xD0
MIDI_PITCH_BEND = 0xE0
MIDI_START = 0xFA
MIDI_CONTINUE = 0xFB
MIDI_STOP = 0xFC
MIDI_TIME_CLOCK = 0xF8
MIDI_SONG_POS_POINTER = 0xF2
MIDI_SONG_SELECT = 0xF3

MAX_PEERS = 20


class MidiMessage:
    """Structure to hold MIDI message data"""
    def __init__(self, status=0, channel=0, first_byte=0, second_byte=0):
        self.channel = channel
        self.status = status
        self.first_byte = first_byte
        self.second_byte = second_byte
    
 
    def to_bytes(self):
        return bytes((
            self.channel & 0xFF,
            self.status & 0xFF,
            self.first_byte & 0x7F,
            self.second_byte & 0x7F
        ))



class ESPNowMidi:
    """ESP-NOW MIDI communication class"""
    
    def __init__(self):
        self.e = espnow.ESPNow()
        self.peers = []
        self._max_peers = MAX_PEERS
    
    def setup(self):
        """Initialize ESP-NOW"""
        self.e.active(True)
        print("ESP-NOW MIDI initialized")
    
    def add_peer(self, mac_address):
        """
        Add a peer by MAC address
        
        Args:
            mac_address: bytes object of length 6 (e.g., b'\xff\xff\xff\xff\xff\xff')
                        or list/tuple of 6 integers
        
        Returns:
            bool: True if peer was added successfully, False otherwise
        """
        if len(self.peers) >= self._max_peers:
            print("Maximum number of peers (" + str(self._max_peers) + ") reached")
            return False
        
        # Convert to bytes if needed
        if isinstance(mac_address, (list, tuple)):
            mac_address = bytes(mac_address)
        
        if len(mac_address) != 6:
            print("MAC address must be 6 bytes")
            return False
        
        # Check if peer already exists
        if mac_address in self.peers:
            print("Peer already registered")
            return False
        
        try:
            self.e.add_peer(mac_address)
            self.peers.append(mac_address)
            mac_str = ':'.join(['%02x' % b for b in mac_address])
            print("Peer added: " + mac_str)
            print("Total peers: " + str(len(self.peers)))
            return True
        except Exception as ex:
            print("Failed to add peer: " + str(ex))
            return False
    
    def remove_peer(self, mac_address):
        """Remove a peer by MAC address"""
        if isinstance(mac_address, (list, tuple)):
            mac_address = bytes(mac_address)
        
        if mac_address in self.peers:
            try:
                self.e.del_peer(mac_address)
                self.peers.remove(mac_address)
                mac_str = ':'.join(['%02x' % b for b in mac_address])
                print("Peer removed: " + mac_str)
                return True
            except Exception as ex:
                print("Failed to remove peer: " + str(ex))
                return False
        return False
    
    def get_peers_count(self):
        """Get the number of registered peers"""
        return len(self.peers)
    
    def print_peers(self):
        """Print all registered peers"""
        print("=== Registered ESP-NOW Peers ===")
        for i, peer in enumerate(self.peers):
            mac_str = ':'.join(['%02x' % b for b in peer])
            print("Peer " + str(i) + ": " + mac_str)
        print("Total: " + str(len(self.peers)) + " peers")
        print("================================")
    
    def clear_peers(self):
        """Remove all peers"""
        for peer in self.peers[:]:  # Copy list to avoid modification during iteration
            self.remove_peer(peer)
    
    def send_to_all_peers(self, data):
        """
        Send data to all registered peers
        
        Args:
            data: bytes object to send
        
        Returns:
            bool: True if sent to at least one peer successfully
        """
        if not self.peers:
            print("No peers registered!")
            return False
        
        success = False
        for peer in self.peers:
            try:
                self.e.send(peer, data)
                success = True
            except Exception as ex:
                mac_str = ':'.join(['%02x' % b for b in peer])
                print("Failed to send to " + mac_str + ": " + str(ex))
        
        return success
    
    def send_note_on(self, note, velocity, channel=1):
        """
        Send MIDI Note On message
        
        Args:
            note: MIDI note number (0-127)
            velocity: Note velocity (0-127)
            channel: MIDI channel value (default=1, matches C++ behavior)
        """
        message = MidiMessage(MIDI_NOTE_ON, channel, note, velocity)
        return self.send_to_all_peers(message.to_bytes())
    
    def send_note_off(self, note, velocity=0, channel=1):
        """
        Send MIDI Note Off message
        
        Args:
            note: MIDI note number (0-127)
            velocity: Release velocity (0-127, default=0)
            channel: MIDI channel value (default=1, matches C++ behavior)
        """
        message = MidiMessage(MIDI_NOTE_OFF, channel, note, velocity)
        return self.send_to_all_peers(message.to_bytes())
    
    def send_control_change(self, control, value, channel=1):
        """
        Send MIDI Control Change message
        
        Args:
            control: Control number (0-127)
            value: Control value (0-127)
            channel: MIDI channel value (default=1, matches C++ behavior)
        """
        message = MidiMessage(MIDI_CONTROL_CHANGE, channel, control, value)
        return self.send_to_all_peers(message.to_bytes())
    
    def send_program_change(self, program, channel=1):
        """
        Send MIDI Program Change message
        
        Args:
            program: Program number (0-127)
            channel: MIDI channel value (default=1, matches C++ behavior)
        """
        message = MidiMessage(MIDI_PROGRAM_CHANGE, channel, program, 0)
        return self.send_to_all_peers(message.to_bytes())
    
    def send_aftertouch(self, pressure, channel=1):
        """
        Send MIDI Channel Aftertouch (pressure) message
        
        Args:
            pressure: Pressure value (0-127)
            channel: MIDI channel value (default=1, matches C++ behavior)
        """
        message = MidiMessage(MIDI_AFTERTOUCH, channel, pressure, 0)
        return self.send_to_all_peers(message.to_bytes())
    
    def send_aftertouch_poly(self, note, pressure, channel=1):
        """
        Send MIDI Polyphonic Aftertouch message
        
        Args:
            note: MIDI note number (0-127)
            pressure: Pressure value (0-127)
            channel: MIDI channel value (default=1, matches C++ behavior)
        """
        message = MidiMessage(MIDI_POLY_AFTERTOUCH, channel, note, pressure)
        return self.send_to_all_peers(message.to_bytes())
    
    def send_pitch_bend_raw(self, value, channel=1):
        """
        Send MIDI Pitch Bend message with raw value (0-16383)
        
        Args:
            value: int, 0-16383 (14-bit value, 8192 is center)
            channel: MIDI channel value (default=1, matches C++ behavior)
        """
        value = value & 0x3FFF  # Clamp to 14 bits
        lsb = value & 0x7F
        msb = (value >> 7) & 0x7F
        message = MidiMessage(MIDI_PITCH_BEND, channel, lsb, msb)
        return self.send_to_all_peers(message.to_bytes())
    
    def send_pitch_bend(self, value, channel=1):
        """
        Send MIDI Pitch Bend message with signed value (-8192 to +8191)
        
        Args:
            value: int, -8192 to +8191 (0 is center)
            channel: MIDI channel value (default=1, matches C++ behavior)
        """
        # Clamp to signed 14-bit range
        value = max(-8192, min(8191, value))
        # Convert signed to unsigned (0-16383)
        raw_value = value + 8192
        return self.send_pitch_bend_raw(raw_value, channel)
    
    def send_start(self):
        """Send MIDI Start (realtime) message"""
        message = MidiMessage(MIDI_START, 0, 0, 0)
        return self.send_to_all_peers(message.to_bytes())
    
    def send_stop(self):
        """Send MIDI Stop (realtime) message"""
        message = MidiMessage(MIDI_STOP, 0, 0, 0)
        return self.send_to_all_peers(message.to_bytes())
    
    def send_continue(self):
        """Send MIDI Continue (realtime) message"""
        message = MidiMessage(MIDI_CONTINUE, 0, 0, 0)
        return self.send_to_all_peers(message.to_bytes())
    
    def send_clock(self):
        """Send MIDI Clock (timing) message"""
        message = MidiMessage(MIDI_TIME_CLOCK, 0, 0, 0)
        return self.send_to_all_peers(message.to_bytes())
    
    def send_song_position(self, value):
        """
        Send MIDI Song Position Pointer message
        
        Args:
            value: int, 0-16383 (14-bit value)
        """
        value = value & 0x3FFF  # Clamp to 14 bits
        lsb = value & 0x7F
        msb = (value >> 7) & 0x7F
        message = MidiMessage(MIDI_SONG_POS_POINTER, 0, lsb, msb)
        return self.send_to_all_peers(message.to_bytes())
    
    def send_song_select(self, value):
        """
        Send MIDI Song Select message
        
        Args:
            value: int, 0-127
        """
        value = value & 0x7F  # Clamp to 7 bits
        message = MidiMessage(MIDI_SONG_SELECT, 0, value, 0)
        return self.send_to_all_peers(message.to_bytes())
    
    def deinit(self):
        """Clean up ESP-NOW resources"""
        self.clear_peers()
        self.e.active(False)
