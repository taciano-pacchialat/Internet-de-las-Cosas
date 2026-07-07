#pragma once

// =============================================================================
// CONTROL DE LEDS CON RETENCIÓN EN DEEP SLEEP (RTC GPIOs)
// =============================================================================

void leds_init(void);
void leds_set_geofence_state(bool inside);
