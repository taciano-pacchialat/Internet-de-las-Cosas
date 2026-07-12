import React, { useState, useRef, useEffect } from 'react';
import { MapContainer, TileLayer, Polygon, Marker, useMap } from 'react-leaflet';
import L from 'leaflet';
import '@geoman-io/leaflet-geoman-free';
import '@geoman-io/leaflet-geoman-free/dist/leaflet-geoman.css';
import {
  Crosshair,
  MapPin,
  Edit3,
  Save,
  RotateCcw,
  Loader2,
  CheckCircle2,
  AlertTriangle,
  X
} from 'lucide-react';
import { sendGeofenceUpdate, type FenceVertex } from '../services/fenceService';

interface GeofenceMapProps {
  latitude: number;
  longitude: number;
  fenceStatus: 'INSIDE' | 'OUTSIDE' | 'UNKNOWN';
  deviceId: string;
}

// Reference polygon for the farm pasture perimeter
const INITIAL_GEOFENCE: [number, number][] = [
  [-34.9185, -57.9565],
  [-34.9185, -57.9505],
  [-34.9225, -57.9505],
  [-34.9225, -57.9565]
];

// Helper component to recenter map automatically when point changes
const MapRecenter: React.FC<{ lat: number; lng: number }> = ({ lat, lng }) => {
  const map = useMap();
  useEffect(() => {
    map.setView([lat, lng], map.getZoom());
  }, [lat, lng, map]);
  return null;
};

// Child component to attach Geoman editing to the polygon
const GeomanEditor: React.FC<{
  isEditing: boolean;
  polygonRef: React.RefObject<L.Polygon | null>;
  onVerticesChange: (coords: [number, number][]) => void;
}> = ({ isEditing, polygonRef, onVerticesChange }) => {
  const map = useMap();

  useEffect(() => {
    const polygon = polygonRef.current;
    if (!polygon) return;

    if (isEditing) {
      // Enable Geoman editing on this polygon
      (polygon as any).pm.enable({
        allowSelfIntersection: false,
        snappable: true
      });

      const handleEdit = () => {
        const latlngs = polygon.getLatLngs();
        const firstRing = Array.isArray(latlngs[0]) ? latlngs[0] : latlngs;
        const updatedCoords: [number, number][] = (firstRing as L.LatLng[]).map((pt: L.LatLng) => [
          pt.lat,
          pt.lng
        ]);
        onVerticesChange(updatedCoords);
      };

      polygon.on('pm:edit', handleEdit);
      polygon.on('pm:vertexadded', handleEdit);
      polygon.on('pm:vertexremoved', handleEdit);
      polygon.on('pm:markerdragend', handleEdit);

      return () => {
        polygon.off('pm:edit', handleEdit);
        polygon.off('pm:vertexadded', handleEdit);
        polygon.off('pm:vertexremoved', handleEdit);
        polygon.off('pm:markerdragend', handleEdit);
        (polygon as any).pm.disable();
      };
    } else {
      (polygon as any).pm?.disable();
    }
  }, [isEditing, polygonRef, onVerticesChange, map]);

  return null;
};

