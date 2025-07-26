import pygame  
import socket  
import time  
import struct  
import cv2  
import requests  
import numpy as np  
import os  
import threading  
from datetime import datetime  # For timestamp in filenames  
  
# === CONFIGURE ===  
ESP32_IP = '192.168.4.1'  # ESP32 IP  
CMD_PORT = 8000              # Command port  
STREAM_URL = f"http://{ESP32_IP}:81/stream"  # Camera stream URL  
  
# Command codes  
CMD_STOP = 0  
CMD_FWD = 1  
CMD_TURN = 2  
  
# Shared resources for video stream  
frame_lock = threading.Lock()  
latest_frame = None  
stop_threads = False  
  
# Folder for saving images  
SAVE_FOLDER = r"C:\VS Code Codes\Self Driving Car\tryout_backward_1"   
os.makedirs(SAVE_FOLDER, exist_ok=True)  # Ensure folder exists  
  
# Initialize joystick  
pygame.init()  
pygame.joystick.init()  
if pygame.joystick.get_count() == 0:  
    print("No joystick detected!")  
    exit()  
joy = pygame.joystick.Joystick(0)  
joy.init()  
print("Using joystick:", joy.get_name())  
  
# Open TCP connection to ESP32 for commands  
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)  
sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)  # Disable Nagle's algorithm  
print(f"Connecting to {ESP32_IP}:{CMD_PORT}…")  
sock.connect((ESP32_IP, CMD_PORT))  
print("Connected!")  
  
# Function to send binary commands  
last_cmd = (0, 0)  # (cmd_code, value)  
  
  
def send_cmd(code: int, value: int):  
    """Send command to ESP32."""  
    global last_cmd  
    if (code, value) == last_cmd:  
        return  # Skip duplicate commands  
    last_cmd = (code, value)  
    packet = struct.pack(">bh", code, value)  # 1 byte + 2-byte signed int  
    sock.sendall(packet)  
  
  
# Function to fetch video stream  
image_counter = 0  # Counter for valid frames  
  
  
def fetch_stream():  
    global latest_frame, stop_threads, image_counter, original_frame  # Add original_frame
  
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
            jpg = buf[s:e + 2]  
            buf = buf[e + 2:]  
            try:  
                img = cv2.imdecode(np.frombuffer(jpg, np.uint8), cv2.IMREAD_COLOR)  
                if img is not None:  
                    image_counter += 1  
                    flipped_img = cv2.flip(img, 0)
  
                    with frame_lock:  
                        original_frame = flipped_img.copy()  # Original 320x240 frame  
                        latest_frame = cv2.resize(flipped_img, (800, 600))  # For display  
            except cv2.error as err:  
                continue
    resp.close()

  
  
# Main function  
def main():  
    global latest_frame, stop_threads, image_counter  
  
    # Start video fetching thread  
    th = threading.Thread(target=fetch_stream, daemon=True)  
    th.start()  
  
    # Wait for the first frame to arrive  
    start_time = time.time()  
    while latest_frame is None and time.time() - start_time < 5:  
        time.sleep(0.1)  
    if latest_frame is None:  
        print("[ERROR] No frames received within timeout.")  
    else:  
        print("[OK] Starting display and control loop.")  
  
    # OpenCV window for video stream  
    cv2.namedWindow("ESP32-CAM", cv2.WINDOW_AUTOSIZE)  
  
    try:  
        while True:  
            # Handle video stream  
            with frame_lock:  
                frame = latest_frame.copy() if latest_frame is not None else None  
  
            if frame is not None:  
                cv2.imshow("ESP32-CAM", frame)  
  
            # Check for quit key  
            if cv2.waitKey(1) & 0xFF == ord('q'):  
                break  
  
            # Handle joystick input  
            pygame.event.pump()  
            r2 = joy.get_axis(5)  # PS4 R2: typically axis 5  
            deadman = r2 > 0.5  
  
            x = -joy.get_axis(2)  # Invert the right stick X-axis (left/right)  
            y = -joy.get_axis(3)  # Invert the right stick Y-axis (forward/backward, already inverted)  
  
            # Determine steering value  
            if abs(y) > abs(x):  # Forward/backward  
                speed = int(min(abs(y), 1.0) * 100)  
                steering_value = speed if y > 0 else -speed  
            else:  # Left/right turn  
                speed = int(min(abs(x), 1.0) * 100)  
                steering_value = speed if x > 0 else -speed  
  
            if not deadman:  
                send_cmd(CMD_STOP, 0)  
                # print("STOP")  
            else:  
                # Send steering command  
                if abs(y) > abs(x):  # Forward/backward  
                    send_cmd(CMD_FWD, steering_value)  
                else:  # Left/right turn  
                    send_cmd(CMD_TURN, steering_value)  
  
                # Save every 5th frame with the steering value and timestamp  
                # if image_counter % 5 == 0:
                timestamp = datetime.now().strftime("%m-%d-%Y_%H-%M-%S-%f")[:-3]
                img_name = f"{image_counter}_{steering_value}_{timestamp}.jpg"
              
                with frame_lock:
                    if original_frame is not None:
                        cv2.imwrite(os.path.join(SAVE_FOLDER, img_name), original_frame)
                        print(f"[INFO] Saved image: {img_name}")
                            
            time.sleep(0.02)  # 50 Hz control loop  
    except KeyboardInterrupt:  
        print("Exiting...")  
    finally:  
        stop_threads = True  
        th.join(timeout=1)  
        cv2.destroyAllWindows()  
        pygame.quit()  
        sock.close()  
  
  
if __name__ == "__main__":  
    main() 
