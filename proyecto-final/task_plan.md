# Plan de Tareas: Panel de Vértices HUD, Alertas de Telegram y Persistencia de Geofence (v3)

## Objetivo
Reimplementar el panel técnico de control de vértices en el frontend, consolidar el mecanismo de alertas de Telegram proactivas desde InfluxDB, y asegurar que los límites y vértices del perímetro (geofence) se almacenen y recuperen de forma persistente desde la base de datos (`InfluxDB`), sobreviviendo a reinicios de Docker (`geofence-nodered` y `geofence-influxdb`).

## Fases y Estado

### Fase 1: Panel de Control de Vértices en Frontend
- [x] Pasar props `currentFence`, `selectedVertexIndex`, `onSelectVertex` de `App.tsx` a `Sidebar.tsx`.
- [x] Crear componente `VertexControlPanel` dentro de `Sidebar.tsx` debajo de la tarjeta "Último Registro".
- [x] Mostrar etiquetas cardinales (NO, NE, SE, SO), coordenadas exactas con 6 decimales, indicador visual técnico `hud-accent` al seleccionar y leyendas claras para mover con flechas del teclado.
- [x] Botón de "Deseleccionar" y acción para guardar cambios del perímetro.
- **Status:** complete

### Fase 2: Alertas Automáticas de Telegram desde InfluxDB
- [x] En Node-RED (`flows.json`), crear subflujo de consulta periódica a InfluxDB (`inject` cada 15s).
- [x] Conectar a `influxdb in` para consultar la última posición.
- [x] Crear nodo de función `tg_check_outside_db` que analice la geometría contra `fence_min_lat/max_lat/min_lon/max_lon`.
- [x] Si `isInside === false` y transcurrieron >= 40s desde la última notificación, enviar alerta al `chatId: 5278783015`.
- **Status:** complete

### Fase 3: Comandos y Control por Telegram
- [x] Comandos `/modulos`, `/detalle_<id>`, `/silenciar`, `/alertas` en Node-RED (`flows.json`).
- [x] Menú interactivo Inline Keyboard para silenciar/activar alertas por módulo de borde y persistencia en context.
- **Status:** complete

### Fase 4: Persistencia del Geofence en Base de Datos (InfluxDB) & Sincronización Total
- [x] Definir esquema en InfluxDB (`geofence_config` measurement) para guardar `min_lat`, `max_lat`, `min_lon`, `max_lon`, `vertices_json` y `vertex_count`.
- [x] Actualizar endpoint `POST /api/update-fence` en Node-RED (`flows.json`) para que al recibir nuevos vértices, guarde inmediatamente un punto en InfluxDB (`geofence_config`) y actualice variables de context, además de publicar a MQTT `geofence/config_update`.
- [x] Crear endpoint `GET /api/fence/current` (y `GET /api/node-red/api/fence/current` de compatibilidad) en Node-RED que consulte `SELECT last(*) FROM geofence_config` en InfluxDB y devuelva los vértices formateados para el frontend.
- [x] Configurar nodo `inject` (disparador al inicio `once: true`) para recargar automáticamente `fence_min_lat/max_lat/min_lon/max_lon` y `vertices_json` desde InfluxDB apenas arranca el contenedor Docker de Node-RED.
- [x] Ajustar `nginx.conf` en `frontend-app` para enrutar correctamente `/api/fence/` y `/api/update-fence` hacia Node-RED sin errores ni fallbacks 404/HTML.
- [x] Modificar o verificar `App.tsx` y `fenceService.ts` para que al cargar la página levanten los vértices persistidos desde `/api/fence/current` y que la UI se actualice exactamente al modificar el perímetro.
- **Status:** complete

## Decisiones Tomadas
| Decisión | Rationale |
|---|---|
| InfluxDB (`cattle_position`) para alertas de Telegram | Evita nodos extra escuchando a `simulated_gps`, separando la simulación de la lógica de negocio real. |
| Mute por `device_id` en `flow.set` y `global.set` | Permite controlar qué módulos envían alarmas al bot de Telegram. |
| Opción 1a: Measurement `geofence_config` con `vertices_json` y límites | Centraliza en una sola consulta (`SELECT last(*)`) todos los datos requeridos por Node-RED y por el Frontend. |
| Opción 2a: Enrutamiento directo `/api/fence/` en `nginx.conf` | Estandariza las rutas REST entre desarrollo y producción. |
| Opción 3a: Auto-recarga al iniciar Node-RED | Garantiza que las alarmas proactivas de Telegram funcionen instantáneamente al reiniciar Docker sin abrir el dashboard web. |

## Errores y Resoluciones
| Error | Intento | Resolución |
|---|---|---|
| `telegram sender` fallo por formato de mensaje | 1 | Ajustado `msg.payload` estructurado correctamente. |
