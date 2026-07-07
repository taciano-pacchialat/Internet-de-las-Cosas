#include "geofence.h"

/*
 * Bounding Box Geofence:
 *
 *     fence_max_lat ─────────────────────
 *                   |                   |
 *                   |    ● (lat, lon)   |   ← INSIDE si:
 *                   |                   |      min_lat <= lat <= max_lat
 *     fence_min_lat ─────────────────────      min_lon <= lon <= max_lon
 *                   |                   |
 *              fence_min_lon       fence_max_lon
 *
 * Para extensión futura a Point-in-Polygon, se puede reemplazar esta
 * función sin cambiar el resto de la arquitectura.
 */
bool geofence_is_inside(double lat, double lon,
                        double min_lat, double max_lat,
                        double min_lon, double max_lon) {
    return (lat >= min_lat && lat <= max_lat &&
            lon >= min_lon && lon <= max_lon);
}
