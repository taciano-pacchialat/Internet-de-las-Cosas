import React, { useEffect, useRef } from 'react';
import { MapContainer, TileLayer, Polygon, Marker, Rectangle, useMap } from 'react-leaflet';
import L from 'leaflet';
import { Crosshair, MapPin, ShieldAlert, ShieldCheck } from 'lucide-react';

interface GeofenceMapProps {
  latitude: number;
  longitude: number;
  fenceStatus: 'INSIDE' | 'OUTSIDE' | 'UNKNOWN';
  deviceId: string;
  polygonCoords: [number, number][];
  selectedVertexIndex: number | null;
  onSelectVertex: (index: number | null) => void;
  onCoordsChange: (coords: [number, number][]) => void;
  showBoundingBox: boolean;
  onToggleBoundingBox: () => void;
}

// Helper para recentrar suavemente el mapa
const MapRecenter: React.FC<{ lat: number; lng: number }> = ({ lat, lng }) => {
  const map = useMap();
  useEffect(() => {
    map.setView([lat, lng], map.getZoom(), { animate: true });
  }, [lat, lng, map]);
  return null;
};

// Editor de polígonos que soporta movimiento por arrastre Y movimiento por teclado
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
          width: ${isSelected ? '36px' : '30px'};
          height: ${isSelected ? '36px' : '30px'};
          border-radius: 50%;
          background-color: ${isSelected ? '#06B6D4' : '#F59E0B'};
          border: ${isSelected ? '3px solid #FFFFFF' : '2px solid #FFFFFF'};
          box-shadow: ${
            isSelected
              ? '0 0 18px rgba(6, 182, 212, 1), inset 0 0 4px #fff'
              : '0 0 10px rgba(245, 158, 11, 0.9)'
          };
          display: flex;
          align-items: center;
          justify-content: center;
          font-family: monospace;
          font-weight: bold;
          font-size: ${isSelected ? '14px' : '12px'};
          color: #000000;
          cursor: grab;
          user-select: none;
          transition: all 0.15s ease;
        ">V${idx + 1}</div>
      `,
      iconSize: [isSelected ? 36 : 30, isSelected ? 36 : 30],
      iconAnchor: [isSelected ? 18 : 15, isSelected ? 18 : 15]
    });

  return (
    <>
      {/* Polígono del Corral Geofence */}
      <Polygon
        ref={polygonRef}
        positions={coords}
        pathOptions={{
          color: selectedVertexIndex !== null ? '#F59E0B' : isOutside ? '#EF4444' : '#10B981',
          weight: 3,
          dashArray: 'none',
          fillColor: selectedVertexIndex !== null ? '#F59E0B' : isOutside ? '#EF4444' : '#10B981',
          fillOpacity: 0.25
        }}
      />

      {/* MARCADORES DE CADA VÉRTICE (Clickeables para seleccionar y Arrastrables) */}
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
  latitude,
  longitude,
  fenceStatus,
  deviceId,
  polygonCoords,
  selectedVertexIndex,
  onSelectVertex,
  onCoordsChange,
  showBoundingBox,
  onToggleBoundingBox
}) => {
  const isOutside = fenceStatus === 'OUTSIDE';

  // Cálculo del Bounding Box hardware del collar
  const minLat = Math.min(...polygonCoords.map((c) => c[0]));
  const maxLat = Math.max(...polygonCoords.map((c) => c[0]));
  const minLon = Math.min(...polygonCoords.map((c) => c[1]));
  const maxLon = Math.max(...polygonCoords.map((c) => c[1]));

  const boundingBoxBounds: [[number, number], [number, number]] = [
    [minLat, minLon],
    [maxLat, maxLon]
  ];

  const isInsideHardwareBox =
    latitude >= minLat && latitude <= maxLat && longitude >= minLon && longitude <= maxLon;

  const cattleIcon = L.divIcon({
    className: 'custom-cattle-marker',
    html: `
      <div style="position: relative; display: flex; align-items: center; justify-content: center; width: 40px; height: 40px;">
        <div class="${isOutside ? 'marker-pulse-red' : 'marker-pulse-green'}"></div>
        <div style="width: 18px; height: 18px; border-radius: 50%; background-color: ${
          isOutside ? '#EF4444' : '#10B981'
        }; border: 2.5px solid #ffffff; box-shadow: 0 0 14px ${
          isOutside ? '#EF4444' : '#10B981'
        }; z-index: 10;"></div>
      </div>
    `,
    iconSize: [40, 40],
    iconAnchor: [20, 20]
  });

  return (
    <div className="relative w-full h-[580px] lg:h-[660px] xl:h-[700px] rounded-xl overflow-hidden border border-hud-border bg-hud-surface shadow-2xl flex flex-col">
      {/* BARRA SUPERIOR TÁCTICA DEL MAPA (z-1000 SIEMPRE ACCESIBLE) */}
      <div className="absolute top-4 right-4 z-[1000] flex items-center gap-2 bg-hud-surface/95 backdrop-blur-md border border-hud-border rounded-lg p-2 shadow-2xl">
        <button
          onClick={onToggleBoundingBox}
          className={`px-3 py-1.5 rounded text-xs font-mono font-bold tracking-wider transition-all flex items-center gap-1.5 border ${
            showBoundingBox
              ? 'bg-cyan-500/20 border-cyan-500/80 text-cyan-300 shadow-md'
              : 'bg-hud-card hover:bg-hud-border border-hud-border text-gray-300 hover:text-white'
          }`}
          title="Muestra la caja límite hardware (min/max lat/lon) que evalúa el ESP32 Edge"
        >
          {showBoundingBox ? 'CAJA HW: ACTIVA' : 'VER CAJA HW'}
        </button>
      </div>

      {/* PANEL RADAR HUD IZQUIERDO (z-900) */}
      <div className="absolute top-4 left-4 z-[900] bg-hud-surface/90 backdrop-blur-md border border-hud-border rounded-lg p-3 shadow-xl max-w-xs pointer-events-none">
        <div className="flex items-center justify-between gap-3 mb-1.5">
          <div className="flex items-center gap-1.5">
            <Crosshair className="w-4 h-4 text-cyan-400" />
            <span className="text-xs font-mono font-bold tracking-wider text-gray-200">
              RADAR PERIMETRAL
            </span>
          </div>
          {isInsideHardwareBox ? (
            <span className="inline-flex items-center gap-1 px-1.5 py-0.5 rounded text-[10px] font-mono font-bold bg-emerald-500/20 text-emerald-300 border border-emerald-500/40">
              <ShieldCheck className="w-3 h-3" /> HW: INSIDE
            </span>
          ) : (
            <span className="inline-flex items-center gap-1 px-1.5 py-0.5 rounded text-[10px] font-mono font-bold bg-red-500/20 text-red-300 border border-red-500/40">
              <ShieldAlert className="w-3 h-3" /> HW: OUTSIDE
            </span>
          )}
        </div>
        <div className="grid grid-cols-2 gap-x-4 gap-y-1 text-xs font-mono text-gray-300">
          <div>
            <span className="text-gray-400 block text-[10px]">LATITUD NODO</span>
            <span className="text-white font-semibold">{latitude.toFixed(6)}</span>
          </div>
          <div>
            <span className="text-gray-400 block text-[10px]">LONGITUD NODO</span>
            <span className="text-white font-semibold">{longitude.toFixed(6)}</span>
          </div>
        </div>
        <div className="mt-2 pt-1.5 border-t border-hud-border text-[10px] font-mono text-cyan-300 leading-tight">
          SELECCIONA UN VÉRTICE EN LA LISTA LATERAL PARA DESPLAZARLO CON TECLADO O BOTONES
        </div>
      </div>

      {/* CONTENEDOR DEL MAPA LEAFLET */}
      <div className="flex-1 w-full">
        <MapContainer
          center={[latitude, longitude]}
          zoom={16}
          scrollWheelZoom={true}
          className="w-full h-full"
        >
          <MapRecenter lat={latitude} lng={longitude} />

          {/* Teselas CartoDB Dark Matter */}
          <TileLayer
            attribution='&copy; <a href="https://carto.com/">CARTO</a>'
            url="https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png"
          />

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

          {/* Polígono y marcadores interactivos V1, V2... */}
          <LivePolygonEditor
            coords={polygonCoords}
            selectedVertexIndex={selectedVertexIndex}
            isOutside={isOutside}
            onSelectVertex={onSelectVertex}
            onCoordsChange={onCoordsChange}
          />

          {/* Marcador del Ganado (Collar IoT) */}
          <Marker position={[latitude, longitude]} icon={cattleIcon} />
        </MapContainer>
      </div>

      {/* PIE DE MAPA TÁCTICO HUD (z-900) */}
      <div className="absolute bottom-4 right-4 z-[900] bg-hud-surface/90 backdrop-blur-md border border-hud-border rounded-lg px-3 py-1.5 flex items-center gap-3 pointer-events-none text-[11px] font-mono text-gray-300">
        <div className="flex items-center gap-1.5">
          <MapPin className="w-3.5 h-3.5 text-emerald-400" />
          <span>
            DISPOSITIVO: <strong className="text-white">{deviceId}</strong>
          </span>
        </div>
        <span className="text-hud-border">|</span>
        <div>
          VÉRTICES: <strong className="text-amber-400">{polygonCoords.length}</strong>
        </div>
      </div>
    </div>
  );
};
