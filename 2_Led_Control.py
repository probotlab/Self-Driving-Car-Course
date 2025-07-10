import requests

# ESP32-CAM configuration
ESP32_IP = "192.168.4.1"  # Replace with your ESP32 IP address
CONTROL_URL = f"http://{ESP32_IP}:81"        # Control URL (Port 81)

# Function for sending control commands
def control_commands():
    print("Control Commands:")
    print("Press 'o' to turn LED ON.")
    print("Press 'p' to turn LED OFF.")
    print("Press 'q' to quit.")

    while True:
        key = input("Enter a command: ").strip().lower()

        if key == 'o':  # Turn LED ON
            try:
                response = requests.post(f"{CONTROL_URL}/on")
                print("LED ON Command Sent:", response.status_code)
            except requests.exceptions.RequestException as e:
                print("Error sending LED ON command:", e)

        elif key == 'p':  # Turn LED OFF
            try:
                response = requests.post(f"{CONTROL_URL}/off")
                print("LED OFF Command Sent:", response.status_code)
            except requests.exceptions.RequestException as e:
                print("Error sending LED OFF command:", e)

        elif key == 'q':  # Quit the program
            print("Exiting control commands...")
            break

        else:
            print("Invalid command. Use 'o', 'p', or 'q'.")

if __name__ == "__main__":
    control_commands()
