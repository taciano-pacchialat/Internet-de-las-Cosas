# Sistema de Geofencing Ganadero con LoRa

## Arquitectura

```
┌──────────────┐    LoRa 433MHz    ┌──────────────┐    Wi-Fi/MQTT    ┌──────────────────┐
│  Edge Module │ ◄───────────────► │   Gateway    │ ◄──────────────► │ Backend Services │
│  (ESP32)     │                   │  (ESP32-CAM) │                  │ (Docker Stack)   │
│              │                   │              │                  │                  │
│ • Deep Sleep │                   │ • Deep Sleep │                  │ • Mosquitto MQTT │
│ • Geofence   │                   │ • LoRa RX    │                  │ • Node-RED       │
│ • LEDs RTC   │                   │ • MQTT pub   │                  │ • InfluxDB 1.8   │
│ • LoRa TX/RX │                   │ • Downlink   │                  │ • Grafana        │
└──────────────┘                   └──────────────┘                  └──────────────────┘
```

## Directorios

- `edge-module/` — Firmware ESP32 clásico (nodo de campo)
- `gateway-module/` — Firmware ESP32-CAM (puente LoRa↔Wi-Fi)
- `backend-services/` — Stack Docker con MQTT, Node-RED, InfluxDB, Grafana

## Requisitos

- ESP-IDF >= v5.0
- Docker y Docker Compose
- Python 3.8+ (para idf.py)

## Levantar Backend

```bash
cd backend-services
docker-compose up -d
```

## Compilar Firmware

```bash
# Edge Module
cd edge-module
idf.py set-target esp32
idf.py build

# Gateway Module
cd gateway-module
idf.py set-target esp32
idf.py build
```

## Tópicos MQTT

| Tópico | Dirección | Descripción |
|--------|-----------|-------------|
| `geofence/heartbeat` | Gateway → Broker | Heartbeat del edge (JSON) |
| `geofence/simulated_gps` | Dashboard → Broker | GPS simulado con retain |
| `geofence/fence_update` | Dashboard → Broker | Límites del geofence con retain |
