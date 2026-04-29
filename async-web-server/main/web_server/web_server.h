#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <esp_err.h>

// Iniciar servidor y tareas
esp_err_t web_server_start(void);

// Cerrar clientes WebSocket activos
void web_server_close_websocket_clients(void);

// Publicar métricas (temperatura, humedad) a clientes WebSocket
void web_server_publish_metrics(float temperature, float humidity);

#endif // WEB_SERVER_H