export const GeofenceMap: React.FC<GeofenceMapProps> = ({
  latitude,
  longitude,
  fenceStatus,
  deviceId
}) => {
  const [polygonCoords, setPolygonCoords] = useState<[number, number][]>(INITIAL_GEOFENCE);
  const [isEditing, setIsEditing] = useState<boolean>(false);
  const [isSaving, setIsSaving] = useState<boolean>(false);
  const [toast, setToast] = useState<{
    type: 'success' | 'error';
    message: string;
  } | null>(null);

  const polygonRef = useRef<L.Polygon | null>(null);
  const isOutside = fenceStatus === 'OUTSIDE';

  // Auto dismiss toast after 6 seconds
  useEffect(() => {
    if (toast) {
      const timer = setTimeout(() => setToast(null), 6000);
      return () => clearTimeout(timer);
    }
  }, [toast]);

  // Handle Save Perimeter
  const handleSaveFence = async () => {
    setIsSaving(true);
    setToast(null);

    try {
      const vertices: FenceVertex[] = polygonCoords.map((coord) => ({
        lat: coord[0],
        lon: coord[1]
      }));

      const response = await sendGeofenceUpdate(vertices);

      setIsEditing(false);
      setToast({
        type: 'success',
        message:
          response.message ||
          `Perímetro (${vertices.length} vértices) guardado y publicado vía MQTT.`
      });
    } catch (error: any) {
      setToast({
        type: 'error',
        message: error.message || 'Error al guardar el nuevo perímetro en Node-RED.'
      });
    } finally {
      setIsSaving(false);
    }
  };

  // Handle Reset Perimeter
  const handleResetFence = () => {
    setPolygonCoords(INITIAL_GEOFENCE);
    setIsEditing(false);
  };

  // Custom DivIcon with glowing radar pulse
  const cattleIcon = L.divIcon({
    className: 'custom-cattle-marker',
    html: `
      <div style="position: relative; display: flex; align-items: center; justify-content: center; width: 36px; height: 36px;">
        <div class="${isOutside ? 'marker-pulse-red' : 'marker-pulse-green'}"></div>
        <div style="width: 14px; height: 14px; border-radius: 50%; background-color: ${
          isOutside ? '#EF4444' : '#10B981'
        }; border: 2px solid #ffffff; box-shadow: 0 0 10px ${
          isOutside ? '#EF4444' : '#10B981'
        }; z-index: 10;"></div>
      </div>
    `,
    iconSize: [36, 36],
    iconAnchor: [18, 18]
  });

  return (
    <div className="relative w-full h-[450px] lg:h-[510px] rounded-xl overflow-hidden border border-hud-border bg-hud-surface shadow-lg flex flex-col">
      {/* Toast Notification HUD Banner */}
      {toast && (
        <div
          className={`absolute top-16 right-4 z-[500] max-w-md p-4 rounded-lg border shadow-xl backdrop-blur-md flex items-start gap-3 animate-in fade-in slide-in-from-top-2 duration-300 ${
            toast.type === 'success'
              ? 'bg-emerald-950/90 border-emerald-500/60 text-emerald-300 shadow-neon-green'
              : 'bg-red-950/90 border-red-500/60 text-red-300 shadow-neon-red'
          }`}
        >
          {toast.type === 'success' ? (
            <CheckCircle2 className="w-5 h-5 text-emerald-400 shrink-0 mt-0.5" />
          ) : (
            <AlertTriangle className="w-5 h-5 text-red-400 shrink-0 mt-0.5" />
          )}
          <div className="flex-1 text-xs font-mono">
            <strong className="block uppercase tracking-wider font-bold mb-1">
              {toast.type === 'success' ? 'PERÍMETRO ACTUALIZADO (200 OK)' : 'ERROR EN LA OPERACIÓN'}
            </strong>
            <p className="text-gray-200">{toast.message}</p>
          </div>
          <button
            onClick={() => setToast(null)}
            className="text-gray-400 hover:text-white transition-colors"
          >
            <X className="w-4 h-4" />
          </button>
        </div>
      )}

      {/* Top Controls Toolbar for Polygon Editing */}
      <div className="absolute top-4 right-4 z-[400] flex items-center gap-2 bg-hud-surface/95 backdrop-blur-md border border-hud-border rounded-lg p-1.5 shadow-xl">
        {/* Toggle Edit Button */}
        <button
          onClick={() => setIsEditing(!isEditing)}
          className={`px-3 py-1.5 rounded text-xs font-mono font-bold tracking-wider transition-all flex items-center gap-1.5 border ${
            isEditing
              ? 'bg-amber-500/20 border-amber-500/60 text-amber-300 shadow-md'
              : 'bg-hud-card hover:bg-hud-border border-hud-border text-gray-300 hover:text-white'
          }`}
        >
          <Edit3 className="w-3.5 h-3.5" />
          {isEditing ? 'EDITANDO VÉRTICES' : 'MODIFICAR CORRAL'}
        </button>

        {/* Reset Button (only shown when editing) */}
        {isEditing && (
          <button
            onClick={handleResetFence}
            disabled={isSaving}
            className="px-2.5 py-1.5 rounded text-xs font-mono border border-hud-border bg-hud-card hover:bg-hud-border text-gray-300 hover:text-white transition-colors flex items-center gap-1"
            title="Restablecer perímetro inicial"
          >
            <RotateCcw className="w-3.5 h-3.5" />
            RESET
          </button>
        )}

        {/* Save Button (Industrial Highlighted) */}
        {isEditing && (
          <button
            onClick={handleSaveFence}
            disabled={isSaving}
            className="px-3.5 py-1.5 rounded text-xs font-mono font-bold tracking-wider transition-all flex items-center gap-1.5 bg-emerald-500 hover:bg-emerald-400 text-black shadow-neon-green disabled:opacity-50 disabled:cursor-not-allowed"
          >
            {isSaving ? (
              <>
                <Loader2 className="w-3.5 h-3.5 animate-spin text-black" />
                ENVIANDO...
              </>
            ) : (
              <>
                <Save className="w-3.5 h-3.5 text-black" />
                GUARDAR NUEVO PERÍMETRO
              </>
            )}
          </button>
        )}
      </div>

      {/* HUD Tactical Overlay header inside the map */}
      <div className="absolute top-4 left-4 z-[400] bg-hud-surface/90 backdrop-blur-md border border-hud-border rounded-lg p-3 shadow-xl max-w-xs pointer-events-none">
        <div className="flex items-center gap-2 mb-1">
          <Crosshair className="w-4 h-4 text-cyan-400" />
          <span className="text-xs font-mono font-bold tracking-wider text-gray-200">
            RADAR PERIMETER VIEW
          </span>
        </div>
        <div className="grid grid-cols-2 gap-x-4 gap-y-1 text-xs font-mono text-gray-300 mt-1">
          <div>
            <span className="text-gray-400 block text-[10px]">LATITUDE</span>
            <span className="text-white font-semibold">{latitude.toFixed(6)}</span>
          </div>
          <div>
            <span className="text-gray-400 block text-[10px]">LONGITUDE</span>
            <span className="text-white font-semibold">{longitude.toFixed(6)}</span>
          </div>
        </div>
        {isEditing && (
          <div className="mt-2 pt-1.5 border-t border-hud-border text-[10px] font-mono text-amber-400">
            ARRASTRA LOS VÉRTICES DEL CORRAL EN EL MAPA PARA RECONFIGURAR
          </div>
        )}
      </div>

      {/* Map Container */}
      <div className="flex-1 w-full">
        <MapContainer
          center={[latitude, longitude]}
          zoom={16}
          scrollWheelZoom={true}
          className="w-full h-full"
        >
          <MapRecenter lat={latitude} lng={longitude} />

          {/* CartoDB Dark Matter Industrial Map Tiles */}
          <TileLayer
            attribution='&copy; <a href="https://carto.com/">CARTO</a>'
            url="https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png"
          />

          {/* Geofence Perimeter Polygon */}
          <Polygon
            ref={polygonRef}
            positions={polygonCoords}
            pathOptions={{
              color: isEditing
                ? '#F59E0B'
                : isOutside
                ? '#EF4444'
                : '#10B981',
              weight: isEditing ? 3 : 2,
              dashArray: isEditing ? 'none' : '6, 6',
              fillColor: isEditing
                ? '#F59E0B'
                : isOutside
                ? '#EF4444'
                : '#10B981',
              fillOpacity: isEditing ? 0.25 : 0.12
            }}
          />

          {/* Attach Geoman Polygon Editor */}
          <GeomanEditor
            isEditing={isEditing}
            polygonRef={polygonRef}
            onVerticesChange={(updated) => setPolygonCoords(updated)}
          />

          {/* Cattle Marker */}
          <Marker position={[latitude, longitude]} icon={cattleIcon} />
        </MapContainer>
      </div>

      {/* Map Footer HUD readout */}
      <div className="absolute bottom-4 right-4 z-[400] bg-hud-surface/90 backdrop-blur-md border border-hud-border rounded-lg px-3 py-1.5 flex items-center gap-2 pointer-events-none">
        <MapPin className="w-3.5 h-3.5 text-emerald-400" />
        <span className="text-[11px] font-mono text-gray-300">
          TRACKED NODE: <strong className="text-white">{deviceId}</strong> | VÉRTICES:{' '}
          <strong className="text-cyan-400">{polygonCoords.length}</strong>
        </span>
      </div>
    </div>
  );
};
