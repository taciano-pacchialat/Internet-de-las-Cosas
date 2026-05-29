Trabajo Práctico: Estación de Monitoreo
Ambiental IoT

1. Introducción
Este trabajo práctico tiene como objetivo implementar una estación de monitoreo ambiental
utilizando un microcontrolador ESP32. El sistema transmite datos de sensores a través del
protocolo MQTT, procesa y almacena la información utilizando Node-RED e InfluxDB, y
finalmente la visualiza en un dashboard en Grafana.

2. Arquitectura del Sistema
El flujo del sistema es el siguiente:
ESP32 + Sensores → MQTT (Mosquitto) → Node-RED → InfluxDB → Grafana

Cada componente cumple una función específica dentro del proceso de adquisición,
transmisión, almacenamiento y visualización de datos.

3. Materiales y Herramientas
Hardware:

- ESP32

●
●  Algún sensor o simularlo:

- Sensor DHT22 o BME280 (temperatura y humedad)
- Sensor MQ135 (calidad del aire)
- Sensor de luz (LDR o BH1750)

Software:

- Mosquitto (Broker MQTT)
- Node-RED
- InfluxDB
- Grafana
- IDE Arduino o PlatformIO

4. Desarrollo

4.1 Código ESP32 (ejemplo básico)
El siguiente código muestra cómo leer sensores y enviar los datos por MQTT:

// Librerías necesarias
#include <WiFi.h>
#include <PubSubClient.h>

// Conexión WiFi y Broker MQTT
const char* ssid = "TU_SSID";
const char* password = "TU_PASSWORD";
const char* mqtt_server = "192.168.X.X";

// Cliente MQTT
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  client.setServer(mqtt_server, 1883);
  client.connect("ESP32Client");
}

void loop() {
  float temp = 24.5; // Simulación
  char msg[50];
  sprintf(msg, "{\"temp\": %.2f}", temp);
  client.publish("sensor/ambiente", msg);
  delay(5000);
}

4.2 Configuración de Mosquitto
Instalar Mosquitto y permitir conexiones del ESP32 en el puerto 1883.

4.3 Node-RED
Crear un flujo que reciba datos desde MQTT, los convierta a JSON y los inserte en InfluxDB.

4.4 InfluxDB
Crear una base de datos llamada 'iot' y una medición 'ambiente'.

4.5 Grafana
Conectar Grafana a InfluxDB y diseñar un dashboard con gráficos de temperatura, humedad
y calidad del aire.

5. Entregables
Armar un informe donde documente el trabajo realizado pudiendo incluir:

●  Capturas de pantalla del dashboard de Grafana.
●  Código completo del ESP32.
●  Diagrama del flujo en Node-RED.
●  Comandos de inicialización/instalación.
●

 Dockerfiles, docker-compose.yml y cualquier archivo de configuración
necesarios si los hubiera.

Además se puede incluir una sección de conclusiones y mejoras donde refleje los
conocimientos adquiridos y los posibles puntos de mejora a futuro.


