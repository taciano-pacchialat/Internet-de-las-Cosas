# Contexto y Estado del Proyecto: Sistema de Geofencing Ganadero con LoRa

Este documento preserva la memoria técnica, arquitectura modular, decisiones de diseño y estado actual de implementación del proyecto. Sirve como punto de partida y referencia para futuras sesiones de desarrollo y colaboración con agentes de inteligencia artificial.

---

## 1. Metadatos del Proyecto

- **Nombre del Sistema:** Sistema de Geofencing para Ganado con LoRa (IoT)
- **Ruta Raíz:** `/home/taci/unlp/iot/proyecto-final/`
- **Framework de Firmware:** ESP-IDF puro (v5.x / v6.0 verificado) en C++ (`.cpp`). No se utilizan wrappers como Arduino ni PlatformIO.
- **Librería de Comunicación RF:** `jgromes/radiolib` (v7.7.1) instalada nativamente mediante *IDF Component Manager*.
- **Transceptores LoRa:** Módulos SX1278 operando en la banda de 433 MHz.
- **Hardware de Campo (Edge):** Placa ESP32 clásica (instalada en el collar del ganado).
- **Hardware de Base (Gateway):** Placa ESP32-CAM (puente entre radio LoRa y red local Wi-Fi/MQTT).
- **Infraestructura Backend:** Stack Docker en servidor local o Raspberry Pi (Mosquitto, InfluxDB 1.8, Node-RED, Grafana).

---

## 2. Arquitectura del Código y Modularización

Ambos firmwares fueron modularizados siguiendo principios de alta cohesión y bajo acoplamiento para facilitar su mantenimiento y paso a producción:

### A. Módulo Edge (`edge-module/`)
Ubicado en `/home/taci/unlp/iot/proyecto-final/edge-module/main/`:
- `main.cpp`: Punto de entrada del sistema. Gestiona el ciclo de vida de bajo consumo (*Deep Sleep*) regido por el temporizador RTC (`WAKEUP_INTERVAL_SEC`).
- `rtc_state.cpp` / `.h`: Encapsula variables persistentes en memoria RTC (`RTC_DATA_ATTR`), como el contador de despertares (`s_wake_count`) y las últimas coordenadas conocidas, evitando pérdidas durante el sueño profundo.
- `lora_comms.cpp` / `.h`: Inicializa el SX1278 en el bus VSPI (`SPI3_HOST`), construye y transmite payloads JSON de latidos (*heartbeat*) y abre una ventana de escucha posterior de 2 segundos para recibir comandos de configuración (*downlinks*).
- `geofence_engine.cpp` / `.h`: Motor trigonométrico/espacial que calcula la distancia de la posición actual al centro del perímetro de seguridad para determinar el estado (`INSIDE` o `OUTSIDE`).
- `led_control.cpp` / `.h`: Control de actuadores físicos en el collar (LEDs de alerta lumínica y zumbador acústico) que se activan al detectar una violación del perímetro para disuadir el movimiento del animal.

### B. Módulo Gateway (`gateway-module/`)
Ubicado en `/home/taci/unlp/iot/proyecto-final/gateway-module/main/`:
- `gateway-module.cpp`: Función `app_main()`, inicialización global e invocación del bucle principal de procesamiento.
- `lora_service.cpp` / `.h`: Manejo del SX1278 en el bus HSPI (`SPI2_HOST`). Implementa el patrón de interrupción eficiente en tiempo real:
  - **ISR en memoria rápida:** Rutina `lora_dio0_isr_handler` declarada estrictamente con `IRAM_ATTR`. Al cambiar de flaco el pin DIO0, ejecuta únicamente un `vTaskNotifyGiveFromISR` y `portYIELD_FROM_ISR()`.
  - **Tarea FreeRTOS dedicada:** `lora_rx_processing_task` permanece bloqueada sin consumir ciclos CPU (`ulTaskNotifyTake(pdTRUE, portMAX_DELAY)`). Al recibir la notificación, lee el paquete por SPI, mide RSSI/SNR y gatilla el proceso de red.
- `wifi_service.cpp` / `.h`: Gestión de conectividad Wi-Fi bajo demanda. Incluye la **desregistración explícita** de manejadores de eventos (`esp_event_handler_instance_unregister`) antes de cada `esp_wifi_deinit()`, erradicando fugas de memoria en el heap durante arranques repetidos.
- `mqtt_service.cpp` / `.h`: Cliente MQTT que publica las tramas enriquecidas en `geofence/heartbeat` y consulta si hay mensajes retenidos en los tópicos de control (`geofence/simulated_gps`, `geofence/fence_update`) para responder al Edge vía downlink.

---

## 3. Infraestructura Backend y Flujos (`backend-services/`)

