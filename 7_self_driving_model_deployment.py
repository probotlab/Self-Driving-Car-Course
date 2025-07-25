import tensorflow as tf  # Add at top
gpus = tf.config.experimental.list_physical_devices('GPU')
if gpus:
    try:
        for gpu in gpus:
            tf.config.experimental.set_memory_growth(gpu, True)
    except RuntimeError as e:
        print(e)

import cv2
import numpy as np
import threading
import time
import struct
import socket
from datetime import datetime
from keras.models import load_model
from keras.optimizers import Adam
from keras.losses import MeanSquaredError
import requests
from tensorflow.keras.models import load_model

from tensorflow.python.client import device_lib
print(device_lib.list_local_devices())

# === CONFIG ===
ESP32_IP = '192.168.4.1'
CMD_PORT = 8000
STREAM_URL = f"http://{ESP32_IP}:81/stream"

# Command codes
CMD_STOP = 0
CMD_FWD = 1
CMD_TURN = 2

# Shared resources
frame_lock = threading.Lock()
latest_frame = None
original_frame = None
stop_threads = False


from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import InputLayer, Conv2D, Flatten, Dense

def create_model():
    model = Sequential([
        InputLayer(input_shape=(66, 200, 3)),
        Conv2D(24, (5,5), strides=(2,2), activation='relu'),
        Conv2D(36, (5,5), strides=(2,2), activation='relu'),
        Conv2D(48, (5,5), strides=(2,2), activation='relu'),
        Conv2D(64, (3,3), activation='relu'),
        Conv2D(64, (3,3), activation='relu'),
        Flatten(),
        Dense(100, activation='relu'),
        Dense(50, activation='relu'),
        Dense(10, activation='relu'),
        Dense(1)
    ])
    return model

# Create and load model
model = create_model()
model.load_weights(r"C:\VS Code Codes\Self Driving Car\new_non_golay_model_2.h5")
model.compile(optimizer=Adam(learning_rate=1e-4), loss=MeanSquaredError())


# TCP connection to ESP32 
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
print(f"Connecting to {ESP32_IP}:{CMD_PORT}…")
sock.connect((ESP32_IP, CMD_PORT))
print("Connected!")

last_cmd = (0, 0)  # avoid repeated commands

def send_cmd(code: int, value: int):
    global last_cmd
    if (code, value) == last_cmd:
        return
    last_cmd = (code, value)
    packet = struct.pack(">bh", code, value)
    sock.sendall(packet)

def img_preprocess(img):
    img = cv2.cvtColor(img, cv2.COLOR_BGR2YUV)  # Color space convert
    img = cv2.GaussianBlur(img, (3, 3), 0)      # Blur
    img = cv2.resize(img, (200, 66))            # Resize for model input
    img_display = img.copy()                    # Copy before normalization for display
    img = img / 255.0                # Normalize for model

    return img, img_display

def fetch_stream():
    global latest_frame, stop_threads, original_frame

    try:
        resp = requests.get(STREAM_URL, stream=True, timeout=5)
        if resp.status_code != 200:
            print(f"[ERROR] Bad status: {resp.status_code}")
            return
    except Exception as e:
        print("[ERROR] Could not connect to stream:", e)
        return

    buf = b''
    for chunk in resp.iter_content(chunk_size=1024):
        if stop_threads:
            break
        buf += chunk
        s = buf.find(b'\xff\xd8')
        e = buf.find(b'\xff\xd9')
        if s != -1 and e != -1:
            jpg = buf[s:e+2]
            buf = buf[e+2:]
            try:
                img = cv2.imdecode(np.frombuffer(jpg, np.uint8), cv2.IMREAD_COLOR)
                if img is not None:
                    flipped_img = cv2.flip(img, 0)
                    with frame_lock:
                        original_frame = flipped_img.copy()  # For prediction
                        latest_frame = cv2.resize(flipped_img, (1100, 900))  # For display
            except cv2.error:
                continue
    resp.close()

def main():
    cv2.namedWindow("Preprocessed View", cv2.WINDOW_NORMAL)
    cv2.resizeWindow("Preprocessed View", 400, 150)

    global latest_frame, stop_threads

    # Start video stream thread
    th = threading.Thread(target=fetch_stream, daemon=True)
    th.start()

    # Wait for first frame or timeout
    start_time = time.time()
    while latest_frame is None and time.time() - start_time < 5:
        time.sleep(0.1)

    if latest_frame is None:
        print("[ERROR] No frames received within timeout.")
        return
    else:
        print("[OK] Starting autonomous driving loop.")

    cv2.namedWindow("ESP32-CAM", cv2.WINDOW_NORMAL)
    cv2.resizeWindow("ESP32-CAM", 1100, 900)

    try:
        while True:
            with frame_lock:
                frame = latest_frame.copy() if latest_frame is not None else None
                original = original_frame.copy() if original_frame is not None else None

            if frame is not None and original is not None:
                cv2.imshow("ESP32-CAM", frame)

                # Preprocess image for model
                processed, display_img = img_preprocess(original)

                image_input = np.array([processed])

                # Show preprocessed view
                cv2.imshow("Preprocessed View", display_img)

            image_input = np.array([processed])

            # # Predict steering angle
            prediction = model.predict(image_input, verbose=0)
            steering_angle = float(prediction[0])
            print(f"[DEBUG] Model prediction: {steering_angle:.4f}")

            # steering_angle = -steering_angle

            # Define threshold
            threshold = 16

            # Convert steering angle to a value in range [-100, 100]
            steering_value = int(steering_angle * 100)

            # Apply threshold logic
            if steering_value <= -threshold:
                send_cmd(CMD_TURN, -threshold)
                print(f"[AUTO] Steering {steering_value} → sent {-threshold}")
            elif -threshold < steering_value < threshold:
                send_cmd(CMD_TURN, 0)
                print(f"[AUTO] Steering {steering_value} → sent 0")
            else:  # steering_value >= threshold
                send_cmd(CMD_TURN, threshold)
                print(f"[AUTO] Steering {steering_value} → sent {threshold}")


            # Quit on 'q' key
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

            # time.sleep(0.02)

    except KeyboardInterrupt:
        print("Exiting...")

    finally:
        stop_threads = True
        th.join(timeout=1)
        cv2.destroyAllWindows()
        sock.close()

if __name__ == "__main__":
    main()
