#include <WiFi.h>  
#include <esp_http_server.h>  
#include "esp_camera.h"  
#include <Robojax_L298N_DC_motor.h>  
  
// ——— Pin Definitions ———  
#define TRIG_PIN 1   // GPIO1 (TX)  
#define ECHO_PIN 3   // GPIO3 (RX)  
#define LED_PIN 4    // GPIO4 — ESP32-CAM flash  
  
// ——— Camera pin definitions (AI-Thinker) ———  
#define PWDN_GPIO_NUM     32  
#define RESET_GPIO_NUM    -1  
#define XCLK_GPIO_NUM      0  
#define SIOD_GPIO_NUM     26  
#define SIOC_GPIO_NUM     27  
#define Y9_GPIO_NUM       35  
#define Y8_GPIO_NUM       34  
#define Y7_GPIO_NUM       39  
#define Y6_GPIO_NUM       36  
#define Y5_GPIO_NUM       21  
#define Y4_GPIO_NUM       19  
#define Y3_GPIO_NUM       18  
#define Y2_GPIO_NUM        5  
#define VSYNC_GPIO_NUM    25  
#define HREF_GPIO_NUM     23  
#define PCLK_GPIO_NUM     22  

const char* ap_ssid  = "ESP32_CTRL";        // Hotspot SSID  
const char* ap_pass  = "12345678";          // Hotspot Password  
  
WiFiServer server(8000);  // Binary command server  
WiFiClient client;  
  
// ——— Motor Configuration ———  
#define CHA 0  
#define ENA 12  
#define IN1 13  
#define IN2 15  
#define IN3 14  
#define IN4 16  
#define ENB 2  
#define CHB 1  
const int CCW = 2; // forward  
const int CW  = 1; // backward  
  
Robojax_L298N_DC_motor robot(IN1, IN2, ENA, CHA, IN3, IN4, ENB, CHB);  
  
// ——— Commands ———  
const int8_t CMD_STOP = 0;  
const int8_t CMD_FWD  = 1;  
const int8_t CMD_TURN = 2;  
  
// ——— Movement Speed ———  
const int16_t BASE_SPEED = 60;  
const int16_t MAX_SPEED  = 100;  
  
// ——— HTTP Server for Camera Streaming ———  
httpd_handle_t stream_httpd = NULL;  
  
// ——— Utility Functions ———  
int16_t clamp(int16_t value, int16_t minValue, int16_t maxValue) {
  if (value < minValue) {
    return minValue;
  } else if (value > maxValue) {
    return maxValue;
  } else {
    return value;
  }
}   

int16_t scaledPower(int16_t percent) {
  // Calculate additional speed based on the percentage (0–100)
  int16_t delta   = (MAX_SPEED - BASE_SPEED) * percent / 100;
  int16_t result  = BASE_SPEED + delta;
  // Ensure we never go below BASE_SPEED or above MAX_SPEED
  return clamp(result, BASE_SPEED, MAX_SPEED);
}
  
// ——— Stream Handler ———  
static esp_err_t stream_handler(httpd_req_t *req) {  
  camera_fb_t *fb;  
  httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");  
  while (true) {  
    fb = esp_camera_fb_get();  
    if (!fb) {  
      httpd_resp_send_500(req);  
      return ESP_FAIL;  
    }  
  
    char header[128];  
    int hlen = snprintf(header, sizeof(header),  
      "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",  
      (uint32_t)fb->len);  
  
    // Send header  
    if (httpd_resp_send_chunk(req, header, hlen) != ESP_OK) {  
      esp_camera_fb_return(fb);  
      break;  
    }  
  
    // Send image data  
    if (httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len) != ESP_OK) {  
      esp_camera_fb_return(fb);  
      break;  
    }  
  
    // Send boundary  
    if (httpd_resp_send_chunk(req, "\r\n", 2) != ESP_OK) {  
      esp_camera_fb_return(fb);  
      break;  
    }  
  
    esp_camera_fb_return(fb);  
    vTaskDelay(10 / portTICK_PERIOD_MS);  // Slight delay for stability  
  }  
  return ESP_OK;  
}  
  
