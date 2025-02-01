#include <WiFi.h>
#include <PubSubClient.h>

// WLAN-Konfiguration
const char *ssid = "WLAN-504009_EXT";
const char *password = "2764193621847697";

// MQTT-Broker-Konfiguration
const char *mqtt_broker = "192.168.2.8";
const int mqtt_port = 30605;
// Wir verwenden einen gemeinsamen Topic, der alle Sensoren abdeckt:
const char *topic = "plants/esp32";

// WiFi- und MQTT-Clients
WiFiClient espClient;
PubSubClient client(espClient);

// Konfiguration für bis zu 3 Sensoren
const int sensorCount = 3;
const int sensorPins[sensorCount] = {34, 35, 32}; // Passe die Pins je nach deinem Aufbau an
const char *sensorNames[sensorCount] = {"Efeutute_rechts_oben", "Efeu_rechts", "Dreachenbaum_rechts"};

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
    // Erstelle eine eindeutige Client-ID
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

    // Erstelle eine JSON-Nachricht mit den Werten aller Sensoren
    String payload = "{";

    for (int i = 0; i < sensorCount; i++)
    {
        int sensorValue = analogRead(sensorPins[i]);
        Serial.printf("Rohwert von %s (Pin %d): %d\n", sensorNames[i], sensorPins[i], sensorValue);

        // Beispielhafte Umrechnung:
        // In deinem ursprünglichen Code wird der Wert mit map(sensorValue, 2000, 1200, 0, 100) umgerechnet.
        // Da map() immer ganzzahlige Werte liefert, hier eine Float-Variante.
        // Wir gehen davon aus, dass 2000 einen trockenen und 1200 einen sehr feuchten Zustand repräsentiert.
        float moisturePercent = 100.0 - ((sensorValue - 1200) * 100.0 / (2000 - 1200));
        // Werte begrenzen:
        if (moisturePercent < 0)
            moisturePercent = 0;
        if (moisturePercent > 100)
            moisturePercent = 100;

        // Zustandserkennung basierend auf dem Feuchtigkeitswert
        const char *status;
        if (moisturePercent <= 20)
        {
            status = "Bitte gießen";
        }
        else if (moisturePercent <= 40)
        {
            status = "Trocken";
        }
        else if (moisturePercent <= 70)
        {
            status = "Feucht genug";
        }
        else
        {
            status = "Sehr feucht";
        }

        // Sensor-Daten als JSON-Objekt hinzufügen
        payload += "\"";
        payload += sensorNames[i];
        payload += "\": {";
        payload += "\"moisture\": ";
        payload += String(moisturePercent, 2);
        payload += ", \"status\": \"";
        payload += status;
        payload += "\"}";

        if (i < sensorCount - 1)
        {
            payload += ", ";
        }
    }

    payload += "}";

    // Sende die Payload per MQTT
    if (client.publish(topic, payload.c_str()))
    {
        Serial.printf("Daten gesendet: %s an Topic: %s\n", payload.c_str(), topic);
    }
    else
    {
        Serial.println("Fehler beim Senden der Daten!");
    }

    delay(5000); // Warte 5 Sekunden bis zur nächsten Messung
}
