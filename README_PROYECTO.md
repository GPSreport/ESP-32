# 📷 ESP32-S3-CAM - Captura de Imágenes para Detección de Aforo

## 📋 Resumen del Proyecto

Este componente utiliza un microcontrolador **ESP32-S3** con cámara **OV2640** integrada para capturar imágenes automáticamente y enviarlas al servidor de procesamiento de imágenes. Las imágenes son procesadas con **YOLOv8** para detectar personas y calcular el aforo de lugares de interés.

---

## 🏗️ Arquitectura del Sistema

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    FLUJO DE DATOS ESP32-CAM                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                        ESP32-S3-CAM                                │  │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐    │  │
│  │  │   OV2640    │  │   WiFi      │  │   PSRAM (8MB)           │    │  │
│  │  │   Cámara    │  │  Conexión   │  │   Almacenamiento temp   │    │  │
│  │  └──────┬──────┘  └──────┬──────┘  └─────────────────────────┘    │  │
│  │         │                │                                         │  │
│  │         ▼                │                                         │  │
│  │  ┌─────────────┐         │                                         │  │
│  │  │  Captura    │         │                                         │  │
│  │  │  800x600    │         │                                         │  │
│  │  │  JPEG       │         │                                         │  │
│  │  └──────┬──────┘         │                                         │  │
│  │         │                │                                         │  │
│  │         ▼                │                                         │  │
│  │  ┌─────────────┐         │                                         │  │
│  │  │  Base64     │         │                                         │  │
│  │  │  Encoding   │─────────┼─────────────────────────┐               │  │
│  │  └─────────────┘         │                         │               │  │
│  └──────────────────────────│─────────────────────────│───────────────┘  │
│                             │                         │                  │
│                             │ HTTP POST               │                  │
│                             ▼                         │                  │
│            ┌────────────────────────────────┐         │                  │
│            │   Servidor Procesamiento       │         │                  │
│            │   mainIMG.py                   │◄────────┘                  │
│            │   18.116.117.140:8000          │    JSON:                   │
│            │                                │    - foto_base64           │
│            │   ┌─────────────────────────┐  │    - timestamp (ISO8601)   │
│            │   │      YOLOv8 nano        │  │    - latitud               │
│            │   │   Detección personas    │  │    - longitud              │
│            │   └─────────────────────────┘  │    - lugar_id              │
│            └───────────────┬────────────────┘                            │
│                            │                                             │
│                            │ Aforo calculado                             │
│                            ▼                                             │
│            ┌────────────────────────────────┐                            │
│            │      API Principal             │                            │
│            │      main.py:5000              │                            │
│            │      MySQL + Almacenamiento    │                            │
│            └────────────────────────────────┘                            │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 📁 Estructura del Proyecto

```
ESP/
└── ESPrep/
    ├── platformio.ini        # Configuración de PlatformIO
    ├── src/
    │   └── main.cpp          # Código principal del ESP32
    ├── include/
    │   └── README            # Documentación de headers
    ├── lib/
    │   └── README            # Bibliotecas personalizadas
    └── test/
        └── README            # Tests
```

---

## 🔧 Especificaciones de Hardware

### ESP32-S3-DevKitC-1

| Característica        | Valor                           |
|-----------------------|---------------------------------|
| **MCU**               | ESP32-S3                        |
| **Flash**             | 16 MB                           |
| **PSRAM**             | 8 MB (OPI)                      |
| **Cámara**            | OV2640 integrada                |
| **Conectividad**      | WiFi 802.11 b/g/n               |
| **USB**               | USB-C (UART + OTG)              |
| **MicroSD**           | Slot disponible                 |
| **LED Flash**         | GPIO 48 (opcional)              |

### Configuración de Pines de Cámara

```cpp
// Pines estándar para ESP32-S3-CAM con OV2640
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4   // I2C Data
#define SIOC_GPIO_NUM     5   // I2C Clock

// Pines de datos (Y2-Y9)
#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM       11

// Pines de sincronización
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13
```

---

## ⚙️ Configuración de PlatformIO

```ini
[env:esp32-s3-n16r8]
platform = espressif32
board = esp32-s3-devkitc-1
board_build.mcu = esp32s3
board_build.flash_size = 16MB
board_build.psram = enabled
board_build.flash_mode = qio
board_build.psram_type = opi
board_build.arduino.memory_type = qio_opi
framework = arduino
upload_port = COM3
monitor_speed = 115200
upload_speed = 921600
build_type = release
lib_deps = 
    bblanchon/ArduinoJson@^7.2.1
build_flags =
    -DBOARD_HAS_PSRAM
    -DCORE_DEBUG_LEVEL=0
```

---

## 🚀 Funcionalidades Principales