// ——— Start Stream Server ———  
void start_stream_server() {  
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();  
  cfg.server_port      = 81;  // Camera stream on port 81  
  cfg.lru_purge_enable = true;  
  
  if (httpd_start(&stream_httpd, &cfg) == ESP_OK) {  
    static httpd_uri_t stream_uri = { "/stream", HTTP_GET, stream_handler, NULL };  
    httpd_register_uri_handler(stream_httpd, &stream_uri);  
  }  
}  

void setupWiFi() {  
    Serial.println("\nStarting Access Point...");  
    WiFi.softAP(ap_ssid, ap_pass);  
    Serial.println("Access Point IP: ");
    Serial.println(WiFi.softAPIP());  
}

  
// ——— Setup ———  
void setup() {  
  Serial.begin(115200);  
  pinMode(LED_PIN, OUTPUT);  
  digitalWrite(LED_PIN, LOW);  
  
  // Initialize Motor  
  robot.begin();  
  
  // Initialize Camera  
  camera_config_t cam_cfg = {  
    .pin_pwdn  = PWDN_GPIO_NUM,  
    .pin_reset = RESET_GPIO_NUM,  
    .pin_xclk  = XCLK_GPIO_NUM,  
    .pin_sccb_sda = SIOD_GPIO_NUM,  
    .pin_sccb_scl = SIOC_GPIO_NUM,  
    .pin_d7    = Y9_GPIO_NUM,  
    .pin_d6    = Y8_GPIO_NUM,  
    .pin_d5    = Y7_GPIO_NUM,  
    .pin_d4    = Y6_GPIO_NUM,  
    .pin_d3    = Y5_GPIO_NUM,  
    .pin_d2    = Y4_GPIO_NUM,  
    .pin_d1    = Y3_GPIO_NUM,  
    .pin_d0    = Y2_GPIO_NUM,  
    .pin_vsync = VSYNC_GPIO_NUM,  
    .pin_href  = HREF_GPIO_NUM,  
    .pin_pclk  = PCLK_GPIO_NUM,  
    .xclk_freq_hz = 20000000,  
    .pixel_format = PIXFORMAT_JPEG,  
    .frame_size = FRAMESIZE_QVGA,  
    .jpeg_quality = 12,  
    .fb_count = 1,  
    .fb_location = CAMERA_FB_IN_DRAM  
  };  
  
  if (esp_camera_init(&cam_cfg) != ESP_OK) {  
    Serial.println("Camera init failed");  
    while (true) delay(1000);  
  }  
  
  // Setup Wi-Fi and start servers  
  setupWiFi();  
  server.begin();  
  start_stream_server();  
}  
  
// ——— Loop ———  
void loop() {  
  // Accept client  
  if (!client || !client.connected()) {  
    client = server.available();  
  }  
  
  // Read and process binary commands  
  const int PKT = 3;  
  static uint8_t buf[PKT];  
  
  while (client && client.available() >= PKT) {  
    client.read(buf, PKT);  
    int8_t cmd = buf[0];  
    int16_t value = (buf[1] << 8) | buf[2];  
  
    int16_t leftPwr  = 0, rightPwr = 0;  
    int leftDir = CCW, rightDir = CCW;  
  
    switch (cmd) {  
      case CMD_STOP:  
        robot.brake(1);  
        robot.brake(2);  
        break;  
  
      case CMD_FWD:  
        leftPwr = rightPwr = scaledPower(BASE_SPEED);  //abs(value)
        break;  
  
      case CMD_TURN:  

        if (value > -15 && value < 15) {
          leftPwr  = BASE_SPEED;
          rightPwr = BASE_SPEED;
          leftDir  = CCW;
          rightDir = CCW;
        }
        else if (value > 0) {
          leftPwr = scaledPower(abs(value));  
          rightPwr = 0;  // stop
          leftDir = CCW;   // Forward  
          rightDir = CW;   // Backward  
        }
        else {  // value < 0
          leftPwr = 0;  // stop
          rightPwr = scaledPower(abs(value));  
          leftDir = CW;    // Backward  
          rightDir = CCW;  // Forward  
        }
        
        break;  
    }  
  
    robot.rotate(1, leftPwr, leftDir);  
    robot.rotate(2, rightPwr, rightDir);  
  }  
  
  delay(10);  // Slight delay for stability  
}
