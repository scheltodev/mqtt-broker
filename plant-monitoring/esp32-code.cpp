#include <WiFi.h>
#include <PubSubClient.h>

// WLAN-Konfiguration
const char* ssid = "WLAN-504009_EXT";          // Dein WLAN-SSID
const char* password = "2764193621847697";  // Dein WLAN-Passwort

// MQTT-Broker-Konfiguration
const char* mqtt_broker = "192.168.2.8";      // IP deines MQTT-Brokers
const int mqtt_port = 30605;                  // NodePort des MQTT-Brokers
const char* topic = "plants/sensor1";         // MQTT-Topic für den Sensor
const char* mqtt_username = "";               // Falls nötig
const char* mqtt_password = "";               // Falls nötig

// Sensor-Pin-Konfiguration
const int sensorPin = 34;  // Pin, an dem der Sensor angeschlossen ist

WiFiClient espClient;
PubSubClient client(espClient);

void connectToWiFi() {
  Serial.print("Verbinde mit WLAN...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nVerbunden mit WLAN!");
}

void connectToMQTT() {
  Serial.print("Verbinde mit MQTT-Broker...");
  while (!client.connected()) {
    if (client.connect("ESP32Client", mqtt_username, mqtt_password)) {
      Serial.println("Verbunden!");
    } else {
      Serial.print("Fehler: ");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // WLAN verbinden
  connectToWiFi();

  // MQTT-Client konfigurieren
  client.setServer(mqtt_broker, mqtt_port);
}

void loop() {
  if (!client.connected()) {
    connectToMQTT();
  }
  client.loop();

  // Sensorwert lesen
  int sensorValue = analogRead(sensorPin);

  // Wert normalisieren (0-100%)
  float moisturePercent = map(sensorValue, 0, 4095, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);

  // Sensorwert an MQTT-Broker senden
  char payload[50];
  snprintf(payload, sizeof(payload), "%.2f", moisturePercent);
  client.publish(topic, payload);

  Serial.printf("Wert gesendet: %s%% an Topic: %s\n", payload, topic);

  delay(5000);  // Daten alle 5 Sekunden senden
}
