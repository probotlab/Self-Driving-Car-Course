import pygame
import socket
import struct
import time

# ——— CONFIG ———
ESP32_IP   = '192.168.4.1'
ESP32_PORT = 8000

# Command codes must match your ESP32 handler
CMD_STOP = 0
CMD_TURN = 2

# ——— INIT CONTROLLER ———
pygame.init()
pygame.joystick.init()
if pygame.joystick.get_count() == 0:
    print("No joystick detected!")
    exit()
joy = pygame.joystick.Joystick(0)
joy.init()
print("Using joystick:", joy.get_name())

# ——— SETUP TCP ———
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
sock.connect((ESP32_IP, ESP32_PORT))
print(f"Connected to ESP32 at {ESP32_IP}:{ESP32_PORT}")

last_cmd = (None, None)
def send_cmd(cmd: int, value: int):
    global last_cmd
    # clamp to –100…+100
    value = max(-100, min(100, value))
    if (cmd, value) == last_cmd:
        return  # skip duplicates
    last_cmd = (cmd, value)
    packet = struct.pack(">bh", cmd, value)
    sock.sendall(packet)

try:
    while True:
        pygame.event.pump()

        # PS4 R2 trigger on axis 5: –1 (released) → +1 (fully pressed)
        r2 = (joy.get_axis(5) + 1) / 2   # normalize to 0..1
        deadman = (r2 > 0.5)

        # Right stick X-axis (axis 2): –1 (left) → +1 (right)
        x = joy.get_axis(2)
        turn_value = int(x * 100)  # –100…+100

        if deadman:
            send_cmd(CMD_TURN, turn_value)
            print(f"Sent: TURN {turn_value}")
        else:
            send_cmd(CMD_STOP, 0)
            print("Sent: STOP")

        time.sleep(0.02)  # 50 Hz loop

except KeyboardInterrupt:
    print("Exiting.")
finally:
    sock.close()
    pygame.quit()
