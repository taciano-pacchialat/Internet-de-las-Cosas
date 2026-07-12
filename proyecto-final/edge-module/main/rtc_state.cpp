#include "rtc_state.h"
#include "esp_attr.h"
#include "esp_log.h"

static const char* TAG = "RTC_STATE";

// =============================================================================
// VARIABLES PERSISTENTES EN RTC MEMORY (sobreviven deep sleep)
// =============================================================================
RTC_DATA_ATTR static double current_lat = -34.9205;
RTC_DATA_ATTR static double current_lon = -57.9536;

RTC_DATA_ATTR static double fence_min_lat = -34.9225;
RTC_DATA_ATTR static double fence_max_lat = -34.9185;
RTC_DATA_ATTR static double fence_min_lon = -57.9565;
RTC_DATA_ATTR static double fence_max_lon = -57.9505;

RTC_DATA_ATTR static uint32_t wake_count = 0;
RTC_DATA_ATTR static bool last_inside = true;

void rtc_state_init_or_log(void) {
    ESP_LOGI(TAG, "=== Edge Module - Wake #%lu ===", (unsigned long)wake_count);
    ESP_LOGI(TAG, "Posición actual: lat=%.6f, lon=%.6f", current_lat, current_lon);
    ESP_LOGI(TAG, "Geofence: [%.6f,%.6f] x [%.6f,%.6f]",
             fence_min_lat, fence_max_lat, fence_min_lon, fence_max_lon);
}

void rtc_state_increment_wake_count(void) {
    wake_count++;
}

uint32_t rtc_state_get_wake_count(void) {
    return wake_count;
}

double rtc_state_get_lat(void) {
    return current_lat;
}

double rtc_state_get_lon(void) {
    return current_lon;
}

void rtc_state_set_pos(double lat, double lon) {
    current_lat = lat;
    current_lon = lon;
}

void rtc_state_get_fence(double* min_lat, double* max_lat, double* min_lon, double* max_lon) {
    if (min_lat) *min_lat = fence_min_lat;
    if (max_lat) *max_lat = fence_max_lat;
    if (min_lon) *min_lon = fence_min_lon;
    if (max_lon) *max_lon = fence_max_lon;
}

void rtc_state_set_fence(double min_lat, double max_lat, double min_lon, double max_lon) {
    fence_min_lat = min_lat;
    fence_max_lat = max_lat;
    fence_min_lon = min_lon;
    fence_max_lon = max_lon;
}

bool rtc_state_get_last_inside(void) {
    return last_inside;
}

void rtc_state_set_last_inside(bool inside) {
    last_inside = inside;
}
