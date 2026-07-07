#pragma once

// =============================================================================
// GEOFENCING — Algoritmo Bounding Box
// =============================================================================

bool geofence_is_inside(double lat, double lon,
                        double min_lat, double max_lat,
                        double min_lon, double max_lon);
