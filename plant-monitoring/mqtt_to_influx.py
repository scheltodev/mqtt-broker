import paho.mqtt.client as mqtt
from influxdb_client import InfluxDBClient, Point, WriteOptions, WritePrecision
import os
import json

# Node-Port und Node-IP verwenden
MQTT_BROKER = os.getenv("MQTT_BROKER", "192.168.2.8")  # Node-IP deines Clusters
MQTT_PORT = int(os.getenv("MQTT_PORT", 30605))         # NodePort für den MQTT-Broker
INFLUX_TOKEN = os.getenv("INFLUX_TOKEN", "SKjQ-USEJWvNhbyq8s-nmp7zpqAQuRTeYKNAGQntjtJ9iT-ceVvGO7P4133twTRcWroJoXugIISPCRxIrRmubA==")
INFLUX_ORG = os.getenv("INFLUX_ORG", "scheltoorg")
INFLUX_BUCKET = os.getenv("INFLUX_BUCKET", "plant")
INFLUX_URL = os.getenv("INFLUX_URL", "http://192.168.2.8:31861")  # NodePort für InfluxDB

# InfluxDB-Client konfigurieren
try:
    client = InfluxDBClient(url=INFLUX_URL, token=INFLUX_TOKEN, org=INFLUX_ORG)
    write_api = client.write_api(write_options=WriteOptions(batch_size=1))
    print("InfluxDB-Client erfolgreich verbunden.")
except Exception as e:
    print(f"Fehler bei der Verbindung zur InfluxDB: {e}")
    exit(1)

# MQTT-Callback-Funktion

def on_message(mqtt_client, userdata, msg):
    try:
        topic = msg.topic
        payload = msg.payload.decode()
        sensor_id = topic.split("/")[-1]
        
        # Prüfen, ob Payload JSON oder einfacher Float ist
        if payload.startswith("{"):
            data = json.loads(payload)  # JSON-Daten dekodieren
            moisture = float(data.get("moisture", 0))
            status = data.get("status", "Unbekannt")
        else:
            moisture = float(payload)
            status = "Unbekannt"
        
        # Punkt für die InfluxDB erstellen
        point = Point("plant_data") \
            .tag("sensor", sensor_id) \
            .field("moisture", moisture) \
            .field("status", status) \
            .time(None, WritePrecision.MS)
        
        # Daten in die InfluxDB schreiben
        write_api.write(bucket=INFLUX_BUCKET, org=INFLUX_ORG, record=point)
        print(f"Sensor {sensor_id}: {moisture}% Feuchtigkeit ({status}) gespeichert.")
    except ValueError as ve:
        print(f"Fehler beim Dekodieren der Nachricht: {ve}")
    except Exception as e:
        print(f"Allgemeiner Fehler beim Verarbeiten der Nachricht: {e}")

# MQTT-Client konfigurieren
try:
    mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    mqtt_client.on_message = on_message
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
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
