import React, { useEffect, useRef } from 'react';
import { MapContainer, TileLayer, Polygon, Polyline, Marker, Rectangle, useMap } from 'react-leaflet';
import L from 'leaflet';
import { Crosshair, MapPin, ShieldAlert, ShieldCheck, Route } from 'lucide-react';
import type { CattlePositionPoint } from '../types/telemetry';

interface GeofenceMapProps {
  viewMode: 'Live' | 'Trail';
  selectedDevice: string;
  latestPointsByDevice: CattlePositionPoint[];
  trailsByDevice: Record<string, CattlePositionPoint[]>;
  polygonCoords: [number, number][];
  selectedVertexIndex: number | null;
  onSelectVertex: (index: number | null) => void;
  onCoordsChange: (coords: [number, number][]) => void;
  showBoundingBox: boolean;
  onToggleBoundingBox: () => void;
  latitude?: number;
  longitude?: number;
  fenceStatus?: string;
  deviceId?: string;
}

const DEVICE_COLORS = [
  '#06B6D4', // cyan-500
  '#10B981', // emerald-500
  '#F59E0B', // amber-500
  '#A855F7', // purple-500
  '#EC4899', // pink-500
  '#3B82F6', // blue-500
];

const getDeviceColor = (index: number) => {
  return DEVICE_COLORS[index % DEVICE_COLORS.length];
};

// Helper para recentrar suavemente el mapa
const MapRecenter: React.FC<{ lat: number; lng: number }> = ({ lat, lng }) => {
  const map = useMap();
  useEffect(() => {
    map.setView([lat, lng], map.getZoom(), { animate: true });
  }, [lat, lng, map]);
  return null;
};

// Editor de polígonos (solo en modo Live)
const LivePolygonEditor: React.FC<{
  coords: [number, number][];
  selectedVertexIndex: number | null;
  isOutside: boolean;
  onSelectVertex: (index: number | null) => void;
  onCoordsChange: (newCoords: [number, number][]) => void;
}> = ({ coords, selectedVertexIndex, isOutside, onSelectVertex, onCoordsChange }) => {
  const polygonRef = useRef<L.Polygon | null>(null);
  const coordsRef = useRef<[number, number][]>(coords);

  useEffect(() => {
    coordsRef.current = coords;
  }, [coords]);

  const createVertexIcon = (idx: number, isSelected: boolean) =>
    L.divIcon({
      className: 'custom-vertex-handle',
      html: `
        <div style="
          width: ${isSelected ? '32px' : '26px'};
          height: ${isSelected ? '32px' : '26px'};
          border-radius: 6px;
          background-color: ${isSelected ? '#06B6D4' : '#374151'};
          border: ${isSelected ? '2px solid #FFFFFF' : '2px solid #9CA3AF'};
          box-shadow: ${
            isSelected
              ? '0 0 14px rgba(6, 182, 212, 0.8)'
              : '0 2px 4px rgba(0,0,0,0.5)'
          };
          display: flex;
          align-items: center;
          justify-content: center;
          font-family: monospace;
          font-weight: bold;
          font-size: ${isSelected ? '13px' : '11px'};
          color: #F9FAFB;
          cursor: grab;
          user-select: none;
          transition: all 0.15s ease;
        ">V${idx + 1}</div>
      `,
      iconSize: [isSelected ? 32 : 26, isSelected ? 32 : 26],
      iconAnchor: [isSelected ? 16 : 13, isSelected ? 16 : 13]
    });

  return (
    <>
      {/* Polígono del Corral Geofence */}
      <Polygon
        ref={polygonRef}
        positions={coords}
        pathOptions={{
          color: selectedVertexIndex !== null ? '#06B6D4' : isOutside ? '#EF4444' : '#10B981',
          weight: 2.5,
          dashArray: 'none',
          fillColor: selectedVertexIndex !== null ? '#06B6D4' : isOutside ? '#EF4444' : '#10B981',
          fillOpacity: 0.18
        }}
      />

      {/* Vértices interactivos V1..V4 */}
      {coords.map((coord, idx) => {
        const isSelected = selectedVertexIndex === idx;
        return (
          <Marker
            key={`vertex-handle-${idx}`}
            position={coord}
            draggable={true}
            icon={createVertexIcon(idx, isSelected)}
            eventHandlers={{
              click: () => {
                onSelectVertex(isSelected ? null : idx);
              },
              dragstart: () => {
                onSelectVertex(idx);
              },
              drag: (e) => {
                const latlng = e.target.getLatLng();
                coordsRef.current[idx] = [latlng.lat, latlng.lng];
                if (polygonRef.current) {
                  polygonRef.current.setLatLngs(coordsRef.current);
                }
              },
              dragend: (e) => {
                const latlng = e.target.getLatLng();
                coordsRef.current[idx] = [latlng.lat, latlng.lng];
                onCoordsChange([...coordsRef.current]);
              }
            }}
          />
        );
      })}
    </>
  );
};

