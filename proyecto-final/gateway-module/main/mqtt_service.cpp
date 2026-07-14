#include "mqtt_service.h"
#include <cstring>
#include <cstdio>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "gateway_config.h"
#include "mdns.h"
#include "esp_netif.h"
#include "lwip/sockets.h"

static const char* TAG = "MQTT_SVC";

static esp_mqtt_client_handle_t mqtt_client = nullptr;
static volatile bool mqtt_connected = false;
static char s_dynamic_mqtt_uri[128] = {0};

// Buffers para datos recibidos de MQTT (downlink para el edge)
static char downlink_gps_buffer[512] = {0};
static char downlink_fence_buffer[512] = {0};
static volatile bool has_new_gps = false;
static volatile bool has_new_fence = false;

/*
 * =============================================================================
 * RESOLUCIÓN mDNS DINÁMICA DEL BROKER MQTT (Alternativa de Respaldo)
 * =============================================================================
 * Para evitar hardcodeo de IPs en entornos de red cambiantes, consultamos
 * mediante mdns_query_a() al servicio local por el hostname MDNS_TARGET_HOST
 * ("iotbroker"). Si se obtiene un registro A dentro de los 2000 ms, armamos
 * dinámicamente la URI mqtt://<IP_RESUELTA>:1883.
 * Si el lookup expira o falla, recurrimos como fallback a MQTT_BROKER_URI
 * ("mqtt://iotbroker.local:1883").
 * =============================================================================
 */
/**
 * Escucha beacons UDP del servicio de descubrimiento del backend.
 * El beacon tiene el formato: "GEOFENCE_MQTT:<IP>:<PUERTO>"
 * También envía una solicitud activa "GEOFENCE_DISCOVER" por si el beacon
 * aún no fue emitido cuando el gateway arrancó.
 */
static bool discover_broker_via_udp(char* out_uri, size_t uri_len) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGW(TAG, "UDP discovery: no se pudo crear socket");
        return false;
    }

    // Permitir recibir broadcasts
    int broadcast_en = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_en, sizeof(broadcast_en));
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &broadcast_en, sizeof(broadcast_en));

    // Bind al puerto del beacon
    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(DISCOVERY_BEACON_PORT);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
        ESP_LOGW(TAG, "UDP discovery: fallo en bind al puerto %d", DISCOVERY_BEACON_PORT);
        close(sock);
        return false;
    }

    // Enviar solicitud activa de descubrimiento
    struct sockaddr_in bcast_addr;
    memset(&bcast_addr, 0, sizeof(bcast_addr));
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port = htons(DISCOVERY_BEACON_PORT);
    bcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    const char* discover_msg = "GEOFENCE_DISCOVER";
    sendto(sock, discover_msg, strlen(discover_msg), 0,
           (struct sockaddr*)&bcast_addr, sizeof(bcast_addr));

    ESP_LOGI(TAG, "UDP discovery: escuchando beacon en puerto %d (max %d ms)...",
             DISCOVERY_BEACON_PORT, DISCOVERY_LISTEN_TIMEOUT_MS);

    // Configurar timeout de recepción
    struct timeval tv;
    tv.tv_sec = DISCOVERY_LISTEN_TIMEOUT_MS / 1000;
    tv.tv_usec = (DISCOVERY_LISTEN_TIMEOUT_MS % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char rx_buf[128] = {0};
    struct sockaddr_in src_addr;
    socklen_t src_len = sizeof(src_addr);

    bool found = false;
    // Escuchar hasta timeout, ignorando nuestros propios paquetes
    while (!found) {
        int len = recvfrom(sock, rx_buf, sizeof(rx_buf) - 1, 0,
                           (struct sockaddr*)&src_addr, &src_len);
        if (len <= 0) break;  // Timeout o error

        rx_buf[len] = '\0';

        // Verificar magic header: "GEOFENCE_MQTT:<IP>:<PORT>"
        if (strncmp(rx_buf, DISCOVERY_BEACON_MAGIC, strlen(DISCOVERY_BEACON_MAGIC)) == 0) {
            char broker_ip[20] = {0};
            int broker_port = 1883;
            if (sscanf(rx_buf, "GEOFENCE_MQTT:%19[^:]:%d", broker_ip, &broker_port) >= 1) {
                snprintf(out_uri, uri_len, "mqtt://%s:%d", broker_ip, broker_port);
                ESP_LOGI(TAG, "UDP discovery: broker MQTT encontrado → %s", out_uri);
                found = true;
            }
        }
    }

    close(sock);
    return found;
}

