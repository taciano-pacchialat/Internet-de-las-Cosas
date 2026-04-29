#include "web_server.h"
#include "dht11.h"
#include <esp_log.h>
#include <esp_http_server.h>
#include <esp_wifi_manager.h>
#include <cJSON.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#define LED_GPIO GPIO_NUM_2
#define WS_CLIENTS_MAX 5

static const char *TAG = "WEB_SERVER";

// Variables globales
static httpd_handle_t server = NULL;
static bool led_state = false;
static float last_temperature = 0.0f;
static float last_humidity = 0.0f;
static time_t last_update_time = 0;
static int ws_clients[WS_CLIENTS_MAX];
static size_t ws_client_count = 0;

static void ws_remove_client_fd(int fd);

static void ws_build_snapshot(char **response_out) {
    dht11_reading_t history_buf[30];
    int history_count = 0;
    dht11_get_history(history_buf, &history_count, 30);

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        *response_out = NULL;
        return;
    }

    cJSON_AddNumberToObject(root, "temperature", last_temperature);
    cJSON_AddNumberToObject(root, "humidity", last_humidity);
    cJSON_AddBoolToObject(root, "led_state", led_state);

    cJSON *history = cJSON_CreateArray();
    for (int i = 0; i < history_count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "temperature", history_buf[i].temperature);
        cJSON_AddNumberToObject(item, "humidity", history_buf[i].humidity);
        cJSON_AddItemToArray(history, item);
    }
    cJSON_AddItemToObject(root, "history", history);

    *response_out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
}

static void ws_add_client_fd(int fd) {
    for (size_t i = 0; i < ws_client_count; i++) {
        if (ws_clients[i] == fd) {
            return;
        }
    }

    if (ws_client_count >= WS_CLIENTS_MAX) {
        ESP_LOGW(TAG, "WS client limit reached, closing fd=%d", fd);
        httpd_sess_trigger_close(server, fd);
        return;
    }

    ws_clients[ws_client_count++] = fd;
    ESP_LOGI(TAG, "WS client added fd=%d total=%u", fd, (unsigned)ws_client_count);
}

static void ws_remove_client_fd(int fd) {
    for (size_t i = 0; i < ws_client_count; i++) {
        if (ws_clients[i] == fd) {
            for (size_t j = i + 1; j < ws_client_count; j++) {
                ws_clients[j - 1] = ws_clients[j];
            }
            ws_client_count--;
            ESP_LOGI(TAG, "WS client removed fd=%d total=%u", fd, (unsigned)ws_client_count);
            return;
        }
    }
}

void web_server_close_websocket_clients(void) {
    if (!server) {
        ws_client_count = 0;
        return;
    }

    for (size_t i = 0; i < ws_client_count; i++) {
        httpd_sess_trigger_close(server, ws_clients[i]);
    }

    ws_client_count = 0;
}

static esp_err_t ws_send_snapshot_to_fd(int fd) {
    char *response = NULL;
    ws_build_snapshot(&response);
    if (!response) {
        return ESP_ERR_NO_MEM;
    }

    httpd_ws_frame_t frame = {
        .final = true,
        .fragmented = false,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)response,
        .len = strlen(response),
    };

    esp_err_t ret = httpd_ws_send_frame_async(server, fd, &frame);
    free(response);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WS send failed fd=%d err=%d", fd, ret);
        ws_remove_client_fd(fd);
    }

    return ret;
}

static void ws_broadcast_work(void *arg) {
    (void)arg;

    char *response = NULL;
    ws_build_snapshot(&response);
    if (!response) {
        return;
    }

    httpd_ws_frame_t frame = {
        .final = true,
        .fragmented = false,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)response,
        .len = strlen(response),
    };

    for (size_t i = 0; i < ws_client_count; ) {
        int fd = ws_clients[i];
        if (httpd_ws_get_fd_info(server, fd) == HTTPD_WS_CLIENT_WEBSOCKET) {
            if (httpd_ws_send_frame_async(server, fd, &frame) != ESP_OK) {
                ws_remove_client_fd(fd);
                continue;
            }
        } else {
            ws_remove_client_fd(fd);
            continue;
        }

        i++;
    }

    free(response);
}

static esp_err_t ws_post_handshake_cb(httpd_req_t *req) {
    int fd = httpd_req_to_sockfd(req);
    ws_add_client_fd(fd);
    return ws_send_snapshot_to_fd(fd);
}

// Manejador: GET /
static esp_err_t handler_get_index(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET /main.html");
    
    // Servir main.html desde SPIFFS
    FILE *f = fopen("/spiffs/main.html", "r");
    if (!f) {
        ESP_LOGE(TAG, "No se puede abrir main.html");
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    
    char buffer[512];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        httpd_resp_send_chunk(req, buffer, read_bytes);
    }
    httpd_resp_send_chunk(req, NULL, 0);
    
    fclose(f);
    return ESP_OK;
}

