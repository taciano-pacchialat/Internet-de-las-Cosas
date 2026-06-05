Pautas Trabajo Integrador Final - IOT 2026

Preferentemente deberá trabajarse en grupos de 2 personas.
Fecha de entrega y presentación :  Miércoles 8/7 y Miércoles 15/7.
Además de la entrega, cada grupo deberá exponer una  breve presentación de 10 minutos
con la idea del proyecto llevada a cabo.

Requisitos generales:

●  Todos los proyectos deben documentar el diseño, justificación técnica, y resultados.
●  En caso de no contar con sensores específicos, se permite la simulación mediante

entradas analógicas, pulsadores o funciones en el código.

●  ENTREGABLES:

○  Presentación en clase, donde se mencionan las particularidades del

proyecto, alcance y solución.

○  Códigos utilizados.
○  Otra documentación complementaria del proyecto ( links, antecedentes,

referencias a proyectos similares, herramientas utilizadas, etc.)

Guía tentativa de temas a implementar

●  TinyML con ESP32 / CAM con EdgeImpulse/TensorFlow Lite/Eloquent Arduino

○  Traducción de contadores mecánicos a digitales
○  Detección de anomalías en motores por vibración
○  Contador de personas
○  Clasificador/Contador de tránsito vehicular
○  Control de calidad en líneas de producción
○  Detector de caídas o accidentes laborales con acelerómetro
○  Clasificación de residuos con visión
○  Otra propuesta consensuada con la cátedra

●  Servidor WEB embebido + Tableros en Cloud / Bot Interactivo Telegram

○  Sensado temperatura/alertas en invernaderos/datacenters/cadenas de frio de

medicamentos

○  Debe poder transmitir telemetría  y recibir comandos/configuraciones por

MQTT

●  Tracking - Geofencing

○  GPS + esp32. Ejemplo: Desarrollar un sistema de localización que envíe
coordenadas GPS periódicamente. Si el dispositivo sale de una zona
predefinida (geofencing), debe emitir una alerta (visual, sonora o por
mensaje). Mapeo en Grafana de las posiciones recorridas.

○  Beacons esp32. Ejemplo: El ESP32 escanea balizas BLE colocadas en

diferentes obras o zonas. Según el beacon detectado, reproduce un audio o
envía información textual por Telegram o una app asociada. Otro ejemplo :
simular un carrito de carga lleva un beacon BLE. El ESP32 los detecta al

pasar por zonas estratégicas (como entradas o bifurcaciones) y actualiza el
estado logístico en una base de datos o lo visualiza en Node-RED.
(Aplicación: Museos, ferias tecnológicas, espacios de divulgación. Logística
interna, trazabilidad industrial.)

○  Opcional para entusiastas: Pueden desarrollar una App que forme parte de la

solución o la complemente.

●  Sistema de Control de Aforo para Aula o Espacio Cerrado

○  El dispositivo cuenta personas que entran y salen de un ambiente (simulado

con pulsadores o sensores de movimiento), y calcula el porcentaje de
ocupación respecto a un aforo máximo. Muestra alertas cuando se supera el
límite. Registros historicos .

●  Sistema de Notificación para contenedores de Basura

○  Detecta si un cesto (simulado) está lleno usando un sensor de distancia o

peso, y envía un aviso para vaciado. Ideal para canastos comunitarios. Mapa
de contenedores.

●  Sistema inteligente de riego automático basado en clima y humedad

○  Automatizar el riego considerando humedad del suelo y datos climáticos

online. Tecnologías: ESP32, Sensor de humedad, API climática, MQTT, Relé
para bomba o válvula. Funcionalidades:

●  Riego automático
●  Suspensión si se pronostica lluvia
●  Configuración y comando remoto
●  Dashboard histórico


