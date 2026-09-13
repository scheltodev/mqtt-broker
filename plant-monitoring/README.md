# 🌿 Pflanzen-Gießmonitoring 2.0 (Next Level)

End-to-End IoT-Gießmonitoring mit **ESP32**, **ESPHome**, **Mosquitto**, **VictoriaMetrics** und **Grafana**, vollständig integriert in die GitOps-Infrastruktur von `scanemall-infra`.

---

## 📋 Übersicht der Komponenten

1. **Hardware & Firmware:**
   - **ESP32-WROOM-32** mit ESPHome (`plant-monitor.yaml`).
   - 3x **Capacitive Soil Moisture Sensor v1.2** an ADC1 (GPIO34, GPIO35, GPIO32).
   - 15x Median-Rauschfilter gegen ESP32-ADC-Schwankungen.
   - Individuelle 2-Punkt-Spannungskalibrierung pro Pflanze.
   - Integrierter ESP32-Webserver für Live-Werte & drahtlose OTA-Updates.

2. **Kubernetes-Infrastruktur (`scanemall-infra/k8s/apps/plant-monitoring`):**
   - **ArgoCD Application:** Automatische Bereitstellung & Sync.
   - **Mosquitto MQTT Broker:** Empfängt Telemetrie über NodePort `30605`.
   - **Telegraf Pipeline:** Abonniert MQTT-Topics und schreibt per Influx Line Protocol direkt an `http://victoria-metrics:8428/write`.
   - **VictoriaMetrics:** Speichert Metriken effizient ohne zusätzliche InfluxDB.

3. **Monitoring & Visualisierung:**
   - **Grafana Dashboard** (`grafana_dashboard.json`) mit Live-Gauges, pflanzenspezifischen Schwellenwerten, Trocknungsverlauf und Sensor-Diagnose.

---

## 🛠️ Schritt 1: Sensor-Hardware vorbereiten (Wichtig!)

Bevor die Sensoren in die Erde gesteckt werden, muss der **Kapillareffekt verhindert werden**:
1. Die seitlichen Schnittkanten der Platine und die oberen Bauteile (IC, Widerstände, Lötstellen des Kabels) mit **klarem Nagellack, Heißkleber oder Epoxidharz** einstreichen.
2. Der weiße Messstreifen am unteren Ende **bleibt frei**.
3. *Ergebnis:* Es kann kein Wasser mehr in die Glasfaser-Schichten des PCBs kriechen. Der Sensor hält dauerhaft stabil ohne Drift.

---

## ⚡ Schritt 2: ESPHome flashen

### Option A: Über den Browser (Am einfachsten)
1. Verbinde den ESP32 per USB-Kabel mit dem PC.
2. Öffne **[web.esphome.io](https://web.esphome.io/)** in Chrome oder Edge.
3. Klicke auf **"Install"**, wähle den ESP32-COM-Port aus und installiere ESPHome.
4. Danach kannst du die `plant-monitor.yaml` über das Web-Dashboard hochladen.

### Option B: Über die Kommandozeile
```bash
# 1. ESPHome installieren (falls noch nicht vorhanden)
pip install esphome

# 2. Ersten Flash per USB durchführen
esphome run plant-monitor.yaml

# 3. Zukünftige Updates laufen automatisch drahtlos über WLAN (OTA)!
```

---

## ⚖️ Schritt 3: Kalibrierung der Sensoren

Jeder kapazitive Sensor hat fertigungsbedingte Toleranzen. In `plant-monitor.yaml` sind bereits getrennte Sensoren für die **Rohspannung (V)** angelegt:
- `Efeutute Rohspannung`
- `Efeu Rohspannung`
- `Drachenbaum Rohspannung`

### So ermittelst du die exakten Werte:
1. Öffne im Browser die IP des ESP32 (oder beobachte den Serial Monitor).
2. **Trockenwert (Luft):** Halte den Sensor trocken in die Luft. Lies die Spannung ab (z. B. `2.84 V`).
3. **Nasswert (Wasser):** Stecke den Sensor bis zur Begrenzungslinie in ein Glas Wasser. Lies die Spannung ab (z. B. `1.32 V`).
4. Öffne `plant-monitor.yaml` und trage die Werte ein:
   ```yaml
   - calibrate_linear:
       - 2.84 -> 0.0     # Luft = 0% Feuchte
       - 1.32 -> 100.0   # Wasser = 100% Feuchte
   ```
5. Führe `esphome run plant-monitor.yaml` aus – das Update wird **kabellos über WLAN** aufgespielt!

---

## 🚀 Schritt 4: Deployment in Kubernetes (`scanemall-infra`)

Die Kubernetes-Manifeste sind bereits in `scanemall-infra/k8s/apps/plant-monitoring/` abgelegt:
- `application.yml` (ArgoCD Application)
- `mosquitto.yml` (Mosquitto Broker)
- `telegraf.yml` (MQTT -> VictoriaMetrics Pipeline)

Sobald du das Git-Repository pushst, synchronisiert ArgoCD automatisch die Workloads in den Namespace `monitoring`.

Alternativ manuell anwenden:
```bash
kubectl apply -f scanemall-infra/k8s/apps/plant-monitoring/ -n monitoring
```

---

## 📊 Schritt 5: Grafana Dashboard importieren

1. Öffne Grafana unter `https://grafana.scanemall.duckdns.org`.
2. Gehe im linken Menü auf **Dashboards** $\rightarrow$ **New** $\rightarrow$ **Import**.
3. Lade die Datei `grafana_dashboard.json` hoch oder füge den Inhalt ein.
4. Klicke auf **Import**.

### Enthaltene Panels:
* **Gauges:** Pflanzenspezifische Farbzonen (Efeutute braucht z. B. mehr Feuchte als ein Drachenbaum).
* **Verlaufsgraph:** Erkennung von Gießzeitpunkten (Peaks) und Trocknungsgeschwindigkeit.
* **Rohspannungs-Diagnose:** Früherkennung von Sensor-Verschmutzung oder Drift.
* **ESP32 WiFi Signal:** Zuverlässigkeits-Überwachung.

---

## 🔋 Bonus: Akkubetrieb (Deep Sleep)
Wenn du später auf Akkubetrieb umsteigen willst, musst du in `plant-monitor.yaml` lediglich die letzten 4 Zeilen einkommentieren:
```yaml
deep_sleep:
  run_duration: 30s
  sleep_duration: 30min
```
Der ESP32 wacht dann alle 30 Minuten auf, verbindet sich mit dem WLAN, sendet die Werte und schläft sofort wieder ein. Mit einem Standard-18650-Akku läuft das Setup monatelang.