### 1. Conexión WiFi

```cpp
const char* WIFI_SSID = "nombre_red";
const char* WIFI_PASS = "contraseña";

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  // Timeout de 20 segundos
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(300);
  }
  // Configurar NTP después de conectar
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}
```

### 2. Sincronización de Tiempo (NTP)

```cpp
// Genera timestamp en formato ISO 8601
String iso8601Now() {
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  return String(buf);
}
```

### 3. Configuración de Cámara

```cpp
// Configuración optimizada para máxima calidad
if(psramFound()){
  config.frame_size = FRAMESIZE_SVGA;  // 800x600
  config.jpeg_quality = 10;            // Alta calidad
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;
} else {
  config.frame_size = FRAMESIZE_VGA;   // 640x480
  config.jpeg_quality = 12;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_DRAM;
}
```

**Ajustes del Sensor OV2640:**
```cpp
sensor_t * s = esp_camera_sensor_get();
s->set_brightness(s, 0);      // Brillo neutral
s->set_contrast(s, 1);        // Contraste aumentado
s->set_saturation(s, 1);      // Saturación aumentada
s->set_sharpness(s, 2);       // Máxima nitidez
s->set_denoise(s, 0);         // Sin reducción de ruido
s->set_quality(s, 4);         // Calidad máxima
s->set_awb_gain(s, 1);        // Auto White Balance
```

### 4. Captura y Codificación Base64

```cpp
void captureAndConvertToBase64() {
  // Captura con reintentos
  camera_fb_t * fb = NULL;
  for (int i = 0; i < 3; i++) {
    fb = esp_camera_fb_get();
    if (fb) break;
    delay(100);
  }
  
  // Codificar a Base64 usando mbedtls
  size_t olen = 0;
  mbedtls_base64_encode(NULL, 0, &olen, fb->buf, fb->len);
  char* base64_buf = (char*)malloc(olen + 1);
  mbedtls_base64_encode((unsigned char*)base64_buf, olen, &olen, 
                         fb->buf, fb->len);
  
  // Enviar al servidor...
  esp_camera_fb_return(fb);
}
```

### 5. Envío HTTP al Servidor

```cpp
// Construir payload JSON
String payload = "{";
payload += "\"foto_base64\":\"" + String(base64_buf) + "\",";
payload += "\"timestamp\":\"" + iso8601Now() + "\",";
payload += "\"latitud\":" + String(DEFAULT_LAT, 6) + ",";
payload += "\"longitud\":" + String(DEFAULT_LON, 6) + ",";
payload += "\"lugar_id\":\"" + String(DEFAULT_LUGAR_ID) + "\"";
payload += "}";

// Enviar POST
HTTPClient http;
http.begin(SERVER_URL);
http.addHeader("Content-Type", "application/json");
http.setTimeout(30000);  // 30 segundos
int code = http.POST(payload);
```

---

## 🔗 Articulación con el Ecosistema

### → Hacia Servidor de Procesamiento

**Endpoint destino:**
```
POST http://18.116.117.140:8000/procesar-imagen
```

**Payload JSON enviado:**
```json
{
  "foto_base64": "iVBORw0KGgoAAAANS...",
  "timestamp": "2025-03-11T14:30:00",
  "latitud": 11.0193,
  "longitud": -74.8516,
  "lugar_id": "Bloque L, Piso 3, 1L6-38"
}
```

### ← Respuesta del Servidor

```json
{
  "success": true,
  "message": "Imagen procesada correctamente",
  "aforo": 5,
  "processing_time_ms": 850.3,
  "forwarded_to_api": true
}
```

### Flujo Completo de Datos

```
ESP32-CAM (Captura cada 30s)
        │
        │ JSON: foto_base64 + coords + timestamp
        ▼
mainIMG.py:8000 (Procesamiento)
        │
        │ YOLOv8 → Detecta personas → Aforo
        ▼
main.py:5000 (API Principal)
        │
        │ Almacena en MySQL + Guarda imagen
        ▼
Base de Datos MySQL
        │
        │ Consulta
        ▼
App Flutter / Mapa Web (Visualización)
```

---

## 📊 Ciclo de Operación

