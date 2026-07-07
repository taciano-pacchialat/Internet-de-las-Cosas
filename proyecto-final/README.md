# Sistema de Geofencing Ganadero con LoRa

Sistema de monitoreo de ganado y control de perímetros (geocerca) diseñado con microcontroladores ESP32, comunicación por radiofrecuencia LoRa (433 MHz) y una infraestructura local de servicios en contenedores Docker.

## Estructura del Proyecto

El repositorio se divide en tres módulos independientes:

- `edge-module/`: Firmware para el nodo de campo (ESP32 clásico). Se instala en el collar del animal.
- `gateway-module/`: Firmware para la estación receptora (ESP32-CAM). Actúa como puente entre la red de radio LoRa y la red local Wi-Fi/MQTT.
- `backend-services/`: Configuración de Docker Compose con los servicios de almacenamiento, procesamiento y visualización (Mosquitto, InfluxDB, Node-RED y Grafana).

---

## Arquitectura y Flujo de Datos

El sistema opera en un ciclo continuo de captura, transmisión, procesamiento y retroalimentación:

```
┌──────────────┐   LoRa 433 MHz    ┌──────────────┐   Wi-Fi / MQTT    ┌──────────────────┐
│  Nodo Edge   │ ◄───────────────► │   Gateway    │ ◄──────────────► │    Servidor      │
│   (ESP32)    │   Heartbeat / DL  │ (ESP32-CAM)  │   geofence/#    │     Backend      │
└──────────────┘                   └──────────────┘                   └──────────────────┘
       │                                   │                                    │
       ├─ Deep Sleep temporizado           ├─ ISR en IRAM (Pin DIO0)            ├─ Mosquitto (Broker)
       ├─ Evaluación de geocerca           ├─ Tarea FreeRTOS (cero CPU)         ├─ Node-RED (ETL / UI)
       ├─ Alertas locales (LED/Buzzer)     ├─ Conexión Wi-Fi bajo demanda       ├─ InfluxDB (Series temp.)
       └─ Lectura de downlink              └─ Publicación y lectura MQTT        └─ Grafana (Dashboard)
```

### 1. Captura en el Campo (Nodo Edge)
1. **Suspensión y Despertado:** El microcontrolador pasa la mayor parte del tiempo en modo de bajo consumo (*Deep Sleep*). Un temporizador de hardware (RTC) lo despierta cada intervalo configurado (por defecto, 30 segundos).
2. **Evaluación de la Geocerca:** Al despertar, el sistema lee la última posición GPS conocida (o simulada) y calcula la distancia respecto al centro y radio de la zona segura.
3. **Alerta Local:** Si el animal supera el límite del perímetro, el nodo activa de inmediato indicadores visuales y sonoros en el collar (LEDs y zumbador) para disuadir el avance.
4. **Transmisión de Heartbeat:** El transceptor LoRa SX1278 (controlado vía bus VSPI con la librería RadioLib) emite un paquete JSON con el identificador del dispositivo, coordenadas actuales, contador de ciclos y estado de la geocerca.
5. **Ventana de Recepción:** Tras transmitir, el nodo escucha el canal de radio durante dos segundos en busca de comandos de configuración enviados por el servidor. Finalizada la ventana, apaga la radio y vuelve a dormir.

### 2. Recepción y Enrutamiento (Gateway)
1. **Escucha Continua:** El módulo Gateway se mantiene alimentado y con el módulo LoRa en modo de recepción continua (bus HSPI).
2. **Manejo por Interrupción:** Cuando la antena recibe un paquete del Edge, el pin DIO0 genera una interrupción de hardware (GPIO2). La rutina de interrupción (ejecutada directamente en memoria rápida IRAM) despierta una tarea de FreeRTOS dedicada, manteniendo el consumo de CPU al mínimo mientras espera.
3. **Puente a Red Local:** La tarea de FreeRTOS lee el payload, calcula las métricas físicas del enlace de radio (RSSI y SNR) e inicia la conexión Wi-Fi local.
4. **Publicación MQTT:** Una vez conectado a la red, el Gateway publica los datos en el bróker MQTT local bajo el tópico `geofence/heartbeat`.
5. **Envío de Comandos (Downlink):** Antes de desconectarse, el Gateway consulta si existen mensajes retenidos en los tópicos de control (`geofence/simulated_gps` o `geofence/fence_update`). Si encuentra nuevas coordenadas o cambios en el perímetro, emite un paquete LoRa de respuesta hacia el nodo Edge.

