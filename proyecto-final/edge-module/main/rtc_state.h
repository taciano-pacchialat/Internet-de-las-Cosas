#pragma once

#include <cstdint>

// =============================================================================
// GESTIÓN DE DATOS PERSISTENTES EN RTC MEMORY
// =============================================================================

void rtc_state_init_or_log(void);
void rtc_state_increment_wake_count(void);
uint32_t rtc_state_get_wake_count(void);

double rtc_state_get_lat(void);
double rtc_state_get_lon(void);
void rtc_state_set_pos(double lat, double lon);

void rtc_state_get_fence(double* min_lat, double* max_lat, double* min_lon, double* max_lon);
void rtc_state_set_fence(double min_lat, double max_lat, double min_lon, double max_lon);

bool rtc_state_get_last_inside(void);
void rtc_state_set_last_inside(bool inside);
