#include <WiFi.h>
#include <PubSubClient.h>

// WLAN-Konfiguration
const char *ssid = "WLAN-504009_EXT";
const char *password = "2764193621847697";

// MQTT-Broker-Konfiguration
const char *mqtt_broker = "192.168.2.8";
const int mqtt_port = 30605;
const char *topic = "plants/sensor2";

WiFiClient espClient;
PubSubClient client(espClient);

void connectToWiFi()
{
  Serial.print("Verbinde mit WLAN...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nVerbunden mit WLAN!");
}

void connectToMQTT()
{
  Serial.print("Verbinde mit MQTT-Broker...");
  String clientId = "ESP32Client-" + String(random(0xffff), HEX);
  while (!client.connected())
  {
    if (client.connect(clientId.c_str()))
    {
      Serial.println("Verbunden!");
    }
    else
    {
      Serial.printf("Fehler: %d\n", client.state());
      delay(2000);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  connectToWiFi();
  client.setServer(mqtt_broker, mqtt_port);
  client.setKeepAlive(60);
}

void loop()
{
  if (!client.connected())
  {
    connectToMQTT();
  }
  client.loop();

  int sensorValue = analogRead(34);
  Serial.printf("Rohwert des Sensors: %d\n", sensorValue);

  // Umrechnung des Sensormesswerts in Prozent
  float moisturePercent = map(sensorValue, 2000, 1200, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);

  // Zustandserkennung basierend auf Feuchtigkeitswert
  const char *status = "Normal";
  if (moisturePercent <= 20)
  {
    status = "Bitte gießen";
  }
  else if (moisturePercent > 20 && moisturePercent <= 40)
  {
    status = "Trocken";
  }
  else if (moisturePercent > 40 && moisturePercent <= 70)
  {
    status = "Feucht genug";
  }
  else
  {
    status = "Sehr feucht";
  }

  // MQTT Payload vorbereiten
  char payload[100];
  snprintf(payload, sizeof(payload), "{\"moisture\": %.2f, \"status\": \"%s\"}", moisturePercent, status);
  client.publish(topic, payload, false);

  Serial.printf("Wert gesendet: %s an Topic: %s\n", payload, topic);
  delay(5000);
}