### 3. Procesamiento y Visualización (Backend)
1. **Recepción en Broker:** El servidor Mosquitto recibe la trama JSON enviada por el Gateway.
2. **Transformación (Node-RED):** Un flujo de Node-RED suscrito al tópico capta el mensaje, valida su estructura, asigna una marca de tiempo absoluta (`timestamp`) y formatea los datos para su inserción en base de datos.
3. **Almacenamiento (InfluxDB):** Los datos procesados se escriben en InfluxDB v1.8 bajo la medición `cattle_position`, catalogando por dispositivo y estado del perímetro.
4. **Monitoreo (Grafana):** Los paneles de Grafana consultan InfluxDB para graficar en tiempo real la ubicación en mapa, evolución del RSSI/SNR, nivel de actividad y registro de alarmas.
5. **Control Interactivo:** Desde la interfaz web de Node-RED (o un bot de Telegram opcional), el operador puede publicar nuevas coordenadas simuladas o modificar el radio de la geocerca. Estos comandos quedan retenidos en MQTT hasta que el Gateway los recoge en el siguiente ciclo de comunicación.

---

## Guía de Utilización

### Prerrequisitos
- **Software:** Docker, Docker Compose, Git y el toolchain de ESP-IDF (v5.0 o superior).
- **Hardware:**
  - 1x Placa ESP32 clásica con módulo LoRa SX1278 (Nodo Edge).
  - 1x Placa ESP32-CAM con módulo LoRa SX1278 (Gateway).
  - 1x PC, servidor local o Raspberry Pi conectada a la red local (Servidor Backend).

### Paso 1: Levantar los Servicios Backend
En la terminal de la computadora que actuará como servidor local:

```bash
cd backend-services
docker-compose up -d
```

Verifica que los servicios estén activos comprobando sus puertos nativos:
- **Node-RED (Dashboard y lógica):** `http://localhost:1880`
- **Grafana (Visualización y métricas):** `http://localhost:3000` (Usuario: `admin`, Clave: `admin123`)
- **InfluxDB (Base de datos temporal):** Puerto `8086`
- **Mosquitto (Broker MQTT):** Puerto `1883`

### Paso 2: Configurar y Compilar el Gateway
El Gateway necesita acceso a la red Wi-Fi local y la dirección IP del servidor donde corre Mosquitto.

1. Edita el archivo de configuración `gateway-module/main/gateway_config.h` y ajusta los parámetros de red:
   ```c
   #define WIFI_SSID "Tu_Red_WiFi"
   #define WIFI_PASS "Tu_Clave_WiFi"
   #define MQTT_BROKER_URI "mqtt://IP_DEL_SERVIDOR:1883"
   ```
2. Conecta la placa ESP32-CAM por USB, inicializa el entorno de ESP-IDF y compila el firmware:
   ```bash
   cd gateway-module
   idf.py set-target esp32
   idf.py build
   idf.py -p /dev/ttyUSB0 flash monitor
   ```
   *Nota: Reemplaza `/dev/ttyUSB0` por el puerto COM correspondiente a tu sistema.*

### Paso 3: Configurar y Compilar el Nodo Edge
El nodo Edge opera de forma autónoma comunicándose por radio LoRa.

1. Si necesitas ajustar el intervalo de sueño o el radio predeterminado del perímetro, edita `edge-module/main/edge_config.h`:
   ```c
   #define WAKEUP_INTERVAL_SEC 30
   #define GEOFENCE_DEFAULT_RADIUS_METERS 100.0
   ```
2. Conecta la placa ESP32 del nodo por USB y graba el firmware:
   ```bash
   cd edge-module
   idf.py set-target esp32
   idf.py build
   idf.py -p /dev/ttyUSB1 flash monitor
   ```

### Paso 4: Pruebas y Operación del Sistema

1. **Verificación de Enlace:** Al reiniciar el nodo Edge, verás en la consola del Gateway la recepción de paquetes por interrupción DIO0, la conexión a Wi-Fi y la publicación en MQTT.
2. **Simulación de Movimiento:**
   - Abre la interfaz de usuario de Node-RED en `http://localhost:1880/ui`.
   - En el panel de control, introduce coordenadas de latitud y longitud simuladas para mover virtualmente al animal fuera del perímetro de seguridad.
   - Pulsa el botón para enviar la coordenada. En el siguiente ciclo de comunicación, el Gateway transmitirá esta coordenada al Edge mediante un paquete downlink.
3. **Activación de Alarma:** Al recibir la coordenada simulada, el nodo Edge recalculará su distancia en su siguiente despertar. Al detectar una violación de límite, encenderá sus LEDs de alerta y emitirá una señal acústica en el campo.
4. **Análisis Histórico:** Ingresa a Grafana en `http://localhost:3000`, selecciona el panel de Geofencing y revisa el registro histórico de recorridos, intensidad de señal de radio y conteo de ciclos de sueño del dispositivo.

---

## Estructura de Tópicos MQTT

| Tópico | Dirección | Retenido | Descripción |
| :--- | :--- | :--- | :--- |
| `geofence/heartbeat` | Gateway → Broker | No | Paquete JSON con posición, batería, RSSI/SNR y estado del perímetro. |
| `geofence/simulated_gps` | Dashboard → Broker | Sí | Comando con coordenadas `{"lat": -34.92, "lon": -57.95}` para simular posición en campo. |
| `geofence/fence_update` | Dashboard → Broker | Sí | Comando para actualizar el centro y radio del perímetro de seguridad. |
