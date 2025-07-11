import cv2
import requests
import numpy as np
import threading

# ESP32-CAM configuration
ESP32_IP = "192.168.4.1"  # Replace with your ESP32 IP address
STREAM_URL = f"http://{ESP32_IP}:80/stream"  # Video Stream URL (Port 80)
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

# Function for streaming video
def video_stream():
    print("Starting video stream...")
    try:
        stream = requests.get(STREAM_URL, stream=True)
        if stream.status_code != 200:
            print(f"Error: Failed to connect to the stream at {STREAM_URL}")
            return
    except requests.exceptions.RequestException as e:
        print(f"Error: {e}")
        return

    bytes_buffer = b''

    print("Press 'Ctrl+C' to quit the video stream.")

    try:
        for chunk in stream.iter_content(chunk_size=1024):
            bytes_buffer += chunk
            start = bytes_buffer.find(b'\xff\xd8')  # JPEG start
            end = bytes_buffer.find(b'\xff\xd9')    # JPEG end

            if start != -1 and end != -1:
                jpg = bytes_buffer[start:end + 2]
                bytes_buffer = bytes_buffer[end + 2:]

                try:
                    img = cv2.imdecode(np.frombuffer(jpg, dtype=np.uint8), cv2.IMREAD_COLOR)
                    if img is None:
                        raise ValueError("Empty frame received.")
                except Exception as e:
                    continue  # Skip invalid frame

                img = cv2.resize(img, (800, 600))
                img = cv2.flip(img, -1)
                cv2.imshow('ESP32-CAM Stream', img)

                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
    except Exception as e:
        print(f"Error: {e}")

    stream.close()
    cv2.destroyAllWindows()

# Threading to run video stream and control commands simultaneously
if __name__ == "__main__":
    video_thread = threading.Thread(target=video_stream)
    control_thread = threading.Thread(target=control_commands)

    video_thread.start()
    control_thread.start()

    video_thread.join()
    control_thread.join()

    print("Program exited.")
