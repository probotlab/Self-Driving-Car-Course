#include <WiFi.h>

// Wi-Fi Access Point config
const char* ssid = "ESP32_CTRL";
const char* password = "12345678";

WiFiServer server(8000);
WiFiClient client;

void setup() {
  Serial.begin(115200);

  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  server.begin();
  Serial.println("Server started, waiting for client…");
}

void loop() {
  // Accept new client
  if (!client || !client.connected()) {
    client = server.available();
    return;
  }

  // Expecting 3-byte binary packet: [cmd][value_hi][value_lo]
  const int packetSize = 3;
  static uint8_t buffer[packetSize];

  while (client.available() >= packetSize) {
    client.read(buffer, packetSize);

    int8_t cmd = buffer[0];
    int16_t value = (buffer[1] << 8) | buffer[2];

    handleCommand(cmd, value);
  }
}

void handleCommand(int8_t cmd, int16_t val) {
  switch (cmd) {
    case 0: Serial.println("STOP"); break;
    case 1: Serial.printf("FWD %d%%\n", val); break;
    case 2: Serial.printf("TURN %d%%\n", val); break;
    default: Serial.println("Unknown command"); break;
  }
}