```
┌─────────────────────────────────────────────────────────┐
│                    LOOP PRINCIPAL                        │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │  1. Esperar 30 segundos                          │   │
│  └──────────────────────────────────────────────────┘   │
│                        │                                 │
│                        ▼                                 │
│  ┌──────────────────────────────────────────────────┐   │
│  │  2. Verificar memoria disponible                 │   │
│  │     - PSRAM libre                                │   │
│  │     - Heap libre                                 │   │
│  └──────────────────────────────────────────────────┘   │
│                        │                                 │
│                        ▼                                 │
│  ┌──────────────────────────────────────────────────┐   │
│  │  3. Capturar imagen (reintentos si falla)        │   │
│  └──────────────────────────────────────────────────┘   │
│                        │                                 │
│                        ▼                                 │
│  ┌──────────────────────────────────────────────────┐   │
│  │  4. Codificar a Base64                           │   │
│  └──────────────────────────────────────────────────┘   │
│                        │                                 │
│                        ▼                                 │
│  ┌──────────────────────────────────────────────────┐   │
│  │  5. Construir JSON con metadatos                 │   │
│  └──────────────────────────────────────────────────┘   │
│                        │                                 │
│                        ▼                                 │
│  ┌──────────────────────────────────────────────────┐   │
│  │  6. Enviar POST al servidor de procesamiento     │   │
│  └──────────────────────────────────────────────────┘   │
│                        │                                 │
│                        ▼                                 │
│  ┌──────────────────────────────────────────────────┐   │
│  │  7. Liberar memoria y volver al paso 1           │   │
│  └──────────────────────────────────────────────────┘   │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

---

## ⚙️ Variables de Configuración

```cpp
// Configuración WiFi
const char* WIFI_SSID = "nombre_red";
const char* WIFI_PASS = "contraseña";

// Servidor de procesamiento
const char* SERVER_URL = "http://18.116.117.140:8000/procesar-imagen";

// Coordenadas del lugar (fijas)
float DEFAULT_LAT = 11.0193f;
float DEFAULT_LON = -74.8516f;
const char* DEFAULT_LUGAR_ID = "Bloque L, Piso 3, 1L6-38";

// Intervalo de captura
#define CAPTURE_INTERVAL_MS 30000  // 30 segundos
```

---

## 🛠️ Compilación y Carga

### Requisitos
- PlatformIO IDE (VS Code extension)
- Cable USB-C
- Driver USB para ESP32-S3

### Comandos

```bash
# Compilar proyecto
pio run

# Compilar y cargar al ESP32
pio run --target upload

# Monitor serial
pio device monitor --baud 115200
```

---

## 📟 Salida Serial (Debug)

```
╔════════════════════════════════════════╗
║  ESP32-S3 Camera Base64 Converter    ║
╚════════════════════════════════════════╝
💾 PSRAM: ✅ Disponible (8MB)
📦 Flash: 16MB
🔌 Puerto: USB-C UART y OTG
💳 MicroSD: Disponible
📷 Cámara: OV2640 Integrada
----------------------------------------

📶 Conectando a Wi‑Fi: realme 8 Pro
....
✅ Wi‑Fi conectado. IP: 192.168.100.15
✅ Cámara OV2640 inicializada correctamente!

📸 Capturando foto...
✅ Foto capturada: 45678 bytes
🔄 Convirtiendo a Base64...
📊 Base64 generado: 60904 caracteres
🚀 Enviando imagen al servidor...
📦 Payload JSON: 61050 bytes
✅ POST => código 200
🔁 Respuesta:
{"success":true,"message":"Imagen procesada","aforo":3}
✅ Proceso completado
```

---

## 🔒 Consideraciones de Seguridad

- Las credenciales WiFi están hardcodeadas (considerar configuración externa)
- Comunicación HTTP sin cifrado (usar HTTPS en producción)
- Sin autenticación hacia el servidor (añadir API key si es necesario)

---

## 🐛 Manejo de Errores

| Error                           | Acción                             |
|---------------------------------|------------------------------------|
| Fallo de captura de cámara      | Reintentar 3 veces, reiniciar cámara |
| Sin conexión WiFi               | No enviar imagen, esperar siguiente ciclo |
| Falta de memoria para Base64    | Liberar memoria, reiniciar ciclo   |
| Error HTTP POST                 | Log de error, continuar operación  |

---

## 📚 Bibliotecas Utilizadas

| Biblioteca        | Uso                                    |
|-------------------|----------------------------------------|
| `esp_camera.h`    | Control de cámara OV2640               |
| `WiFi.h`          | Conectividad WiFi                      |
| `HTTPClient.h`    | Peticiones HTTP POST                   |
| `mbedtls/base64.h`| Codificación Base64 eficiente          |
| `time.h`          | Sincronización NTP y timestamps        |
| `ArduinoJson`     | Parsing de respuestas JSON             |

---

## 🔮 Mejoras Futuras

- [ ] Configuración WiFi via BLE o portal cautivo
- [ ] Almacenamiento en SD si no hay conexión
- [ ] Lectura de GPS real (módulo NEO-6M)
- [ ] Comunicación HTTPS con certificado
- [ ] Deep sleep entre capturas para ahorro de energía
- [ ] Detección de movimiento para captura inteligente

---

*Última actualización: Marzo 2026*
