import serial
import time

# Open the serial port with the updated parameters
ser = serial.Serial(
    port='/dev/serial0',      # Replace with your serial port (on Raspberry Pi, typically /dev/serial0 or /dev/ttyS0)
    baudrate=250125,           # Set baud rate to 98,015 bps (from your calculation)
    bytesize=8,               # 8 data bits
    parity=serial.PARITY_NONE,  # No parity
    stopbits=serial.STOPBITS_ONE,  # 1 stop bit (assuming)
    timeout=1                 # Set timeout for reading (1 second)
)

# Print a message indicating the script is running
print("Reading data from /dev/serial0 at 98,015 bps. Press Ctrl+C to stop.")

def print_boundary():
    print("\n" + "="*50 + "\nEnd of Data Block\n" + "="*50 + "\n")

try:
    while True:
        data_block = b""  # Initialize an empty byte string for data
        start_time = time.time()

        while True:
            # Read data from the serial port if available
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)  # Read the data available in the buffer
                data_block += data  # Append the data to the data block

                # Print the received data in hexadecimal format
                print(f"Received data (hex): {data.hex()}")

                start_time = time.time()  # Reset the start time since we received data

            # If no data received for 1 second, stop and print the boundary
            elif time.time() - start_time > 1:
                # Only print the boundary after the first data block
                if data_block:
                    print_boundary()
                    data_block = b""  # Reset the data block for the next round
                    break

except KeyboardInterrupt:
    # Handle Ctrl+C to stop the script gracefully
    print("\nScript interrupted. Exiting...")

finally:
    # Close the serial port when the script exits
    ser.close()
