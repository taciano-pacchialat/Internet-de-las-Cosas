# Backend Services — Geofence System

## Levantar el stack

```bash
cd backend-services
docker-compose up -d
```

## Servicios

| Servicio | Puerto | URL |
|----------|--------|-----|
| Mosquitto MQTT | 1883 | mqtt://localhost:1883 |
| Node-RED | 1880 | http://localhost:1880 |
| Node-RED Dashboard | 1880/ui | http://localhost:1880/ui |
| InfluxDB | 8086 | http://localhost:8086 |
| Grafana | 3000 | http://localhost:3000 |

## Primeros pasos

1. Levantar Docker: `docker-compose up -d`
2. Abrir Node-RED: http://localhost:1880
3. Instalar nodos requeridos (Manage Palette):
   - `node-red-contrib-influxdb`
   - `node-red-dashboard`
   - (Opcional) `node-red-contrib-telegrambot`
4. Importar el flow desde el menú si no carga automáticamente.
5. Configurar Grafana datasource si no se auto-provisiona.

## Tópicos MQTT

| Tópico | Dirección | Descripción |
|--------|-----------|-------------|
| `geofence/heartbeat` | Gateway → Broker | Heartbeat del edge (JSON) |
| `geofence/simulated_gps` | Dashboard → Broker | GPS simulado con retain |
| `geofence/fence_update` | Dashboard → Broker | Límites del geofence con retain |
