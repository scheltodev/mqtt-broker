/*
 * ============================================================================
 * Pflanzen-Gießmonitoring 2.0 — Sensor-Kalibrierungs-Tool
 * ============================================================================
 * Mit diesem Tool ermittelst du die exakten Grenzwerte für jeden deiner
 * kapazitiven Feuchtigkeitssensoren (v1.2).
 *
 * ANLEITUNG:
 * 1. Flashe diesen Code auf deinen ESP32.
 * 2. Öffne den Serial Monitor (Baudrate: 115200, "Both NL & CR").
 * 3. Trockenwert (Air):
 *    - Halte den Sensor komplett trocken in die Luft.
 *    - Tippe 'd' in den Serial Monitor und drücke Enter.
 * 4. Nasswert (Water):
 *    - Stelle den Sensor bis zur weißen Grenzlinie in ein Glas Wasser.
 *    - Berühre das Glas/Wasser nicht mit den Fingern (Kapazitätsverfälschung!).
 *    - Warte 5 Sekunden, tippe 'w' in den Serial Monitor und drücke Enter.
 * 5. Der ESP32 gibt dir den fertigen C++-Konfigurationsblock aus!
 * ============================================================================
 */

#include <Arduino.h>

// Pins der Sensoren, die kalibriert werden sollen (müssen auf ADC1 liegen: 32, 33, 34, 35, 36, 39)
const int SENSOR_PINS[] = {34, 35, 32};
const char* SENSOR_NAMES[] = {"Efeutute", "Efeu", "Drachenbaum"};
const int NUM_SENSORS = sizeof(SENSOR_PINS) / sizeof(SENSOR_PINS[0]);

// Optional: Power-Gating-Pin (falls VCC des Sensors über GPIO geschaltet wird)
// Auf -1 setzen, falls der Sensor direkt an 3.3V hängt.
const int POWER_PIN = -1; // z. B. 25

int dryValues[NUM_SENSORS] = {0};
int wetValues[NUM_SENSORS] = {0};
int currentSensorIndex = 0;

// Filterung: 32 Samples einlesen, sortieren, Ausreißer verwerfen (Trimmed Mean)
int readFilteredADC(int pin) {
  const int SAMPLES = 32;
  int raw[SAMPLES];

  for (int i = 0; i < SAMPLES; i++) {
    raw[i] = analogRead(pin);
    delay(3);
  }

  // Simple Bubble-Sort
  for (int i = 0; i < SAMPLES - 1; i++) {
    for (int j = 0; j < SAMPLES - i - 1; j++) {
      if (raw[j] > raw[j + 1]) {
        int temp = raw[j];
        raw[j] = raw[j + 1];
        raw[j + 1] = temp;
      }
    }
  }

  // Mittlere 50% der Werte mitteln (Ausreißer an Rändern ignorieren)
  long sum = 0;
  int startIdx = SAMPLES / 4;
  int endIdx = SAMPLES - startIdx;
  for (int i = startIdx; i < endIdx; i++) {
    sum += raw[i];
  }
  return sum / (endIdx - startIdx);
}

void printStatus() {
  Serial.println("\n------------------------------------------------------------");
  Serial.printf("AKTUELLES ZIEL: Sensor %d [%s] an Pin %d\n", 
                currentSensorIndex + 1, 
                SENSOR_NAMES[currentSensorIndex], 
                SENSOR_PINS[currentSensorIndex]);
  Serial.println("Befehle im Serial Monitor:");
  Serial.println("  'd' -> Aktuellen Wert als TROCKEN (Luft) speichern");
  Serial.println("  'w' -> Aktuellen Wert als NASS (Wasserglas) speichern");
  Serial.println("  'n' -> Weiter zum nächsten Sensor");
  Serial.println("  'p' -> Fertigen Code-Block ausgeben");
  Serial.println("------------------------------------------------------------\n");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  analogReadResolution(12); // 12-Bit (0-4095)
  analogSetAttenuation(ADC_11db);

  if (POWER_PIN >= 0) {
    pinMode(POWER_PIN, OUTPUT);
    digitalWrite(POWER_PIN, HIGH);
  }

  Serial.println("\n============================================================");
  Serial.println("   Pflanzen-Gießmonitoring 2.0 — Sensor-Kalibrierung        ");
  Serial.println("============================================================");
  printStatus();
}

void loop() {
  if (POWER_PIN >= 0) {
    digitalWrite(POWER_PIN, HIGH);
    delay(20);
  }

  int activePin = SENSOR_PINS[currentSensorIndex];
  int liveADC = readFilteredADC(activePin);

  // Live-Ausgabe alle 800ms
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 800) {
    lastPrint = millis();
    Serial.printf("Sensor [%s, Pin %d] Live-ADC: %d", SENSOR_NAMES[currentSensorIndex], activePin, liveADC);
    if (dryValues[currentSensorIndex] > 0 && wetValues[currentSensorIndex] > 0) {
      int dry = dryValues[currentSensorIndex];
      int wet = wetValues[currentSensorIndex];
      float pct = ((float)(dry - liveADC) / (float)(dry - wet)) * 100.0f;
      pct = constrain(pct, 0.0f, 100.0f);
      Serial.printf(" | Berechnete Feuchtigkeit: %.1f %%", pct);
    }
    Serial.println();
  }

  // Benutzereingaben über Serial auswerten
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == '\r' || cmd == '\n' || cmd == ' ') return;

    switch (cmd) {
      case 'd':
      case 'D':
        dryValues[currentSensorIndex] = liveADC;
        Serial.printf("\n>>> GESPEICHERT: Sensor [%s] Trockenwert (Air) = %d <<<\n\n", SENSOR_NAMES[currentSensorIndex], liveADC);
        break;

      case 'w':
      case 'W':
        wetValues[currentSensorIndex] = liveADC;
        Serial.printf("\n>>> GESPEICHERT: Sensor [%s] Nasswert (Water) = %d <<<\n\n", SENSOR_NAMES[currentSensorIndex], liveADC);
        break;

      case 'n':
      case 'N':
        currentSensorIndex = (currentSensorIndex + 1) % NUM_SENSORS;
        printStatus();
        break;

      case 'p':
      case 'P':
        Serial.println("\n============================================================");
        Serial.println("// KOPIERE DIESEN BLOCK IN DEINE plant_monitor_firmware.cpp:");
        Serial.println("const SensorConfig SENSORS[] = {");
        for (int i = 0; i < NUM_SENSORS; i++) {
          int d = dryValues[i] > 0 ? dryValues[i] : 2800;
          int w = wetValues[i] > 0 ? wetValues[i] : 1300;
          Serial.printf("  {\"%s\", %d, %d, %d}%s\n", 
                        SENSOR_NAMES[i], 
                        SENSOR_PINS[i], 
                        d, 
                        w, 
                        (i < NUM_SENSORS - 1) ? "," : "");
        }
        Serial.println("};");
        Serial.println("============================================================\n");
        break;

      default:
        Serial.printf("Unbekannter Befehl '%c'. Erlaubt: d, w, n, p\n", cmd);
        break;
    }
  }
}
