import espnow
import struct
import wifi

class esp_now_midi:
    def __init__(self, broadcast_address):
        """
        Initialize the ESP-NOW MIDI class with the broadcast address of the receiver.
        :param broadcast_address: MAC address of the ESP-NOW receiver (e.g., Arduino).
        """
        self.broadcast_address = broadcast_address
        self.e = espnow.ESPNow()
        self.e.active(True)
        self.e.add_peer(broadcast_address)

    def send_note_on(self, note, velocity, channel):
        """
        Send a MIDI Note On message.
        :param note: MIDI note number (0-127)
        :param velocity: Note velocity (0-127)
        :param channel: MIDI channel (1-16)
        """
        status = 0x90 | (channel & 0x0F)  # MIDI_NOTE_ON with channel
        message = struct.pack('BBBB', channel, status, note, velocity)
        self.e.send(self.broadcast_address, message)

    def send_note_off(self, note, velocity, channel):
        """
        Send a MIDI Note Off message.
        :param note: MIDI note number (0-127)
        :param velocity: Note velocity (0-127)
        :param channel: MIDI channel (1-16)
        """
        status = 0x80 | (channel & 0x0F)  # MIDI_NOTE_OFF with channel
        message = struct.pack('BBBB', channel, status, note, velocity)
        self.e.send(self.broadcast_address, message)

    def send_control_change(self, control, value, channel):
        """
        Send a MIDI Control Change message.
        :param control: MIDI control number (0-127)
        :param value: Control value (0-127)
        :param channel: MIDI channel (1-16)
        """
        status = 0xB0 | (channel & 0x0F)  # MIDI_CONTROL_CHANGE
        message = struct.pack('BBBB', channel, status, control, value)
        self.e.send(self.broadcast_address, message)

    def send_program_change(self, program, channel):
        """
        Send a MIDI Program Change message.
        :param program: Program number (0-127)
        :param channel: MIDI channel (1-16)
        """
        status = 0xC0 | (channel & 0x0F)  # MIDI_PROGRAM_CHANGE
        message = struct.pack('BBB', channel, status, program)
        self.e.send(self.broadcast_address, message)

    def send_aftertouch(self, pressure, channel):
        """
        Send a MIDI Aftertouch (Channel Pressure) message.
        :param pressure: Aftertouch pressure value (0-127)
        :param channel: MIDI channel (1-16)
        """
        status = 0xD0 | (channel & 0x0F)  # MIDI_AFTERTOUCH
        message = struct.pack('BBB', channel, status, pressure)
        self.e.send(self.broadcast_address, message)

    def send_pitch_bend(self, value, channel):
        """
        Send a MIDI Pitch Bend message.
        :param value: Pitch bend value (0-16383, 8192 is no pitch bend)
        :param channel: MIDI channel (1-16)
        """
        status = 0xE0 | (channel & 0x0F)  # MIDI_PITCH_BEND
        value = value & 0x3FFF  # Ensure value is within 14-bit range (0-16383)
        first_byte = value & 0x7F  # LSB: lower 7 bits
        second_byte = (value >> 7) & 0x7F  # MSB: upper 7 bits
        message = struct.pack('BBBB', channel, status, first_byte, second_byte)
        self.e.send(self.broadcast_address, message)
