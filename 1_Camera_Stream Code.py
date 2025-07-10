import cv2
import requests
import numpy as np

ESP32_IP = "192.168.4.1"
STREAM_URL = f"http://{ESP32_IP}:80" #/stream (if explicitly required)

def video_stream():
    print("Connecting to stream...")

    try:
        stream = requests.get(STREAM_URL, stream=True, timeout=5)
        if stream.status_code != 200:
            print(f"[ERROR] Bad status code: {stream.status_code}")
            return
    except requests.exceptions.RequestException as e:
        print("[ERROR] Could not connect to stream:", e)
        return

    print("[OK] Connected. Starting frame capture...")
    bytes_buffer = b''
    cv2.namedWindow("ESP32-CAM", cv2.WINDOW_AUTOSIZE)

    try:
        for chunk in stream.iter_content(chunk_size=1024):
            bytes_buffer += chunk
            start = bytes_buffer.find(b'\xff\xd8')
            end = bytes_buffer.find(b'\xff\xd9')

            if start != -1 and end != -1:
                jpg = bytes_buffer[start:end+2]
                bytes_buffer = bytes_buffer[end+2:]

                try:
                    frame = cv2.imdecode(np.frombuffer(jpg, dtype=np.uint8), cv2.IMREAD_COLOR)
                    if frame is None:
                        print("[WARN] Empty frame decoded.")
                        continue

                    frame = cv2.resize(frame, (800, 600))
                    frame = cv2.flip(frame, -1)
                    cv2.imshow("ESP32-CAM", frame)

                    key = cv2.waitKey(1)
                    if key == ord('q'):
                        print("Exiting stream.")
                        break
                except Exception as e:
                    print("[ERROR] Frame decode error:", e)
                    continue

    except Exception as e:
        print("[FATAL ERROR] Streaming failed:", e)

    finally:
        print("Cleaning up...")
        stream.close()
        cv2.destroyAllWindows()

if __name__ == "__main__":
    video_stream()
