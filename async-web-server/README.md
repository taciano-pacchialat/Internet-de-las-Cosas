# Entrega 1 - IoT

Hola profes, acá les dejo una breve descripción de la entrega. El video demostrativo es [este](https://youtube.com/shorts/1e3vPbvofIU?feature=share).

La entrega fué desarrollada sobre ESP-IDF, con la extensión de vscode. Las dependencias se encuentran en el `dependencies.lock`. Intenté implementar todos los objetivos principales y secundarios. Las tareas de la entrega son las siguientes:

- **DHT11:** una vez iniciada la tarea, maneja las lecturas periódicas del sensor y envía los datos de temperatura y humedad al web server. Tiene menor prioridad que el web server. Su período es de 8 segundos. El manejo del sensor lo implementé con una librería que encontré [acá](https://components.espressif.com/components/esp-idf-lib/dht/versions/1.2.0/readme).

- **Tarea Principal:** maneja el flujo principal del programa. Llama al wifi manager, que una vez terminado inicializa el web server asincrónico.

## Servidor Web

Una breve descripción de la función de `web_server`.

1. Inicializa el puerto de GPIO del led.
2. Intenta obtener el web server desde el [wifi manager](https://components.espressif.com/components/tuanpmt/esp_wifi_manager/versions/1.1.0/readme).
3. Una vez configurado (ya sea que se utiliza una red ya registrada o una nueva) registra los siguientes handlers: 
    - main.html: el frontend de SPIFFS.
    - api: registra la API para obtener las métricas y prender/apagar el diodo.
    - ws: endpoint para agregar una sesión websocket.

Los handlers de obtención del html y el manejo del diodo se manejan de forma tradicional, mientras que la obtención de los datos de temperatura y humedad se hace por web socket.
