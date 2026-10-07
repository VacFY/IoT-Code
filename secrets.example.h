// Copia este archivo como "secrets.h" en la misma carpeta y completa los datos.
// secrets.h está en .gitignore: NUNCA lo subas a GitHub.
#pragma once

// ---- WiFi ----
#define WIFI_SSID     "nombre-de-tu-red"
#define WIFI_PASSWORD "clave-de-tu-red"

// ---- Broker MQTT ----
// Local (Mosquitto en tu laptop): IP de la laptop, puerto 1883, sin TLS.
// Nube (p. ej. HiveMQ Cloud / EMQX / Mosquitto en un servidor): host público, puerto 8883, TLS = 1.
#define MQTT_HOST     "192.168.137.1"
#define MQTT_PORT     1883
#define MQTT_USE_TLS  0
#define MQTT_USER     ""
#define MQTT_PASS     ""

// Certificado raíz (PEM) del broker si usas TLS. Vacío = no se verifica (solo pruebas).
#define MQTT_CA_CERT  ""