Ubicado en `/home/taci/unlp/iot/proyecto-final/backend-services/`:
- **`docker-compose.yml`**: Define contenedores interconectados con políticas de reinicio y volúmenes persistentes para datos y registros.
- **Mosquitto (Puerto 1883/9001):** Configurado con retención de mensajes y acceso local (`mosquitto.conf`).
- **InfluxDB v1.8 (Puerto 8086):** Base de datos temporal preconfigurada con la base `geofence_db` para almacenar la medición `cattle_position`.
- **Node-RED (Puerto 1880):** Contiene el archivo de flujos crítico `nodered/data/flows.json`:
  - **Flujo MQTT ➔ InfluxDB:** Recibe los payloads de `geofence/heartbeat`, ejecuta el nodo función `parse_heartbeat` (el cual **inyecta un sello de tiempo absoluto `timestamp_ms`** o `Date.now()` para asegurar la sincronización temporal exacta) y escribe en InfluxDB.
  - **Flujo Dashboard UI:** Provee un panel de control accesible en `http://localhost:1880/ui` que permite al operador inyectar coordenadas simuladas con bandera *retain*.
  - **Flujo Telegram Bot:** Plantilla lista para vincular un bot de Telegram que reporta la última coordenada al recibir el comando `/posicion`.
- **Grafana (Puerto 3000):** Aprovisionado automáticamente con la fuente de datos `InfluxDB-Geofence` apuntando a los contenedores locales.

---

## 4. Auditoría de Software y Soluciones de Producción (Code Smells Resueltos)

