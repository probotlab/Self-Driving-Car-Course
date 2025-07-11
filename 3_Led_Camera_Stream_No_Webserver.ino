// Dual-Server ESP32-CAM Sketch: Stream & Command Servers (No HTML)

#include "esp_camera.h"
#include <WiFi.h>
#include <esp_http_server.h>

// ——— Wi-Fi AP credentials ———
const char* ap_ssid     = "ESP32-CAM";
const char* ap_password = "12345678";

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

// ——— LED pin ———
#define LED_GPIO_NUM       4

// ——— HTTP server handles ———
httpd_handle_t stream_httpd = NULL;
httpd_handle_t cmd_httpd    = NULL;

// ——— MJPEG stream handler (port 80) ———

static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res;

    res = httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
    if (res != ESP_OK) return res;

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        char header[128];
        int hlen = snprintf(header, sizeof(header),
            "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
            fb->len);

        res = httpd_resp_send_chunk(req, header, hlen);
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

        vTaskDelay(10 / portTICK_PERIOD_MS);  // frame rate control
    }

    httpd_resp_send_chunk(req, NULL, 0);  

    return ESP_OK;
}


// ——— LED command handlers (port 81) ———
static esp_err_t led_on_handler(httpd_req_t *req) {
  digitalWrite(LED_GPIO_NUM, HIGH);
  return httpd_resp_send(req, "LED ON", HTTPD_RESP_USE_STRLEN);
}
static esp_err_t led_off_handler(httpd_req_t *req) {
  digitalWrite(LED_GPIO_NUM, LOW);
  return httpd_resp_send(req, "LED OFF", HTTPD_RESP_USE_STRLEN);
}

// ——— Start the stream server on port 80 ———
void start_stream_server() {
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.server_port      = 80;
  cfg.ctrl_port        = 32768;
  cfg.lru_purge_enable = true;

  if (httpd_start(&stream_httpd, &cfg) == ESP_OK) {
    httpd_uri_t mjpg = { "/stream", HTTP_GET, stream_handler, NULL };
    httpd_register_uri_handler(stream_httpd, &mjpg);
  }
}

// ——— Start the LED command server on port 81 ———
void start_cmd_server() {
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.server_port = 81;
  cfg.ctrl_port   = 32769;

  if (httpd_start(&cmd_httpd, &cfg) == ESP_OK) {
    httpd_uri_t on  = { "/on",  HTTP_POST, led_on_handler,  NULL };
    httpd_uri_t off = { "/off", HTTP_POST, led_off_handler, NULL };
    httpd_register_uri_handler(cmd_httpd, &on);
    httpd_register_uri_handler(cmd_httpd, &off);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_GPIO_NUM, OUTPUT);
  digitalWrite(LED_GPIO_NUM, LOW);

  camera_config_t cam_cfg = {
    .pin_pwdn       = PWDN_GPIO_NUM,
    .pin_reset      = RESET_GPIO_NUM,
    .pin_xclk       = XCLK_GPIO_NUM,
    .pin_sccb_sda   = SIOD_GPIO_NUM,
    .pin_sccb_scl   = SIOC_GPIO_NUM,
    .pin_d7         = Y9_GPIO_NUM,
    .pin_d6         = Y8_GPIO_NUM,
    .pin_d5         = Y7_GPIO_NUM,
    .pin_d4         = Y6_GPIO_NUM,
    .pin_d3         = Y5_GPIO_NUM,
    .pin_d2         = Y4_GPIO_NUM,
    .pin_d1         = Y3_GPIO_NUM,
    .pin_d0         = Y2_GPIO_NUM,
    .pin_vsync      = VSYNC_GPIO_NUM,
    .pin_href       = HREF_GPIO_NUM,
    .pin_pclk       = PCLK_GPIO_NUM,
    .xclk_freq_hz   = 20000000,
    .ledc_timer     = LEDC_TIMER_0,
    .ledc_channel   = LEDC_CHANNEL_0,
    .pixel_format   = PIXFORMAT_JPEG,
    .frame_size     = FRAMESIZE_QVGA,
    .jpeg_quality   = 12,
    .fb_count       = 1
  };
  if (esp_camera_init(&cam_cfg) != ESP_OK) {
    Serial.println("Camera init failed");
    while (true) { delay(1000); }
  }

  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("AP IP: http://");
  Serial.println(WiFi.softAPIP());

  start_stream_server();
  start_cmd_server();
}

void loop() {
  // Nothing to do here
}
