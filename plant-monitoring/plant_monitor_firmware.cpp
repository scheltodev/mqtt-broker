/*
 * ============================================================================
 * Pflanzen-Gießmonitoring 2.0 — ESP32 Production Firmware
 * ============================================================================
 * Features:
 *  - Rauschunterdrückung: 32x Oversampling + Trimmed Mean Filter
 *  - Individuelle 2-Punkt-Kalibrierung pro Sensor (Trocken / Nass)
 *  - Sensor Power-Gating (verhindert Erwärmung / Spart Strom für Akkubetrieb)
 *  - Deep-Sleep Vorbereitung (Umschaltbar zwischen USB-Dauerbetrieb und Akkumodus)
 *  - MQTT Last Will & Testament (LWT) zur automatischen Offline-Erkennung
 *  - Strukturiertes Telemetrie-JSON (Rohwert, Prozent, RSSI, Akkuspannung)
 * ============================================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>

// ============================================================================
// 1. KONFIGURATION & CREDENTIALS
// ============================================================================

// WLAN
const char* WIFI_SSID     = "WLAN-504009_EXT";
const char* WIFI_PASSWORD = "2764193621847697";

// MQTT Broker (scanemall K8s Mosquitto NodePort oder Cluster Ingress)
const char* MQTT_BROKER   = "192.168.2.8";
const int   MQTT_PORT     = 30605;
const char* DEVICE_ID     = "esp32_plants";

// MQTT Topics
const char* TOPIC_TELEMETRY = "scanemall/plants/esp32_plants/telemetry";
const char* TOPIC_STATUS    = "scanemall/plants/esp32_plants/status";

// Betriebsmodus:
// true  = Deep Sleep nach jedem Sendezyklus (für Batterie-/Akkubetrieb)
// false = Dauerbetrieb über USB mit delay/Timer
#define ENABLE_DEEP_SLEEP false

// Sendeintervall (in Sekunden): Pflanzen trocknen langsam, 10-15 Min sind ideal
const uint32_t MEASURE_INTERVAL_SEC = 300; // 5 Minuten (300s)

// Optionales Power-Gating: Sensor-VCC an GPIO hängen (z.B. 25 oder 26)
// Auf -1 setzen, wenn die Sensoren fest mit 3.3V verbunden sind.
const int SENSOR_POWER_PIN = -1;

// ============================================================================
// 2. SENSOR-DEFINITIONEN & KALIBRIERWERTE
// ============================================================================
// HINWEIS: Ersetze diese Werte durch die Ausgabe des Kalibrierungstools!
// Pins müssen auf ADC1 liegen: 32, 33, 34, 35, 36, 39
struct SensorConfig {
  const char* id;
  int pin;
  int rawDry;  // In der Luft gemessen (0% Feuchte)
  int rawWet;  // Im Wasserglas gemessen (100% Feuchte)
};

const SensorConfig SENSORS[] = {
  {"efeutute_rechts_oben", 34, 2850, 1320},
  {"efeu_rechts",          35, 2790, 1380},
  {"drachenbaum_rechts",   32, 2820, 1350}
};
const int NUM_SENSORS = sizeof(SENSORS) / sizeof(SENSORS[0]);

// ============================================================================
// 3. HARDWARE & ADC MESSLOGIK
// ============================================================================
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Liest den ADC mit 32-fachem Oversampling und verwirft Ausreißer (Trimmed Mean)
int readFilteredADC(int pin) {
  const int SAMPLES = 32;
  int raw[SAMPLES];

  for (int i = 0; i < SAMPLES; i++) {
    raw[i] = analogRead(pin);
    delay(3);
  }

  // Sortieren
  for (int i = 0; i < SAMPLES - 1; i++) {
    for (int j = 0; j < SAMPLES - i - 1; j++) {
      if (raw[j] > raw[j + 1]) {
        int temp = raw[j];
        raw[j] = raw[j + 1];
        raw[j + 1] = temp;
      }
    }
  }

  // Mittlere 50% aufsummieren
  long sum = 0;
  int startIdx = SAMPLES / 4;
  int endIdx = SAMPLES - startIdx;
  for (int i = startIdx; i < endIdx; i++) {
    sum += raw[i];
  }
  return (int)(sum / (endIdx - startIdx));
}

// Rechnet Rohwert in kalibrierte Feuchte in Prozent (0 - 100%) um
float calculateMoisturePercent(int rawValue, int rawDry, int rawWet) {
  if (rawDry == rawWet) return 0.0f;
  float pct = ((float)(rawDry - rawValue) / (float)(rawDry - rawWet)) * 100.0f;
  return constrain(pct, 0.0f, 100.0f);
}

// ============================================================================
// 4. NETZWERK & MQTT
// ============================================================================
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.printf("Verbinde mit WLAN '%s'", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(250);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWLAN verbunden! IP: %s | RSSI: %d dBm\n", 
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println("\nWLAN-Verbindung fehlgeschlagen!");
  }
}

bool connectMQTT() {
  if (mqttClient.connected()) return true;

  Serial.printf("Verbinde mit MQTT Broker %s:%d ... ", MQTT_BROKER, MQTT_PORT);
  String clientId = String(DEVICE_ID) + "-" + String(random(0xffff), HEX);

  // LWT: Falls der ESP32 unerwartet offline geht, meldet der Broker "offline"
  bool connected = mqttClient.connect(
    clientId.c_str(),
    nullptr, nullptr,                // User / Password (falls vorhanden)
    TOPIC_STATUS, 1, true, "offline" // LWT Topic, QoS 1, Retain = true, Payload
  );

  if (connected) {
    Serial.println("Erfolgreich!");
    // Status sofort auf "online" setzen
    mqttClient.publish(TOPIC_STATUS, "online", true);
    return true;
  } else {
    Serial.printf("Fehlgeschlagen! State-Code: %d\n", mqttClient.state());
    return false;
  }
}

// ============================================================================
// 5. MESSUNG & TELEMETRIE
// ============================================================================
void performMeasurementAndPublish() {
  // 1. Sensoren einschalten (Power-Gating)
  if (SENSOR_POWER_PIN >= 0) {
    digitalWrite(SENSOR_POWER_PIN, HIGH);
    delay(30); // 30ms Einschwingzeit für Oszillatoren
  }

  // 2. Sensoren auslesen
  int rawValues[NUM_SENSORS];
  float moistureValues[NUM_SENSORS];

  for (int i = 0; i < NUM_SENSORS; i++) {
    rawValues[i] = readFilteredADC(SENSORS[i].pin);
    moistureValues[i] = calculateMoisturePercent(rawValues[i], SENSORS[i].rawDry, SENSORS[i].rawWet);
  }

  // 3. Sensoren ausschalten (spart Strom / schont Sensor)
  if (SENSOR_POWER_PIN >= 0) {
    digitalWrite(SENSOR_POWER_PIN, LOW);
  }

  // 4. Telemetrie-JSON zusammenbauen
  // Format:
  // {
  //   "device_id": "esp32_plants",
  //   "wifi_rssi": -65,
  //   "battery_mv": 3300,
  //   "uptime_s": 120,
  //   "plants": [
  //     {"id": "efeutute_rechts_oben", "pin": 34, "raw": 2720, "moisture": 58.4}, ...
  //   ]
  // }
  String payload = "{";
  payload += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  payload += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
  payload += "\"battery_mv\":3300,"; // Dummy oder via Teiler an ADC gemessen
  payload += "\"uptime_s\":" + String(millis() / 1000) + ",";
  payload += "\"plants\":[";

  for (int i = 0; i < NUM_SENSORS; i++) {
    payload += "{";
    payload += "\"id\":\"" + String(SENSORS[i].id) + "\",";
    payload += "\"pin\":" + String(SENSORS[i].pin) + ",";
    payload += "\"raw\":" + String(rawValues[i]) + ",";
    payload += "\"moisture\":" + String(moistureValues[i], 1);
    payload += "}";
    if (i < NUM_SENSORS - 1) payload += ",";

    Serial.printf("  - %-20s | Pin %2d | Raw: %4d | Feuchte: %5.1f %%\n",
                  SENSORS[i].id, SENSORS[i].pin, rawValues[i], moistureValues[i]);
  }
  payload += "]}";

  // 5. Per MQTT publishen
  if (mqttClient.publish(TOPIC_TELEMETRY, payload.c_str(), false)) {
    Serial.println("Telemetrie erfolgreich gesendet!");
  } else {
    Serial.println("Fehler beim Senden der Telemetrie!");
  }
}

// ============================================================================
// SETUP & LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n============================================================");
  Serial.println("      Pflanzen-Gießmonitoring 2.0 — ESP32 Gestartet         ");
  Serial.println("============================================================");

  // ADC auf 12 Bit und 11dB Abschwächung (0 - 3.1V Messbereich) konfigurieren
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  if (SENSOR_POWER_PIN >= 0) {
    pinMode(SENSOR_POWER_PIN, OUTPUT);
    digitalWrite(SENSOR_POWER_PIN, LOW);
  }

  // Netzwerkverbindungen aufbauen
  connectWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setKeepAlive(60);
  connectMQTT();

  // Erste Messung ausführen
  performMeasurementAndPublish();

#if ENABLE_DEEP_SLEEP
  Serial.printf("Gehe für %u Sekunden in Deep Sleep...\n", MEASURE_INTERVAL_SEC);
  mqttClient.disconnect();
  WiFi.disconnect(true);
  esp_sleep_enable_timer_wakeup((uint64_t)MEASURE_INTERVAL_SEC * 1000000ULL);
  esp_deep_sleep_start();
#endif
}

void loop() {
#if !ENABLE_DEEP_SLEEP
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();

  static unsigned long lastMeasure = 0;
  if (millis() - lastMeasure > ((unsigned long)MEASURE_INTERVAL_SEC * 1000)) {
    lastMeasure = millis();
    performMeasurementAndPublish();
  }

  delay(50);
#endif
}
