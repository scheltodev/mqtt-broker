FROM python:3.10-slim

WORKDIR /app
COPY . /app

RUN pip install paho-mqtt influxdb-client

CMD ["python", "mqtt_to_influx.py"]