/**
 * Escanea la subred local del gateway buscando un broker MQTT (puerto 1883).
 * Intenta conexiones TCP rápidas con timeout corto a un rango de IPs.
 * Retorna true si encuentra un broker, escribiendo la URI en out_uri.
 */
static bool scan_subnet_for_mqtt(char* out_uri, size_t uri_len) {
    // Obtener la IP del gateway desde la interfaz STA
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        ESP_LOGW(TAG, "Subnet scan: no se pudo obtener interfaz STA");
        return false;
    }

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        ESP_LOGW(TAG, "Subnet scan: no se pudo obtener IP info");
        return false;
    }

    // Extraer los 3 primeros octetos de la IP del gateway
    uint32_t my_ip = ntohl(ip_info.ip.addr);
    uint8_t oct1 = (my_ip >> 24) & 0xFF;
    uint8_t oct2 = (my_ip >> 16) & 0xFF;
    uint8_t oct3 = (my_ip >> 8) & 0xFF;
    uint8_t my_oct4 = my_ip & 0xFF;

    ESP_LOGI(TAG, "Subnet scan: escaneando %d.%d.%d.1-%d buscando MQTT en puerto 1883...",
             oct1, oct2, oct3, MQTT_SUBNET_SCAN_MAX);

    for (int host = 1; host <= MQTT_SUBNET_SCAN_MAX; host++) {
        if (host == (int)my_oct4) continue;  // Saltar nuestra propia IP

        // Construir IP candidata
        char candidate_ip[20];
        snprintf(candidate_ip, sizeof(candidate_ip), "%d.%d.%d.%d", oct1, oct2, oct3, host);

        // Intentar conexión TCP rápida al puerto 1883
        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock < 0) continue;

        // Configurar socket como non-blocking para timeout rápido
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        struct sockaddr_in dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(1883);
        inet_aton(candidate_ip, &dest_addr.sin_addr);

        connect(sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));

        // Esperar con select() usando timeout corto
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(sock, &write_fds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = MQTT_SUBNET_SCAN_TIMEOUT_MS * 1000;  // Convertir ms a us

        int result = select(sock + 1, NULL, &write_fds, NULL, &tv);
        close(sock);

        if (result > 0) {
            // Verificar que la conexión fue exitosa (no solo que select() retornó)
            // Si select retornó writable, hay un host escuchando en ese puerto
            snprintf(out_uri, uri_len, "mqtt://%s:1883", candidate_ip);
            ESP_LOGI(TAG, "Subnet scan: broker MQTT encontrado en %s", out_uri);
            return true;
        }
    }

    ESP_LOGW(TAG, "Subnet scan: no se encontró broker MQTT en la subred %d.%d.%d.x",
             oct1, oct2, oct3);
    return false;
}

