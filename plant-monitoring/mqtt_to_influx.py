import paho.mqtt.client as mqtt
from influxdb_client import InfluxDBClient, Point, WriteOptions, WritePrecision
import os
import json

# Node-Port und Node-IP verwenden
MQTT_BROKER = os.getenv("MQTT_BROKER", "192.168.2.8")   # Node-IP deines Clusters
MQTT_PORT = int(os.getenv("MQTT_PORT", 30605))            # NodePort für den MQTT-Broker
INFLUX_TOKEN = os.getenv("INFLUX_TOKEN", "SKjQ-USEJWvNhbyq8s-nmp7zpqAQuRTeYKNAGQntjtJ9iT-ceVvGO7P4133twTRcWroJoXugIISPCRxIrRmubA==")
INFLUX_ORG = os.getenv("INFLUX_ORG", "scheltoorg")
INFLUX_BUCKET = os.getenv("INFLUX_BUCKET", "plant")
INFLUX_URL = os.getenv("INFLUX_URL", "http://192.168.2.8:31861")  # NodePort für InfluxDB

# InfluxDB-Client konfigurieren
try:
    influx_client = InfluxDBClient(url=INFLUX_URL, token=INFLUX_TOKEN, org=INFLUX_ORG)
    write_api = influx_client.write_api(write_options=WriteOptions(batch_size=1))
    print("InfluxDB-Client erfolgreich verbunden.")
except Exception as e:
    print(f"Fehler bei der Verbindung zur InfluxDB: {e}")
    exit(1)

def process_sensor(sensor_id, moisture, status):
    """Schreibt die Sensordaten in InfluxDB."""
    try:
        point = Point("plant_data") \
            .tag("sensor", sensor_id) \
            .field("moisture", moisture) \
            .field("status", status) \
            .time(None, WritePrecision.MS)
        write_api.write(bucket=INFLUX_BUCKET, org=INFLUX_ORG, record=point)
        print(f"Sensor {sensor_id}: {moisture}% Feuchtigkeit ({status}) gespeichert.")
    except Exception as e:
        print(f"Fehler beim Speichern der Daten für Sensor {sensor_id}: {e}")

# MQTT-Callback-Funktion
def on_message(mqtt_client, userdata, msg):
    try:
        topic = msg.topic
        payload = msg.payload.decode()
        print(f"Empfangene Nachricht auf Topic {topic}: {payload}")

        # Versuche, die Payload als JSON zu interpretieren
        try:
            data = json.loads(payload)
        except json.JSONDecodeError:
            # Falls Payload kein gültiges JSON ist, interpretieren wir es als reinen Float-Wert (alte Variante)
            moisture = float(payload)
            status = "Unbekannt"
            # Sensor-ID: letzter Teil des Topics
            sensor_id = topic.split("/")[-1]
            process_sensor(sensor_id, moisture, status)
            return

        # Prüfe, ob es sich um das alte Format (einzelner Sensor) handelt:
        if isinstance(data, dict) and "moisture" in data and "status" in data:
            moisture = float(data.get("moisture", 0))
            status = data.get("status", "Unbekannt")
            # Sensor-ID aus dem Topic (alte Variante)
            sensor_id = topic.split("/")[-1]
            process_sensor(sensor_id, moisture, status)
        # Andernfalls nehmen wir an, dass mehrere Sensoren gesendet wurden (neue Variante)
        elif isinstance(data, dict):
            for sensor_id, sensor_data in data.items():
                try:
                    moisture = float(sensor_data.get("moisture", 0))
                    status = sensor_data.get("status", "Unbekannt")
                    process_sensor(sensor_id, moisture, status)
                except Exception as sensor_e:
                    print(f"Fehler beim Verarbeiten der Daten für {sensor_id}: {sensor_e}")
        else:
            print("Empfangenes JSON hat ein unerwartetes Format.")
    except Exception as e:
        print(f"Allgemeiner Fehler beim Verarbeiten der Nachricht: {e}")

# MQTT-Client konfigurieren
try:
    mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    mqtt_client.on_message = on_message
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    # Abonniere ein generelles Topic, das beide Varianten abdeckt.
    mqtt_client.subscribe("plants/#")
    print(f"Verbunden mit MQTT-Broker {MQTT_BROKER}:{MQTT_PORT}. Abonniere Topic 'plants/#'...")
except Exception as e:
    print(f"Fehler bei der Verbindung zum MQTT-Broker: {e}")
    exit(1)

# MQTT-Loop starten
try:
    mqtt_client.loop_forever()
except KeyboardInterrupt:
    print("\nMQTT-Client beendet.")
    mqtt_client.disconnect()
    print("Verbindung zum MQTT-Broker getrennt.")
