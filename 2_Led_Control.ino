#include <WiFi.h>
#include <esp_http_server.h>

// ——— Wi-Fi AP credentials ———
const char* ap_ssid     = "ESP32-CAM";
const char* ap_password = "12345678";

// ——— LED pin ———
#define LED_GPIO_NUM       4

// ——— HTTP server handle ———
httpd_handle_t cmd_httpd = NULL;

// ——— LED command handlers (port 81) ———
static esp_err_t led_on_handler(httpd_req_t *req) {
  digitalWrite(LED_GPIO_NUM, HIGH);
  return httpd_resp_send(req, "LED ON", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t led_off_handler(httpd_req_t *req) {
  digitalWrite(LED_GPIO_NUM, LOW);
  return httpd_resp_send(req, "LED OFF", HTTPD_RESP_USE_STRLEN);
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

  // Start Access Point
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("AP IP: http://");
  Serial.println(WiFi.softAPIP());

  start_cmd_server();
}

void loop() {
  // Nothing to do here
}