export const GeofenceMap: React.FC<GeofenceMapProps> = ({
  viewMode,
  selectedDevice,
  latestPointsByDevice,
  trailsByDevice,
  polygonCoords,
  selectedVertexIndex,
  onSelectVertex,
  onCoordsChange,
  showBoundingBox,
  onToggleBoundingBox,
  latitude = -34.9205,
  longitude = -57.9536,
  fenceStatus = 'INSIDE'
}) => {
  const isOutside = fenceStatus === 'OUTSIDE';

  // Cálculo del Bounding Box hardware
  const minLat = Math.min(...polygonCoords.map((c) => c[0]));
  const maxLat = Math.max(...polygonCoords.map((c) => c[0]));
  const minLon = Math.min(...polygonCoords.map((c) => c[1]));
  const maxLon = Math.max(...polygonCoords.map((c) => c[1]));

  const boundingBoxBounds: [[number, number], [number, number]] = [
    [minLat, minLon],
    [maxLat, maxLon]
  ];

  // Determinar centro del mapa
  const displayLat =
    latestPointsByDevice.length > 0 ? latestPointsByDevice[0].latitude : latitude;
  const displayLon =
    latestPointsByDevice.length > 0 ? latestPointsByDevice[0].longitude : longitude;

  // Creador de ícono para animales en modo Live
  const createCattleMarkerIcon = (devId: string, status: string, color: string) =>
    L.divIcon({
      className: 'custom-cattle-marker',
      html: `
        <div style="position: relative; display: flex; align-items: center; justify-content: center; width: 42px; height: 42px;">
          <div style="
            position: absolute;
            width: 36px;
            height: 36px;
            border-radius: 50%;
            background-color: ${status === 'OUTSIDE' ? '#EF4444' : color};
            opacity: 0.25;
          "></div>
          <div style="
            width: 28px;
            height: 28px;
            border-radius: 6px;
            background-color: #1F2937;
            border: 2px solid ${status === 'OUTSIDE' ? '#EF4444' : color};
            box-shadow: 0 0 10px ${status === 'OUTSIDE' ? '#EF4444' : color};
            display: flex;
            align-items: center;
            justify-content: center;
            font-family: monospace;
            font-size: 9px;
            font-weight: bold;
            color: #F3F4F6;
            z-index: 10;
          ">${devId.replace(/^.*-/, '')}</div>
        </div>
      `,
      iconSize: [42, 42],
      iconAnchor: [21, 21]
    });

  // Creador de ícono para vectores de sentido (flecha orientada con Math.atan2)
  const createDirectionArrowIcon = (angleDeg: number, color: string) =>
    L.divIcon({
      className: 'custom-direction-arrow',
      html: `
        <div style="
          transform: rotate(${angleDeg}deg);
          display: flex;
          align-items: center;
          justify-content: center;
          width: 24px;
          height: 24px;
          filter: drop-shadow(0 1px 2px rgba(0,0,0,0.85));
        ">
          <svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="${color}" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
            <path d="M12 19V5M5 12l7-7 7 7"/>
          </svg>
        </div>
      `,
      iconSize: [24, 24],
      iconAnchor: [12, 12]
    });

  // Dispositivos a renderizar en modo Trail
  const trailDevices =
    selectedDevice === 'ALL'
      ? Object.keys(trailsByDevice)
      : Object.keys(trailsByDevice).filter((id) => id === selectedDevice);

  return (
    <div className="relative w-full h-full rounded-xl overflow-hidden border border-gray-700 shadow-2xl flex flex-col bg-gray-900">
      {/* HUD DE BOTÓN PARA CAJA HARDWARE (Solo relevante en modo Live) */}
      {viewMode === 'Live' && (
        <div className="absolute top-4 right-4 z-[1000] flex items-center gap-2 bg-gray-900/90 backdrop-blur-md border border-gray-700 rounded-lg p-1.5 shadow-lg">
          <button
            onClick={onToggleBoundingBox}
            className={`px-3 py-1.5 rounded text-xs font-mono font-bold tracking-wider transition-all flex items-center gap-1.5 border ${
              showBoundingBox
                ? 'bg-cyan-500/20 border-cyan-500 text-cyan-300 shadow-sm'
                : 'bg-gray-800 hover:bg-gray-750 border-gray-700 text-gray-300 hover:text-white'
            }`}
            title="Muestra la caja límite hardware (min/max lat/lon) que evalúa el ESP32 Edge"
          >
            {showBoundingBox ? 'CAJA HW: ACTIVA' : 'VER CAJA HW'}
          </button>
        </div>
      )}

      {/* HUD SUPERIOR IZQUIERDO */}
      <div className="absolute top-4 left-4 z-[900] bg-gray-900/90 backdrop-blur-md border border-gray-700 rounded-xl p-3.5 shadow-xl max-w-sm">
        <div className="flex items-center justify-between mb-2 gap-3">
          <div className="flex items-center gap-2">
            <Crosshair className="w-4 h-4 text-cyan-400" />
            <span className="text-xs font-mono font-bold tracking-wider text-gray-200 uppercase">
              {viewMode === 'Live' ? 'RADAR EN VIVO (LIVE)' : 'ANÁLISIS DE RUTAS (TRAIL)'}
            </span>
          </div>
          {viewMode === 'Live' ? (
            isOutside ? (
              <span className="inline-flex items-center gap-1 px-1.5 py-0.5 rounded text-[10px] font-mono font-bold bg-red-500/20 text-red-300 border border-red-500/40">
                <ShieldAlert className="w-3 h-3" /> OUTSIDE
              </span>
            ) : (
              <span className="inline-flex items-center gap-1 px-1.5 py-0.5 rounded text-[10px] font-mono font-bold bg-emerald-500/20 text-emerald-300 border border-emerald-500/40">
                <ShieldCheck className="w-3 h-3" /> INSIDE
              </span>
            )
          ) : (
            <span className="inline-flex items-center gap-1 px-1.5 py-0.5 rounded text-[10px] font-mono font-bold bg-cyan-500/20 text-cyan-300 border border-cyan-500/40">
              <Route className="w-3 h-3" /> RUTAS
            </span>
          )}
        </div>
        <div className="grid grid-cols-2 gap-x-4 gap-y-1 text-xs font-mono text-gray-200">
          <div>
            <span className="text-gray-400 block text-[10px]">CENTRO LAT</span>
            <span className="text-cyan-400 font-semibold">{displayLat.toFixed(6)}</span>
          </div>
          <div>
            <span className="text-gray-400 block text-[10px]">CENTRO LON</span>
            <span className="text-cyan-400 font-semibold">{displayLon.toFixed(6)}</span>
          </div>
        </div>
        <div className="mt-2 pt-1.5 border-t border-gray-800 text-[10px] font-mono text-gray-400 leading-tight">
          {viewMode === 'Live'
            ? 'Mostrando posición actual y perímetro Geofence persistido.'
            : 'Polígono oculto para despejar vista. Flechas indican el sentido de avance en los últimos puntos.'}
        </div>
      </div>

      {/* CONTENEDOR DEL MAPA LEAFLET */}
      <div className="flex-1 w-full">
        <MapContainer
          center={[displayLat, displayLon]}
          zoom={16}
          scrollWheelZoom={true}
          className="w-full h-full"
        >
          <MapRecenter lat={displayLat} lng={displayLon} />

          {/* CartoDB Dark Matter */}
          <TileLayer
            attribution='&copy; <a href="https://carto.com/">CARTO</a>'
            url="https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png"
          />

          {viewMode === 'Live' ? (
            <>
              {/* Caja Hardware Bounding Box */}
              {showBoundingBox && (
                <Rectangle
                  bounds={boundingBoxBounds}
                  pathOptions={{
                    color: '#06B6D4',
                    weight: 1.5,
                    dashArray: '4, 4',
                    fill: false
                  }}
                />
              )}

              {/* Polígono y marcadores interactivos V1..V4 */}
              <LivePolygonEditor
                coords={polygonCoords}
                selectedVertexIndex={selectedVertexIndex}
                isOutside={isOutside}
                onSelectVertex={onSelectVertex}
                onCoordsChange={onCoordsChange}
              />

              {/* Exclusivamente la posición más reciente de CADA vaca/dispositivo */}
              {latestPointsByDevice.map((pt, index) => {
                const color = getDeviceColor(index);
                return (
                  <Marker
                    key={`live-marker-${pt.device_id}`}
                    position={[pt.latitude, pt.longitude]}
                    icon={createCattleMarkerIcon(pt.device_id, pt.fence_status, color)}
                  />
                );
              })}
            </>
          ) : (
            <>
              {/* MODO TRAIL: Sin polígono de Geocerca. Renderiza polylines históricas y flechas de sentido */}
              {trailDevices.map((devId, devIndex) => {
                const points = trailsByDevice[devId] || [];
                if (points.length === 0) return null;

                const color = getDeviceColor(devIndex);
                const positions = points.map((p): [number, number] => [p.latitude, p.longitude]);

                // Últimos 2 o 3 puntos de la línea para calcular y mostrar vectores de sentido con Math.atan2
                const arrowSegments: Array<{ lat: number; lon: number; angle: number }> = [];
                const startIdx = Math.max(1, points.length - 3);

                for (let i = startIdx; i < points.length; i++) {
                  const pPrev = points[i - 1];
                  const pCurr = points[i];
                  const dy = pCurr.latitude - pPrev.latitude;
                  const dx = pCurr.longitude - pPrev.longitude;

                  // Evitar calcular si los puntos son idénticos
                  if (Math.abs(dy) > 0.000001 || Math.abs(dx) > 0.000001) {
                    const angleDeg = Math.atan2(dx, dy) * (180 / Math.PI);
                    arrowSegments.push({
                      lat: pCurr.latitude,
                      lon: pCurr.longitude,
                      angle: angleDeg
                    });
                  }
                }

                const latestPt = points[points.length - 1];

                return (
                  <React.Fragment key={`trail-group-${devId}`}>
                    {/* Línea del recorrido histórico */}
                    {positions.length > 1 && (
                      <Polyline
                        positions={positions}
                        pathOptions={{
                          color,
                          weight: 3.5,
                          opacity: 0.85
                        }}
                      />
                    )}

                    {/* Flechas de sentido nativas de Leaflet (L.divIcon) en los últimos 2 o 3 puntos */}
                    {arrowSegments.map((seg, segIdx) => (
                      <Marker
                        key={`arrow-${devId}-${segIdx}`}
                        position={[seg.lat, seg.lon]}
                        icon={createDirectionArrowIcon(seg.angle, color)}
                      />
                    ))}

                    {/* Marcador final / actual del dispositivo */}
                    <Marker
                      position={[latestPt.latitude, latestPt.longitude]}
                      icon={createCattleMarkerIcon(devId, latestPt.fence_status, color)}
                    />
                  </React.Fragment>
                );
              })}
            </>
          )}
        </MapContainer>
      </div>

      {/* PIE DE MAPA HUD */}
      <div className="absolute bottom-4 right-4 z-[900] bg-gray-900/90 backdrop-blur-md border border-gray-700 rounded-lg px-3 py-1.5 flex items-center gap-3 pointer-events-none text-[11px] font-mono text-gray-300">
        <div className="flex items-center gap-1.5">
          <MapPin className="w-3.5 h-3.5 text-cyan-400" />
          <span>
            MODO: <strong className="text-white uppercase">{viewMode}</strong>
          </span>
        </div>
        <span className="text-gray-600">|</span>
        <div>
          DISPOSITIVOS: <strong className="text-cyan-400">{latestPointsByDevice.length}</strong>
        </div>
      </div>
    </div>
  );
};
