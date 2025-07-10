ode#include "esp_camera.h"
#include <WiFi.h>
#include <esp_http_server.h>

// Wi-Fi Credentials
const char* sta_ssid = "YOUR_WIFI_SSID";
const char* sta_password = "YOUR_WIFI_PASSWORD";

// Hotspot credentials
const char* ap_ssid = "ESP32-CAM";
const char* ap_password = "12345678"; // Minimum 8 characters

// Camera pin definitions for AI-Thinker module
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

#define LED_GPIO_NUM       4

httpd_handle_t camera_httpd = NULL;

static esp_err_t stream_handler(httpd_req_t *req) {
    char part_buf[64];
    camera_fb_t * fb = NULL;
    esp_err_t res;

    res = httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
    if (res != ESP_OK) return res;

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        size_t hlen = snprintf(part_buf, sizeof(part_buf),
            "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
        res = httpd_resp_send_chunk(req, part_buf, hlen);
        if (res != ESP_OK) {
            esp_camera_fb_return(fb);
            break;
        }

        res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
        if (res != ESP_OK) {
            esp_camera_fb_return(fb);
            break;
        }

        res = httpd_resp_send_chunk(req, "\r\n", 2);
        esp_camera_fb_return(fb);
        if (res != ESP_OK) break;

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    return res;
}

void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.max_uri_handlers = 1;

    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_uri_t uri_config = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = stream_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(camera_httpd, &uri_config);
    }
}

void setup() {
  Serial.begin(115200);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
      Serial.printf("Camera init failed: 0x%x", err);
      return;
  }

  pinMode(LED_GPIO_NUM, OUTPUT);
  digitalWrite(LED_GPIO_NUM, LOW);


  // Try to connect in Station mode first
  WiFi.mode(WIFI_STA);
  WiFi.begin(sta_ssid, sta_password);

  Serial.println("Trying to connect to Wi-Fi (Station Mode)...");
  unsigned long startAttemptTime = millis();

  // Wait up to 2 seconds
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 5000) {
      delay(100);
  }

  IPAddress IP;

  if (WiFi.status() == WL_CONNECTED) {
      IP = WiFi.localIP();
      Serial.println("Connected to Wi-Fi!");
      Serial.print("Camera Stream URL: http://");
      Serial.println(IP);
  } else {
      Serial.println("Wi-Fi not found, starting Access Point...");
      WiFi.mode(WIFI_AP);
      WiFi.softAP(ap_ssid, ap_password);
      delay(1000);
      IP = WiFi.softAPIP();
      Serial.println("Access Point Started");
      Serial.print("Connect to Wi-Fi: ");
      Serial.println(ap_ssid);
      Serial.print("Camera Stream URL: http://");
      Serial.println(IP);
  }

    startCameraServer();
}

void loop() {
    delay(10000);
}
