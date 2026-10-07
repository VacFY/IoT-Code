// ============================================================
// VacTy — Monitor de temperatura de vacunas (ESP32 WROVER + DHT22)
//
// Mide cada 2 s y publica por MQTT el mismo JSON de siempre:
//   {"contenedor":"001","temperatura":5.20,"humedad":40.00}
// Si el DHT22 falla, envía null (JSON válido) y el backend abre la
// alerta INVALID_READING. Las alertas las decide el backend.
//
// Sin wifi o sin broker NO se bloquea: sigue leyendo y reintenta
// la conexión cada 10 s.
//
// Credenciales: copia secrets.example.h como secrets.h (no se sube a git).
// No usar GPIO16/17: los ocupa la PSRAM del módulo WROVER.
// ============================================================

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <DHT.h>

// Copia este archivo como "secrets.h" en la misma carpeta y completa los datos.
// secrets.h está en .gitignore: NUNCA lo subas a GitHub.
#pragma once

// ---- WiFi ----
#define WIFI_SSID     "--"
#define WIFI_PASSWORD "--"

// ---- Broker MQTT ----
// Local (Mosquitto en tu laptop): IP de la laptop, puerto 1883, sin TLS.
// Nube (p. ej. HiveMQ Cloud / EMQX / Mosquitto en un servidor): host público, puerto 8883, TLS = 1.
#define MQTT_HOST     "y11be1f3.ala.us-east-1.emqxsl.com"
#define MQTT_PORT     8883
#define MQTT_USE_TLS  1
#define MQTT_USER     "esp32-001"
#define MQTT_PASS     "@VacTy@123@"

// Certificado raíz (PEM) del broker si usas TLS. Vacío = no se verifica (solo pruebas).
static const char MQTT_CA_CERT[] = R"EOF(
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
)EOF";

// ---------------- Configuración ----------------
#define DHT_PIN 4
#define DHT_TYPE DHT22

const char* CONTENEDOR = "001";            // código del contenedor (debe coincidir en el backend)
const char* MQTT_TOPIC = "iot/telemetry";

const unsigned long READ_INTERVAL_MS      = 2000;   // el DHT22 no debe leerse más rápido
const unsigned long WIFI_RETRY_MS         = 10000;
const unsigned long MQTT_RETRY_MS         = 10000;
const unsigned long SERIAL_BAUD           = 115200;

// ---------------- Objetos ----------------
DHT dht(DHT_PIN, DHT_TYPE);

#if MQTT_USE_TLS
WiFiClientSecure netClient;
#else
WiFiClient netClient;
#endif
PubSubClient mqttClient(netClient);

String clientId;
unsigned long lastReadAt = 0;
unsigned long lastWifiAttemptAt = 0;
unsigned long lastMqttAttemptAt = 0;
bool wifiWasConnected = false;

// ---------------- WiFi (sin bloquear) ----------------
void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiWasConnected) {
      wifiWasConnected = true;
      Serial.print("WiFi conectado. IP: ");
      Serial.println(WiFi.localIP());
    }
    return;
  }

  if (wifiWasConnected) {
    wifiWasConnected = false;
    Serial.println("WiFi desconectado. Sigo midiendo sin conexión.");
  }

  unsigned long now = millis();
  if (lastWifiAttemptAt != 0 && now - lastWifiAttemptAt < WIFI_RETRY_MS) return;
  lastWifiAttemptAt = now;

  Serial.print("Intentando WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// ---------------- MQTT (sin bloquear) ----------------
void maintainMqtt() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (mqttClient.connected()) {
    mqttClient.loop();
    return;
  }

  unsigned long now = millis();
  if (lastMqttAttemptAt != 0 && now - lastMqttAttemptAt < MQTT_RETRY_MS) return;
  lastMqttAttemptAt = now;

  Serial.print("Conectando a MQTT ");
  Serial.print(MQTT_HOST);
  Serial.print(":");
  Serial.print(MQTT_PORT);
  Serial.print(" ... ");

  bool ok;
  if (strlen(MQTT_USER) > 0) {
    ok = mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS);
  } else {
    ok = mqttClient.connect(clientId.c_str());
  }

  if (ok) {
    Serial.println("CONECTADO");
  } else {
    Serial.print("ERROR (estado ");
    Serial.print(mqttClient.state());
    Serial.println("). Reintento en 10 s.");
  }
}

// ---------------- Lectura y publicación ----------------
String toJsonNumber(float value) {
  if (isnan(value)) return "null";   // "nan" no es JSON válido
  return String(value, 2);
}

void readAndPublish() {
  float temperatura = dht.readTemperature();
  float humedad = dht.readHumidity();

  Serial.println("--------------------------------");
  if (isnan(temperatura) || isnan(humedad)) {
    Serial.println("DHT22: ERROR DE LECTURA");
  } else {
    Serial.print("Temperatura: ");
    Serial.print(temperatura);
    Serial.println(" °C");
    Serial.print("Humedad:     ");
    Serial.print(humedad);
    Serial.println(" %");
  }

  String payload = "{";
  payload += "\"contenedor\":\"";
  payload += CONTENEDOR;
  payload += "\",";
  payload += "\"temperatura\":";
  payload += toJsonNumber(temperatura);
  payload += ",";
  payload += "\"humedad\":";
  payload += toJsonNumber(humedad);
  payload += "}";

  if (mqttClient.connected()) {
    bool sent = mqttClient.publish(MQTT_TOPIC, payload.c_str());
    Serial.print(sent ? "MQTT -> " : "MQTT (falló) -> ");
    Serial.println(payload);
  } else {
    Serial.print("Sin conexión, no se publica: ");
    Serial.println(payload);
  }
}

// ---------------- Setup / Loop ----------------
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);

  Serial.println();
  Serial.println("================================");
  Serial.println("   VacTy - Contenedor de vacunas");
  Serial.println("================================");

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  // clientId único: dos equipos con el mismo id se desconectan entre sí
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  clientId = "vacty-esp32-" + mac;

#if MQTT_USE_TLS
  if (strlen(MQTT_CA_CERT) > 0) {
    netClient.setCACert(MQTT_CA_CERT);
  } else {
    // Solo para pruebas: cifra la conexión pero no verifica el certificado del broker
    netClient.setInsecure();
  }
#endif

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setBufferSize(512);
  mqttClient.setSocketTimeout(5);

  maintainWiFi();
}

void loop() {
  maintainWiFi();
  maintainMqtt();

  unsigned long now = millis();
  if (now - lastReadAt >= READ_INTERVAL_MS) {
    lastReadAt = now;
    readAndPublish();
  }
}
