#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ========================================
// CONFIGURACIÓN WiFi
// ========================================

const char* WIFI_SSID = "Carlos-PC";
const char* WIFI_PASSWORD = "12345678";

// ========================================
// CONFIGURACIÓN MQTT
// ========================================

const char* MQTT_SERVER = "192.168.137.1";
const int MQTT_PORT = 1883;

const char* MQTT_TOPIC = "iot/telemetry";

// ========================================
// DHT22 / AM2302B
// ========================================

#define DHT_PIN 4
#define DHT_TYPE DHT22

DHT dht(DHT_PIN, DHT_TYPE);

// ========================================
// WiFi / MQTT
// ========================================

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);


// ========================================
// Conectar a WiFi
// ========================================

void conectarWiFi() {

  Serial.print("Conectando a WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi conectado.");

  Serial.print("IP del ESP32: ");
  Serial.println(WiFi.localIP());
}


// ========================================
// Conectar a MQTT
// ========================================

void conectarMQTT() {

  while (!mqttClient.connected()) {

    Serial.print("Conectando a Mosquitto... ");

    String clientId = "ESP32-Contenedor-001";

    if (mqttClient.connect(clientId.c_str())) {

      Serial.println("CONECTADO");

    } else {

      Serial.print("ERROR. Estado MQTT: ");
      Serial.println(mqttClient.state());

      delay(5000);
    }
  }
}


// ========================================
// SETUP
// ========================================

void setup() {

  Serial.begin(115200);

  // DHT22 / AM2302B
  dht.begin();

  // MQTT
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  Serial.println();
  Serial.println("================================");
  Serial.println("    CONTENEDOR DE VACUNAS");
  Serial.println("          ESP32");
  Serial.println("================================");
  Serial.println();

  conectarWiFi();
}


// ========================================
// LOOP
// ========================================

void loop() {

  // ------------------------------------
  // Comprobar WiFi
  // ------------------------------------

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("WiFi desconectado.");

    conectarWiFi();
  }


  // ------------------------------------
  // Comprobar MQTT
  // ------------------------------------

  if (!mqttClient.connected()) {

    conectarMQTT();
  }

  mqttClient.loop();


  // ------------------------------------
  // Leer DHT22
  // ------------------------------------

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


  // ------------------------------------
  // Crear JSON
  // ------------------------------------

  String payload = "{";

  payload += "\"contenedor\":\"001\",";

  payload += "\"temperatura\":";
  payload += String(temperatura, 2);
  payload += ",";

  payload += "\"humedad\":";
  payload += String(humedad, 2);

  payload += "}";


  // ------------------------------------
  // Publicar MQTT
  // ------------------------------------

  Serial.print("MQTT -> ");
  Serial.println(payload);

  mqttClient.publish(MQTT_TOPIC, payload.c_str());


  // DHT22/AM2302B: no leer demasiado rápido
  delay(2000);
}