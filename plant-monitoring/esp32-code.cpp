#include <WiFi.h>
#include <PubSubClient.h>

// WLAN-Konfiguration
const char* ssid = "WLAN-504009_EXT";          
const char* password = "2764193621847697";  

// MQTT-Broker-Konfiguration
const char* mqtt_broker = "192.168.2.8";      
const int mqtt_port = 30605;                  
const char* topic = "plants/sensor2";         

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
  String clientId = "ESP32Client-" + String(random(0xffff), HEX);
  while (!client.connected()) {
    if (client.connect(clientId.c_str())) {
      Serial.println("Verbunden!");
    } else {
      Serial.printf("Fehler: %d\n", client.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  connectToWiFi();
  client.setServer(mqtt_broker, mqtt_port);
  client.setKeepAlive(60);
}

void loop() {
  if (!client.connected()) {
    connectToMQTT();
  }
  client.loop();

  int sensorValue = analogRead(34);
  float moisturePercent = map(sensorValue, 0, 4095, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);

  char payload[50];
  snprintf(payload, sizeof(payload), "%.2f", moisturePercent);
  client.publish(topic, payload, false);

  Serial.printf("Wert gesendet: %s%% an Topic: %s\n", payload, topic);
  delay(5000);
}
