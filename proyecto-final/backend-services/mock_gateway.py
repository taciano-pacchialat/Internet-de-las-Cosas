#!/usr/bin/env python3
"""
Mock Gateway LoRa - Simulador Automatizado para QA IoT (Geofencing Ganadero)
-----------------------------------------------------------------------------
Simula la recepción de tramas del collar ganadero (Edge Module) y su publicación
hacia el broker MQTT local (Mosquitto) con un recorrido aleatorio (Random Walk)
para validar el pipeline ETL de Node-RED, InfluxDB v1.8 y el panel Geomap en Grafana.

Requisitos:
    pip install paho-mqtt
Uso:
    python3 mock_gateway.py
"""

import json
import math
import random
import time
import argparse
from datetime import datetime
import paho.mqtt.client as mqtt

# =============================================================================
# CONFIGURACIÓN POR DEFECTO
# =============================================================================
DEFAULT_BROKER_HOST = "localhost"
DEFAULT_BROKER_PORT = 1883
TOPIC_HEARTBEAT = "geofence/heartbeat"
PUBLISH_INTERVAL_SEC = 5.0

# Coordenada central de la geocerca (La Plata, Argentina)
CENTER_LAT = -34.920500
CENTER_LON = -57.953600
GEOFENCE_RADIUS_METERS = 80.0  # Radio del perímetro seguro


def calculate_distance_meters(lat1, lon1, lat2, lon2):
    """Calcula la distancia aproximada en metros entre dos coordenadas geográficas."""
    R = 6378137.0  # Radio medio de la Tierra en metros
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = (math.sin(dlat / 2)**2 +
         math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.sin(dlon / 2)**2)
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
    return R * c


class CattleRandomWalkSimulator:
    """Simula el movimiento de una res en el campo usando un Random Walk acotado."""

    def __init__(self, device_id="edge-mock-qa01", start_lat=CENTER_LAT, start_lon=CENTER_LON):
        self.device_id = device_id
        self.lat = start_lat
        self.lon = start_lon
        self.wake_count = 0
        self.battery = 98.5

    def step(self):
        self.wake_count += 1

        # Paso aleatorio de ~5 a ~15 metros por iteración (0.00005 a 0.00015 grados)
        step_lat = random.uniform(-0.00012, 0.00012)
        step_lon = random.uniform(-0.00012, 0.00012)

        self.lat += step_lat
        self.lon += step_lon

        # Descarga ligera de batería simulada
        self.battery = max(0.0, self.battery - random.uniform(0.01, 0.05))

        # Calcular distancia al centro para determinar estado del geofence
        dist = calculate_distance_meters(CENTER_LAT, CENTER_LON, self.lat, self.lon)
        fence_status = "INSIDE" if dist <= GEOFENCE_RADIUS_METERS else "OUTSIDE"

        # Simular métricas RF LoRa (RSSI / SNR) realistas
        rssi = int(random.uniform(-105, -70))
        snr = round(random.uniform(4.0, 11.5), 1)

        payload = {
            "device_id": self.device_id,
            "lat": round(self.lat, 6),
            "lon": round(self.lon, 6),
            "fence_status": fence_status,
            "battery": round(self.battery, 1),
            "rssi": rssi,
            "snr": snr,
            "wake_count": self.wake_count
        }
        return payload, dist


def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print("✅ Conectado exitosamente a Mosquitto MQTT Broker")
    else:
        print(f"❌ Error al conectar a Mosquitto: código {reason_code}")


def main():
    parser = argparse.ArgumentParser(description="Mock Gateway LoRa para QA de Geofencing IoT")
    parser.add_argument("--host", default=DEFAULT_BROKER_HOST, help="MQTT Broker Host (default: localhost)")
    parser.add_argument("--port", type=int, default=DEFAULT_BROKER_PORT, help="MQTT Broker Port (default: 1883)")
    parser.add_argument("--interval", type=float, default=PUBLISH_INTERVAL_SEC, help="Intervalo entre publicaciones en seg")
    parser.add_argument("--device", default="edge-mock-qa01", help="ID de dispositivo simulado")
    args = parser.parse_args()

    # Inicializar cliente MQTT (compatible con paho-mqtt v2.x)
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"mock-gateway-{random.randint(1000, 9999)}")
    client.on_connect = on_connect

    print(f"🚀 Conectando a MQTT Broker en {args.host}:{args.port}...")
    try:
        client.connect(args.host, args.port, 60)
        client.loop_start()
    except Exception as e:
        print(f"❌ No se pudo conectar a {args.host}:{args.port} -> {e}")
        return

    simulator = CattleRandomWalkSimulator(device_id=args.device)

    print("=========================================================================")
    print(f"📡 Iniciando Mock Gateway -> Tópico: {TOPIC_HEARTBEAT}")
    print(f"📍 Centro Geocerca: ({CENTER_LAT}, {CENTER_LON}) | Radio: {GEOFENCE_RADIUS_METERS}m")
    print("=========================================================================\n")

    try:
        while True:
            payload_dict, dist = simulator.step()
            payload_json = json.dumps(payload_dict)

            info_client = client.publish(TOPIC_HEARTBEAT, payload_json, qos=1)
            info_client.wait_for_publish()

            status_icon = "🟢" if payload_dict["fence_status"] == "INSIDE" else "🔴 ALERTA"
            now_str = datetime.now().strftime("%H:%M:%S")
            print(
                f"[{now_str}] #{payload_dict['wake_count']:03d} TX -> "
                f"Lat: {payload_dict['lat']:.6f}, Lon: {payload_dict['lon']:.6f} | "
                f"Dist: {dist:5.1f}m | {status_icon} ({payload_dict['fence_status']}) | "
                f"Bat: {payload_dict['battery']}% | RSSI: {payload_dict['rssi']}dBm"
            )

            time.sleep(args.interval)

    except KeyboardInterrupt:
        print("\n🛑 Simulación detenida por el usuario. Desconectando...")
        client.loop_stop()
        client.disconnect()


if __name__ == "__main__":
    main()
