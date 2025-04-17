import serial
import RPi.GPIO as GPIO
import time

#green wire = lower button
#blue = upper button
# Define GPIO pins (use BCM pin numbering)
GPIO_PIN_1 = 17  # GPIO pin 17
GPIO_PIN_2 = 27  # GPIO pin 27

# Define the serial port and settings
serial_port = '/dev/serial0'
baud_rate = 250000
timeout = 1  # Timeout in seconds

# Hex values to match (valid patterns)
hex_values = {
    "Left": ("Pressed", "f5550003201402"),
    "Right": ("Pressed", "f5550003201401"),
    "Circle": ("Lit to Not Lit", "f5550003201000"),
    "Circle": ("Not Lit to Lit", "f5550003201001"),
}

def setup_gpio():
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(GPIO_PIN_1, GPIO.IN, pull_up_down=GPIO.PUD_UP)
    GPIO.setup(GPIO_PIN_2, GPIO.IN, pull_up_down=GPIO.PUD_UP)

def read_gpio():
    pin_1_state = GPIO.input(GPIO_PIN_1)
    pin_2_state = GPIO.input(GPIO_PIN_2)
    return pin_1_state, pin_2_state

def handle_midi_message(chunk):
    if len(chunk) >= 6:
        try:
            status_byte = int(chunk[0:2], 16)
            note_byte = int(chunk[2:4], 16)
            velocity_byte = int(chunk[4:6], 16)
        except ValueError:
            return 0  # Invalid hex

        if status_byte == 0x80:
            print(f"MIDI Note Off: Note {note_byte} Velocity {velocity_byte}")
            return 6
        elif status_byte == 0x90:
            print(f"MIDI Note On: Note {note_byte} Velocity {velocity_byte}")
            return 6
    return 0

def process_buffer(data_buffer):
    index = 0
    results = []

    while index < len(data_buffer):
        remaining = data_buffer[index:]

        # Try MIDI first
        midi_length = handle_midi_message(remaining)
        if midi_length:
            index += midi_length
            continue

        matched = False
        for label, (status, hex_val) in hex_values.items():
            hex_clean = hex_val.lower()
            hex_len = len(hex_clean)
            if remaining.startswith(hex_clean):
                print(f"Found match: {label} {status}")
                index += hex_len
                matched = True
                break

        if not matched:
            # No match, skip one character (half byte) to avoid full reset
            index += 2

    return data_buffer[index:]  # Return any leftover (incomplete) data

def main():
    setup_gpio()
    try:
        ser = serial.Serial(serial_port, baudrate=baud_rate, timeout=timeout)
        print(f"Connected to {serial_port} at {baud_rate}bps")
    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
        return

    data_buffer = ""
    old_pin1 = 0
    old_pin2 = 0
    while True:
        try:
            data = ser.read(128)
            if data:
                data_buffer += data.hex()
                data_buffer = process_buffer(data_buffer)

            # Read GPIO states (optional)
            pin_1_state, pin_2_state = read_gpio()
            if old_pin1 != pin_1_state:
                print("pin 1 is now " + str(pin_1_state))
            if old_pin2 != pin_2_state:
                print("pin 2 is now " + str(pin_2_state))
            old_pin1 = pin_1_state;

            old_pin2 = pin_2_state;
            time.sleep(0.05)


        except KeyboardInterrupt:
            print("Interrupted. Cleaning up...")
            break
        except Exception as e:
            print(f"Error: {e}")

    ser.close()
    GPIO.cleanup()

if __name__ == "__main__":
    main()
