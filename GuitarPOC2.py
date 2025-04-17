import serial
import time

# Predefined button mapping
button_map = {
    'None Off': 'aa55000a22010000000000',
    '11 On': 'aa55000a22010000000400',
    '12 On': 'aa55000a22010000000200',
    '13 On': 'aa55000a22010000000100',
    '21 On': 'aa55000a22010000002000',
    '22 On': 'aa55000a22010000001000',
    '23 On': 'aa55000a22010000000800',
    '31 On': 'aa55000a22010000010000',
    '32 On': 'aa55000a22010000008000',
    '33 On': 'aa55000a22010000004000',
    '41 On': 'aa55000a22010000080000',
    '42 On': 'aa55000a22010000040000',
    '51 On': 'aa55000a22010000400000',
    '52 On': 'aa55000a22010000200000',
    '53 On': 'aa55000a22010000100000',
    '61 On': 'aa55000a22010002000000',
    '62 On': 'aa55000a22010001000000',
    '63 On': 'aa55000a22010000800000',
    '71 On': 'aa55000a22010010000000',
    '72 On': 'aa55000a22010008000000',
    '73 On': 'aa55000a22010004000000',
    '81 On': 'aa55000a22010080000000',
    '82 On': 'aa55000a22010040000000',
    '83 On': 'aa55000a22010020000000',
    '91 On': 'aa55000a22010400000000',
    '92 On': 'aa55000a22010200000000',
    '93 On': 'aa55000a22010100000000',
    'Rocker Up': 'aa550003220200d8',
    'Rocker Down': 'aa5500032202ffd9',
}

# Reverse map for fast lookup
hex_to_label = {v.lower(): k for k, v in button_map.items()}
expected_lengths = set(len(v) for v in button_map.values())

def process_partial_data(partial_data):
    results = []
    index = 0
    while index + 4 <= len(partial_data):
        # Look for header
        if partial_data[index:index+4] == 'aa55':
            matched = False
            for length in sorted(expected_lengths, reverse=True):
                chunk = partial_data[index:index+length]
                if chunk.lower() in hex_to_label:
                    label = hex_to_label[chunk.lower()]
                    print(f"Processing chunk: {chunk}")
                    print(f"Match found: {label}")
                    results.append(label)
                    index += length
                    matched = True
                    break
            if not matched:
                index += 2  # skip ahead to look for next possible 'aa55'
        else:
            index += 2  # avoid breaking mid-byte
    return results

def ensure_starting_with_aa55(data):
    start_idx = data.find('aa55')
    return data[start_idx:] if start_idx != -1 else ''

def read_serial():
    ser = serial.Serial('/dev/serial0', baudrate=250000, timeout=1)
    partial_data = ""
    while True:
        try:
            data = ser.read(128)
            if data:
                hex_data = data.hex()
                hex_data = ensure_starting_with_aa55(hex_data)
                if hex_data:
                    partial_data += hex_data
                    print(f"Partial Data: {partial_data}")
                    results = process_partial_data(partial_data)

                    # Keep any trailing data after the last 'aa55'
                    last_aa55 = partial_data.rfind('aa55')
                    partial_data = partial_data[last_aa55:] if last_aa55 != -1 else ""

            else:
                if partial_data:
                    print("No more data received, checking partial data.")
                    partial_data = ""

        except Exception as e:
            print(f"Error: {e}")
            time.sleep(1)

if __name__ == "__main__":
    read_serial()