static const char* resolve_mqtt_uri(void) {
    // 1. [PRIORIDAD] Auto-descubrimiento por beacon UDP (funciona en hotspot y router)
    if (discover_broker_via_udp(s_dynamic_mqtt_uri, sizeof(s_dynamic_mqtt_uri))) {
        return s_dynamic_mqtt_uri;
    }

    // 2. Pequeño retardo (600 ms) tras IP_EVENT_STA_GOT_IP para estabilizar IGMP/multicast en el router
    vTaskDelay(pdMS_TO_TICKS(600));

    // 2. Intentar auto-descubrimiento de servicios mDNS PTR (_mqtt._tcp.local)
    mdns_result_t* results = NULL;
    esp_err_t err = mdns_query_ptr("_mqtt", "_tcp", 1500, 5, &results);
    if (err == ESP_OK && results != NULL) {
        mdns_result_t* r = results;
        while (r) {
            mdns_ip_addr_t* a = r->addr;
            while (a) {
                if (a->addr.type == ESP_IPADDR_TYPE_V4) {
                    snprintf(s_dynamic_mqtt_uri, sizeof(s_dynamic_mqtt_uri),
                             "mqtt://" IPSTR ":%u", IP2STR(&(a->addr.u_addr.ip4)), r->port ? r->port : 1883);
                    ESP_LOGI(TAG, "Broker MQTT auto-descubierto en red local vía servicio mDNS (_mqtt._tcp): %s", s_dynamic_mqtt_uri);
                    mdns_query_results_free(results);
                    return s_dynamic_mqtt_uri;
                }
                a = a->next;
            }
            r = r->next;
        }
        mdns_query_results_free(results);
    }

    // 3. Consultar A record para MDNS_TARGET_HOST con reintento (ej. archlinux.local)
    esp_ip4_addr_t addr;
    for (int retry = 0; retry < 2; retry++) {
        err = mdns_query_a(MDNS_TARGET_HOST, 1200, &addr);
        if (err == ESP_OK) {
            snprintf(s_dynamic_mqtt_uri, sizeof(s_dynamic_mqtt_uri),
                     "mqtt://" IPSTR ":1883", IP2STR(&addr));
            ESP_LOGI(TAG, "Host mDNS '%s.local' resuelto exitosamente -> %s", MDNS_TARGET_HOST, s_dynamic_mqtt_uri);
            return s_dynamic_mqtt_uri;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    // 4. Intentar hostnames alternativos frecuentes en redes locales
    const char* fallback_hosts[] = {"iotbroker", "mosquitto"};
    for (int i = 0; i < 2; i++) {
        err = mdns_query_a(fallback_hosts[i], 800, &addr);
        if (err == ESP_OK) {
            snprintf(s_dynamic_mqtt_uri, sizeof(s_dynamic_mqtt_uri),
                     "mqtt://" IPSTR ":1883", IP2STR(&addr));
            ESP_LOGI(TAG, "Host mDNS '%s.local' resuelto -> %s", fallback_hosts[i], s_dynamic_mqtt_uri);
            return s_dynamic_mqtt_uri;
        }
    }

    // 5. [NUEVO] Escaneo de subred local: intento de auto-descubrimiento por TCP connect
    //    Útil en redes hotspot donde mDNS no funciona (sin multicast).
    if (scan_subnet_for_mqtt(s_dynamic_mqtt_uri, sizeof(s_dynamic_mqtt_uri))) {
        return s_dynamic_mqtt_uri;
    }

    // 6. Si no hay publicidad mDNS en la red NI se encontró broker por escaneo,
    //    usar limpiamente la URI por defecto configurada
    ESP_LOGI(TAG, "Usando broker MQTT configurado por defecto para la red: %s", MQTT_BROKER_URI);
    return MQTT_BROKER_URI;
}

static void mqtt_event_handler(void* arg, esp_event_base_t base,
                               int32_t event_id, void* event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT conectado al broker exitosamente");
            mqtt_connected = true;
            esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_SIM_GPS, 1);
            esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_FENCE_UPDATE, 1);
            esp_mqtt_client_subscribe(mqtt_client, "geofence/config_update", 1);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT datos recibidos en topico: %.*s", event->topic_len, event->topic);

            if (strncmp(event->topic, MQTT_TOPIC_SIM_GPS, event->topic_len) == 0) {
                int copy_len = event->data_len < (int)sizeof(downlink_gps_buffer) - 1
                             ? event->data_len : (int)sizeof(downlink_gps_buffer) - 1;
                memcpy(downlink_gps_buffer, event->data, copy_len);
                downlink_gps_buffer[copy_len] = '\0';
                has_new_gps = true;
                ESP_LOGI(TAG, "GPS simulado recibido: %s", downlink_gps_buffer);
            }
            else if (strncmp(event->topic, MQTT_TOPIC_FENCE_UPDATE, event->topic_len) == 0 ||
                     strncmp(event->topic, "geofence/config_update", event->topic_len) == 0) {
                char temp_buf[1024] = {0};
                int copy_len = event->data_len < (int)sizeof(temp_buf) - 1
                             ? event->data_len : (int)sizeof(temp_buf) - 1;
                memcpy(temp_buf, event->data, copy_len);
                temp_buf[copy_len] = '\0';

                cJSON* root = cJSON_Parse(temp_buf);
                if (root != NULL) {
                    cJSON* min_lat_j = cJSON_GetObjectItem(root, "min_lat");
                    cJSON* max_lat_j = cJSON_GetObjectItem(root, "max_lat");
                    cJSON* min_lon_j = cJSON_GetObjectItem(root, "min_lon");
                    cJSON* max_lon_j = cJSON_GetObjectItem(root, "max_lon");

                    if (cJSON_IsNumber(min_lat_j) && cJSON_IsNumber(max_lat_j) &&
                        cJSON_IsNumber(min_lon_j) && cJSON_IsNumber(max_lon_j)) {
                        snprintf(downlink_fence_buffer, sizeof(downlink_fence_buffer),
                                 "{\"min_lat\":%.6f,\"max_lat\":%.6f,\"min_lon\":%.6f,\"max_lon\":%.6f}",
                                 min_lat_j->valuedouble, max_lat_j->valuedouble,
                                 min_lon_j->valuedouble, max_lon_j->valuedouble);
                        has_new_fence = true;
                        ESP_LOGI(TAG, "Fence update compactado para downlink LoRa (<100 bytes): %s", downlink_fence_buffer);
                    } else {
                        ESP_LOGW(TAG, "Faltan min_lat/max_lat/min_lon/max_lon en JSON de fence recibido");
                    }
                    cJSON_Delete(root);
                } else {
                    ESP_LOGW(TAG, "Fallo al parsear JSON recibido en fence update");
                }
            }
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT desconectado del broker");
            mqtt_connected = false;
            break;

        default:
            break;
    }
}

