#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include "mbedtls/base64.h"

// Configuración de pines para ESP32-S3 con cámara OV2640 integrada
// Esta configuración es estándar para módulos ESP32-S3-CAM
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5

#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM       11
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

// LED Flash (opcional, algunos módulos lo tienen en GPIO 48)
#define LED_GPIO_NUM      48

// ======= Config Wi‑Fi y servidor =======
const char* WIFI_SSID = "realme 8 Pro";
const char* WIFI_PASS = "asdfghjkl";

// Endpoint de procesamiento
const char* SERVER_URL = "http://18.116.117.140:8000/procesar-imagen";

// Coordenadas.
float DEFAULT_LAT = 11.0193f;
float DEFAULT_LON = -74.8516f;
const char* DEFAULT_LUGAR_ID = "Bloque L, Piso 3, 1L6-38";

// ======= Utilidades de tiempo (NTP) =======
static String iso8601Now() {
  time_t now;
  time(&now);
  struct tm timeinfo;
  if (!localtime_r(&now, &timeinfo)) {
    // Fallback si aún no hay tiempo NTP; usa millis
    unsigned long ms = millis();
    return String("1970-01-01T00:00:") + String((ms / 1000) % 60).c_str();
  }
  char buf[25];
  // Formato ISO8601: YYYY-MM-DDTHH:MM:SS (sin zona)
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  return String(buf);
}

static void connectWiFi() {
  Serial.printf("📶 Conectando a Wi‑Fi: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("✅ Wi‑Fi conectado. IP: %s\n", WiFi.localIP().toString().c_str());
    // Configurar NTP (UTC)
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  } else {
    Serial.println("❌ No se pudo conectar a Wi‑Fi");
  }
}

void setupCamera() {
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
  config.grab_mode = CAMERA_GRAB_LATEST;
  
  // Configuración de calidad de imagen - MÁXIMA CALIDAD
  if(psramFound()){
    config.frame_size = FRAMESIZE_SVGA; // 800x600 - Reducido para evitar problemas de memoria
    config.jpeg_quality = 10; // Calidad alta pero no extrema
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size = FRAMESIZE_VGA; // 640x480
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  // Inicializar cámara
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("❌ Error al inicializar la cámara: 0x%x\n", err);
    return;
  }
  
  // Ajustes adicionales del sensor - Optimizado para máximo detalle
  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_brightness(s, 0);     // -2 a 2
    s->set_contrast(s, 1);       // Aumentar contraste para más detalle
    s->set_saturation(s, 1);     // Aumentar saturación para colores vivos
    s->set_sharpness(s, 2);      // Máxima nitidez
    s->set_denoise(s, 0);        // Desactivar reducción de ruido para más detalle
    s->set_quality(s, 4);        // Calidad máxima
    s->set_hmirror(s, 0);        // 0 = desactivado, 1 = activado
    s->set_vflip(s, 0);          // 0 = desactivado, 1 = activado
    s->set_awb_gain(s, 1);       // Auto White Balance activado
    s->set_wb_mode(s, 0);        // Modo Auto
  }
  
  Serial.println("✅ Cámara OV2640 inicializada correctamente!");
}

void captureAndConvertToBase64() {
  Serial.println("\n📸 Capturando foto...");
  
  // Intentar capturar con reintentos
  camera_fb_t * fb = NULL;
  for (int i = 0; i < 3; i++) {
    fb = esp_camera_fb_get();
    if (fb) break;
    Serial.printf("⚠️ Reintento %d/3...\n", i + 1);
    delay(100);
  }
  
  if (!fb) {
    Serial.println("❌ Error al capturar imagen tras 3 intentos");
    Serial.println("🔄 Reiniciando cámara...");
    esp_camera_deinit();
    delay(200);
    setupCamera();
    return;
  }
  
  Serial.printf("✅ Foto capturada: %d bytes\n", fb->len);
  
  // Convertir a Base64 usando mbedtls (sin saltos de línea)
  Serial.println("🔄 Convirtiendo a Base64...");
  
  size_t olen = 0;
  // Calcular tamaño necesario
  mbedtls_base64_encode(NULL, 0, &olen, fb->buf, fb->len);
  
  // Reservar buffer para base64
  char* base64_buf = (char*)malloc(olen + 1);
  if (!base64_buf) {
    Serial.println("❌ Error: no hay memoria para Base64");
    esp_camera_fb_return(fb);
    return;
  }
  
  // Codificar
  int ret = mbedtls_base64_encode((unsigned char*)base64_buf, olen, &olen, fb->buf, fb->len);
  if (ret != 0) {
    Serial.printf("❌ Error en codificación Base64: %d\n", ret);
    free(base64_buf);
    esp_camera_fb_return(fb);
    return;
  }
  base64_buf[olen] = '\0';
  
  Serial.printf("📊 Base64 generado: %d caracteres\n", olen);
  
  // Liberar el buffer de la cámara
  esp_camera_fb_return(fb);

  // Enviar al servidor
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("🚀 Enviando imagen al servidor...");
    HTTPClient http;
    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(30000); // 30 segundos timeout

    // Construir JSON
    String ts = iso8601Now();
    String payload;
    payload.reserve(128 + olen);
    payload = "{";
    payload += "\"foto_base64\":\""; 
    payload += base64_buf; 
    payload += "\",";
    payload += "\"timestamp\":\""; 
    payload += ts; 
    payload += "\",";
    payload += "\"latitud\":";
    payload += String(DEFAULT_LAT, 6);
    payload += ",";
    payload += "\"longitud\":";
    payload += String(DEFAULT_LON, 6);
    payload += ",";
    payload += "\"lugar_id\":\""; 
    payload += DEFAULT_LUGAR_ID; 
    payload += "\"";
    payload += "}";
    
    free(base64_buf); // Liberar memoria base64

    Serial.printf("📦 Payload JSON: %d bytes\n", payload.length());
    
    int code = http.POST(payload);
    if (code > 0) {
      Serial.printf("✅ POST => código %d\n", code);
      String resp = http.getString();
      if (resp.length() > 0 && resp.length() < 512) {
        Serial.println("🔁 Respuesta:");
        Serial.println(resp);
      }
    } else {
      Serial.printf("❌ Error en POST: %d\n", code);
    }
    http.end();
    Serial.println("✅ Proceso completado\n");
  } else {
    Serial.println("⚠️ Sin Wi‑Fi, no se envía la imagen");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n╔════════════════════════════════════════╗");
  Serial.println("║  ESP32-S3 Camera Base64 Converter    ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.printf("💾 PSRAM: %s\n", psramFound() ? "✅ Disponible (8MB)" : "❌ No disponible");
  Serial.printf("📦 Flash: 16MB\n");
  Serial.printf("🔌 Puerto: USB-C UART y OTG\n");
  Serial.printf("💳 MicroSD: Disponible\n");
  Serial.printf("📷 Cámara: OV2640 Integrada\n");
  Serial.println("----------------------------------------\n");

  // Conectar Wi‑Fi y preparar NTP
  connectWiFi();
  
  // Inicializar cámara
  setupCamera();
  
  delay(2000);
  
  // Capturar primera foto
  captureAndConvertToBase64();
}

void loop() {
  // Capturar foto cada 30 segundos
  delay(30000);
  
  // Limpiar memoria antes de capturar
  if (psramFound()) {
    Serial.printf("💾 PSRAM libre: %d bytes\n", ESP.getFreePsram());
  }
  Serial.printf("💾 Heap libre: %d bytes\n", ESP.getFreeHeap());
  
  captureAndConvertToBase64();
}
