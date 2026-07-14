# Hallazgos y Descubrimientos (Findings)

## Frontend (`App.tsx` & `Sidebar.tsx` & `fenceService.ts`)
- `selectedVertexIndex`, `moveVertex` y el event listener de teclado para flechas (`ArrowUp`, `ArrowDown`, `ArrowLeft`, `ArrowRight` con paso 0.0001 y 0.0005 con `Shift`) están operando en `App.tsx` y `Sidebar.tsx`.
- En `fenceService.ts`, `fetchCurrentGeofence()` hace `GET /api/fence/current` y espera una respuesta `{ fence: { vertices: [{lat, lon}, ...] } }`.
- En `sendGeofenceUpdate()`, el POST va a `/api/fence` (y si falla hace fallback a `/api/node-red/api/update-fence`).

## Enrutamiento Nginx (`nginx.conf` & `vite.config.ts`)
- Se agregaron exitosamente las ubicaciones para `/api/fence/` y `/api/update-fence` en `nginx.conf`, redirigiendo el tráfico hacia `http://geofence-nodered:1880/api/fence/` y `http://geofence-nodered:1880/api/update-fence` respectivamente.
- En `vite.config.ts` se configuraron los proxies inversos correspondientes para el entorno de desarrollo (`npm run dev`).

## Node-RED (`flows.json`) y Persistencia
- Se crearon los endpoints `POST /api/update-fence` (y `POST /api/fence`), que guardan el perímetro en la medición `geofence_config` de InfluxDB (`vertices_json`, `min_lat`, `max_lat`, `min_lon`, `max_lon`, `vertex_count`) y publican por MQTT al tópico `geofence/config_update`.
- Se creó el endpoint `GET /api/fence/current` (y aliases de compatibilidad), que consulta `SELECT last(*) FROM geofence_config` y retorna la estructura `{ status: "success", fence: { vertices: [...], min_lat, max_lat, min_lon, max_lon } }`.
- Se implementó un nodo `inject` de inicialización (`once: true`) conectado a una consulta de InfluxDB y al nodo `restore_fence_to_context`, el cual al arrancar Node-RED recarga automáticamente en memoria (`global` y `flow` context) todos los límites y vértices persistidos.
- Se actualizó `tg_check_outside_db` con el algoritmo de ray-casting (`pointInPolygon`) que evalúa geometrías arbitrarias persistidas en `vertices_json` para generar alarmas precisas ante violaciones de perímetro.