void mqtt_service_init_async(void) {
    if (mqtt_client != nullptr) {
        ESP_LOGI(TAG, "Cliente MQTT ya inicializado");
        return;
    }

    const char* target_uri = resolve_mqtt_uri();

    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = target_uri;

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
    ESP_LOGI(TAG, "Cliente MQTT arrancado en segundo plano hacia: %s", target_uri);
}

void mqtt_init_and_publish(const char* heartbeat_payload) {
    if (mqtt_client == nullptr) {
        mqtt_service_init_async();
    }

    int wait = 0;
    while (!mqtt_connected && wait < 3000) {
        vTaskDelay(pdMS_TO_TICKS(100));
        wait += 100;
    }

    if (mqtt_connected) {
        esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_HEARTBEAT,
                                heartbeat_payload, 0, 1, 0);
        ESP_LOGI(TAG, "Heartbeat publicado: %s", heartbeat_payload);
    } else {
        ESP_LOGW(TAG, "No se pudo publicar (broker MQTT no conectado aún)");
    }
}

bool mqtt_is_ready(void) { return mqtt_connected; }
bool mqtt_has_new_gps(void) { return has_new_gps; }
bool mqtt_has_new_fence(void) { return has_new_fence; }
const char* mqtt_get_downlink_gps(void) { return downlink_gps_buffer; }
const char* mqtt_get_downlink_fence(void) { return downlink_fence_buffer; }
void mqtt_reset_flags(void) { has_new_gps = false; has_new_fence = false; }