Duran la última auditoría arquitectónica, se detectaron y resolvieron 3 problemas críticos para preparar el sistema para un entorno de producción y demostración en vivo:
1. **Fuga de Memoria Wi-Fi (Smell #1):** Al encender y apagar el Wi-Fi de forma cíclica en el Gateway para ahorrar energía, los *event handlers* se acumulaban en la RAM. **Solución:** Se integró la limpieza simétrica con `esp_event_handler_instance_unregister` en `wifi_service.cpp`.
2. **Bloqueo Sincrónico en ISR LoRa (Smell #2):** El procesamiento completo del paquete de radio ocurría dentro de la interrupción de hardware, violando normas de tiempo real y corriendo riesgo de saturar el watchdog o causar pánico en el núcleo. **Solución:** Refactorización a tarea FreeRTOS dedicada (`lora_rx_processing_task`) notificada desde una ISR mínima en IRAM.
3. **Inconsistencia Temporal en Base de Datos (Smell #3):** InfluxDB dependía del tiempo de llegada de red o de relojes no sincronizados en el microcontrolador. **Solución:** Inyección de marca de tiempo canónica en el pipeline ETL de Node-RED.

---

## 5. Higiene del Repositorio y Git

- **Archivo `.gitignore`:** Se creó un archivo exhaustivo en la raíz que excluye directorios de compilación de ESP-IDF (`build/`, `sdkconfig.old`, `.ninja`), datos persistentes de contenedores Docker (`backend-services/*/data/`, `log/`) y archivos de IDE (`.vscode/`, `.idea/`).
- **Limpieza del Índice Git:** Se ejecutó un purgado de caché (`git rm -r --cached .`) que eliminó **4,451 archivos temporales y de compilación** que habían sido rastreados por error, manteniendo el repositorio limpio y liviano.
- **Documentación Técnica:** El archivo `README.md` fue redactado exhaustivamente utilizando técnicas para evitar prosa genérica o artificial (*avoid-ai-writing*), explicando en detalle la arquitectura, flujos de datos, tabla de tópicos MQTT y manual de usuario.

---

## 6. Procedimiento de Pruebas sin Hardware (Simulación Local)

Para verificar y probar la totalidad del backend sin conectar las placas ESP32 físicas, se establecieron los siguientes métodos de simulación:

### A. Simulación por Terminal (MQTT Pub via Docker)
Ejecutar en la terminal para enviar un heartbeat normal al bróker:
```bash
docker exec -it geofence-mosquitto mosquitto_pub -t "geofence/heartbeat" -m '{"device_id":"sim-cattle-01", "lat":-34.920500, "lon":-57.953600, "fence_status":"INSIDE", "wake_count":1, "rssi":-85, "snr":8.5}'
```
Simular una fuga del ganado cambiándole coordenadas y estado:
```bash
docker exec -it geofence-mosquitto mosquitto_pub -t "geofence/heartbeat" -m '{"device_id":"sim-cattle-01", "lat":-34.925000, "lon":-57.950000, "fence_status":"OUTSIDE", "wake_count":2, "rssi":-92, "snr":5.0}'
```

### B. Simulación desde la UI de Node-RED
1. Acceder al panel en `http://localhost:1880/ui`.
2. En la pestaña **Geofence Control**, ingresar latitud y longitud simulada y hacer clic en **📡 Publicar Coordenada Simulada**.
3. Para escuchar cómo el Gateway recibiría este comando retenido, suscribirse en terminal con:
```bash
docker exec -it geofence-mosquitto mosquitto_sub -t "geofence/#" -v
```

### C. Verificación en Base de Datos y Grafana
- **Consulta SQL en InfluxDB:**
```bash
docker exec -it geofence-influxdb influx -database 'geofence_db' -execute 'SELECT * FROM cattle_position'
```
- **Panel Grafana:** Ingresar a `http://localhost:3000` (admin/admin123) ➔ **Explore** ➔ seleccionar `InfluxDB-Geofence` y graficar la medición `cattle_position`.

---

## 7. Arquitectura del Frontend y Modos de Visualización Interactivos (`backend-services/frontend-app/`)

El frontend web (`React 18` + `Vite` + `Tailwind CSS` + `Leaflet`) fue diseñado y construido bajo una filosofía estrictamente sobria, técnica e industrial, eliminando cualquier estilo residual "retro/fancy":

### A. Filosofía de Diseño Industrial (Tailwind CSS)
- **Paleta Técnica:** Fondos grises oscuros (`bg-gray-900`, `bg-gray-800`), bordes divisorios técnicos (`border-gray-700`), tipografía limpia sin serifa (`Inter`, `monospace`) e indicadores lumínicos LED de estado (rojo intermitente para alerta, verde esmeralda para seguro).
- **Consola HUD:** Paneles modulares informativos con telemetría en tiempo real (Batería LiPo, RSSI, SNR y ciclos de despertado/Wake Count).

### B. Sistema de Modos de Vista (`Live` vs `Trail`)
El componente principal de navegación (`Header.tsx`) y el visor geográfico (`GeofenceMap.tsx`) operan en dos modos mutuamente excluyentes controlados por el estado `viewMode`:

1. **Modo "En Vivo" (Live View):**
   - **Perímetro Activo:** Muestra el polígono de la Geocerca actual persistida en base de datos junto a sus 4 vértices interactivos (`LivePolygonEditor`) para ajuste por teclado (`↑ ↓ ← →`) o arrastre.
   - **Posición Más Reciente:** Consulta InfluxDB (`useCattleTelemetry`) agrupando por `device_id` para dibujar **exclusivamente el punto más reciente** de cada animal en el campo.
   - **Caja Hardware (Bounding Box):** Permite visualizar el rectángulo envolvente (`min/max lat/lon`) evaluado por el firmware Edge en el collar.
   - **Sin Ruido Visual:** Suprime por completo líneas históricas o trayectorias pasadas.

2. **Modo "Rutas" (Trail / Breadcrumbs Mode):**
   - **Ocultamiento de Geocerca:** Oculta deliberadamente el polígono de la Geocerca para enfocar el análisis en las trayectorias de movimiento sin saturar el mapa.
   - **Selector de Entidades (`selectedDevice`):** Dropdown que permite aislar un dispositivo concreto (`vaca-01`, `vaca-02`, etc.) o seleccionar la vista global **"TODAS LAS VACAS (ALL)"** donde cada animal adquiere un color distintivo.
   - **Trazado Cronológico (`<Polyline>`):** Conecta las posiciones históricas ordenadas cronológicamente en orden ascendente.
   - **Vectores de Sentido Nativos:** En los últimos 2 a 3 segmentos de cada recorrido, se proyectan marcadores nativos de Leaflet (`L.divIcon` con SVG orientable) sin dependencias externas complejas. La orientación de las flechas se calcula matemáticamente mediante álgebra vectorial:
     $$\theta = \text{atan2}(\Delta \text{lon}, \Delta \text{lat}) \times \frac{180}{\pi}$$

---

## 8. Próximos Pasos Sugeridos para Futuras Sesiones

1. **Prueba en Hardware Real:** Conectar el ESP32-CAM (Gateway) y el ESP32 clásico (Edge), flashearlos usando `idf.py -p /dev/ttyUSBX flash monitor` y validar el enlace de radio de 433 MHz en campo.
2. **Calibración de la Geocerca:** Ajustar en `edge_config.h` el radio predeterminado (`GEOFENCE_DEFAULT_RADIUS_METERS`) y la posición del centro de la zona segura según el terreno real.
3. **Activación del Bot de Telegram:** Obtener un token en @BotFather y reemplazar `TU_TOKEN_AQUI` en el flujo deshabilitado de Node-RED para permitir consultas remotas desde celulares.
4. **Despliegue del Frontend:** Empaquetar la construcción estática verificada (`npm run build` en `backend-services/frontend-app/dist/`) en un contenedor Nginx o integrarla directamente al servidor Node-RED.