// Manejador: GET /api/metrics
static esp_err_t handler_get_metrics(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET /api/metrics");
    
    // Obtener histórico de DHT11
    dht11_reading_t history_buf[30];
    int history_count = 0;
    dht11_get_history(history_buf, &history_count, 30);
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temperature", last_temperature);
    cJSON_AddNumberToObject(root, "humidity", last_humidity);
    cJSON_AddBoolToObject(root, "led_state", led_state);
    
    // Array de histórico
    cJSON *history = cJSON_CreateArray();
    for (int i = 0; i < history_count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "temperature", history_buf[i].temperature);
        cJSON_AddNumberToObject(item, "humidity", history_buf[i].humidity);
        cJSON_AddItemToArray(history, item);
    }
    cJSON_AddItemToObject(root, "history", history);
    
    char *response = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    cJSON_Delete(root);
    free(response);
    
    return ESP_OK;
}

// Manejador: POST /api/led
static esp_err_t handler_post_led(httpd_req_t *req) {
    ESP_LOGI(TAG, "POST /api/led");
    
    char buf[100] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty");
        return ESP_FAIL;
    }
    
    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    // Alternar LED
    led_state = !led_state;
    gpio_set_level(LED_GPIO, led_state ? 1 : 0);
    ESP_LOGI(TAG, "LED: %s", led_state ? "ON" : "OFF");
    
    // Respuesta
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "state", led_state);
    cJSON_AddNumberToObject(resp, "timestamp", time(NULL));
    
    char *response = cJSON_PrintUnformatted(resp);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    cJSON_Delete(json);
    cJSON_Delete(resp);
    free(response);
    
    return ESP_OK;
}

// Manejador: WebSocket /ws
static esp_err_t handler_ws(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(ws_pkt));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        return ret;
    }

    if (ws_pkt.len > 0) {
        uint8_t *payload = calloc(1, ws_pkt.len + 1);
        if (!payload) {
            return ESP_ERR_NO_MEM;
        }

        ws_pkt.payload = payload;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        free(payload);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

// Registrar manejadores
static esp_err_t register_handlers(httpd_handle_t httpd) {
    // GET /main.html
    httpd_uri_t uri_index = {
        .uri = "/main.html",
        .method = HTTP_GET,
        .handler = handler_get_index,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(httpd, &uri_index);

    // GET /index.html (alias)
    httpd_uri_t uri_index_alias = {
        .uri = "/index.html",
        .method = HTTP_GET,
        .handler = handler_get_index,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(httpd, &uri_index_alias);
    
    // GET /api/metrics
    httpd_uri_t uri_metrics = {
        .uri = "/api/metrics",
        .method = HTTP_GET,
        .handler = handler_get_metrics,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(httpd, &uri_metrics);
    
    // POST /api/led
    httpd_uri_t uri_led = {
        .uri = "/api/led",
        .method = HTTP_POST,
        .handler = handler_post_led,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(httpd, &uri_led);
    
    // WebSocket
    httpd_uri_t uri_ws = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = handler_ws,
        .user_ctx = NULL,
        .is_websocket = true,
#if CONFIG_HTTPD_WS_POST_HANDSHAKE_CB_SUPPORT
        .ws_post_handshake_cb = ws_post_handshake_cb,
#endif
    };
    httpd_register_uri_handler(httpd, &uri_ws);
    
    ESP_LOGI(TAG, "Manejadores registrados");
    return ESP_OK;
}

// Inicializar GPIO LED
static void led_gpio_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    gpio_config(&io_conf);
    gpio_set_level(LED_GPIO, 0); // Apagado
    ESP_LOGI(TAG, "GPIO LED configurado en %d", LED_GPIO);
}

// Iniciar servidor
esp_err_t web_server_start(void) {
    ESP_LOGI(TAG, "Iniciando servidor web");
    
    // Inicializar LED
    led_gpio_init();
    
    // Obtener servidor de WiFi Manager (esperar a que esté disponible)
    int attempts = 0;
    while ((server = wifi_manager_get_httpd()) == NULL && attempts < 50) {
        ESP_LOGD(TAG, "Esperando servidor HTTP de wifi_manager...");
        vTaskDelay(pdMS_TO_TICKS(100));
        attempts++;
    }
    
    if (!server) {
        ESP_LOGE(TAG, "Servidor HTTP no disponible después de 5 segundos");
        return ESP_FAIL;
    }
    
    // Registrar manejadores
    register_handlers(server);
    
    ESP_LOGI(TAG, "Servidor iniciado correctamente");
    
    return ESP_OK;
}

// Publicar métricas a WebSocket/polling
void web_server_publish_metrics(float temperature, float humidity) {
    last_temperature = temperature;
    last_humidity = humidity;
    last_update_time = time(NULL);
    
    if (server) {
        httpd_queue_work(server, ws_broadcast_work, NULL);
    }
}
