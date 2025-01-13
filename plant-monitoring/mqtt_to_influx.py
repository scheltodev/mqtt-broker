import paho.mqtt.client as mqtt
from influxdb_client import InfluxDBClient, Point, WritePrecision
import os

# Umgebungsvariablen für Konfiguration
MQTT_BROKER = os.getenv("MQTT_BROKER", "mosquitto.monitoring.svc.cluster.local")
MQTT_PORT = int(os.getenv("MQTT_PORT", 1883))
INFLUX_TOKEN = os.getenv("INFLUX_TOKEN", "your-influxdb-token")
INFLUX_ORG = os.getenv("INFLUX_ORG", "your-org")
INFLUX_BUCKET = os.getenv("INFLUX_BUCKET", "plant_monitoring")
INFLUX_URL = os.getenv("INFLUX_URL", "http://influxdb.monitoring.svc.cluster.local:8086")

# InfluxDB-Client konfigurieren
client = InfluxDBClient(url=INFLUX_URL, token=INFLUX_TOKEN, org=INFLUX_ORG)
write_api = client.write_api(write_options=WritePrecision.MS)

# MQTT-Callback-Funktionen
def on_message(mqtt_client, userdata, msg):
    topic = msg.topic
    payload = float(msg.payload.decode())
    sensor_id = topic.split("/")[-1]

    point = Point("plant_data") \
        .tag("sensor", sensor_id) \
        .field("moisture", payload) \
        .time(None, WritePrecision.MS)

    write_api.write(bucket=INFLUX_BUCKET, org=INFLUX_ORG, record=point)
    print(f"Sensor {sensor_id}: {payload}% Feuchtigkeit gespeichert.")

# MQTT-Client konfigurieren
mqtt_client = mqtt.Client()
mqtt_client.on_message = on_message
mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
mqtt_client.subscribe("plants/#")
mqtt_client.loop_forever()
