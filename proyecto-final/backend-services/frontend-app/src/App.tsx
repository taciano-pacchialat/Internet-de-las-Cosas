import { useState, useEffect, useCallback } from 'react';
import { useCattleTelemetry } from './hooks/useCattleTelemetry';
import { Header } from './components/Header';
import { MetricCard } from './components/MetricCard';
import { GeofenceMap } from './components/GeofenceMap';
import { TelemetryCharts } from './components/TelemetryCharts';
import { sendGeofenceUpdate, fetchCurrentGeofence, type FenceVertex } from './services/fenceService';
import {
  Battery,
  Radio,
  Signal,
  Activity,
  Navigation,
  Loader2,
  Save,
  RotateCcw,
  CheckCircle2,
  AlertTriangle,
  X,
  ArrowUp,
  ArrowDown,
  ArrowLeft,
  ArrowRight,
  Keyboard
} from 'lucide-react';

const INITIAL_GEOFENCE: [number, number][] = [
  [-34.9185, -57.9565],
  [-34.9185, -57.9505],
  [-34.9225, -57.9505],
  [-34.9225, -57.9565]
];

export default function App() {
  const {
    latestPoint,
    latestPointsByDevice,
    trailsByDevice,
    availableDevices,
    history,
    isLoading,
    isSimulated,
    refetch
  } = useCattleTelemetry(3000);

  // Coordenadas y vértices del corral
  const [polygonCoords, setPolygonCoords] = useState<[number, number][]>(INITIAL_GEOFENCE);
  const [selectedVertexIndex, setSelectedVertexIndex] = useState<number | null>(0);
  const [showBoundingBox, setShowBoundingBox] = useState<boolean>(true);
  const [isSaving, setIsSaving] = useState<boolean>(false);
  const [toast, setToast] = useState<{ type: 'success' | 'error'; message: string } | null>(null);

  // Modo de visualización del mapa: Live vs Trail
  const [viewMode, setViewMode] = useState<'Live' | 'Trail'>('Live');
  const [selectedDevice, setSelectedDevice] = useState<string>('ALL');

  // Sincronización Inicial con InfluxDB al cargar la app (evita desincronización tras F5)
  useEffect(() => {
    async function syncPersistedFence() {
      const persisted = await fetchCurrentGeofence();
      if (persisted && persisted.length >= 3) {
        setPolygonCoords(persisted);
      }
    }
    syncPersistedFence();
  }, []);

  // Ocultar toast automáticamente
  useEffect(() => {
    if (toast) {
      const timer = setTimeout(() => setToast(null), 6000);
      return () => clearTimeout(timer);
    }
  }, [toast]);

  // Mover un vértice específico mediante offsets dLat y dLon
  const moveVertex = useCallback((idx: number, dLat: number, dLon: number) => {
    setPolygonCoords((prev) => {
      const updated: [number, number][] = [...prev];
      if (updated[idx]) {
        updated[idx] = [
          Number((updated[idx][0] + dLat).toFixed(6)),
          Number((updated[idx][1] + dLon).toFixed(6))
        ];
      }
      return updated;
    });
  }, []);

  // Control por teclado con las flechas de dirección (↑ ↓ ← →)
  useEffect(() => {
    if (selectedVertexIndex === null) return;

    const handleKeyDown = (e: KeyboardEvent) => {
      const activeTag = (document.activeElement as HTMLElement)?.tagName;
      if (['INPUT', 'TEXTAREA'].includes(activeTag)) return;

      // Paso estándar: 0.00005° (~5 metros). Shift = Rápido (~30m), Alt = Fino (~1 metro)
      let step = 0.00005;
      if (e.shiftKey) step = 0.0003;
      if (e.altKey) step = 0.00001;

      if (e.key === 'ArrowUp') {
        e.preventDefault();
        moveVertex(selectedVertexIndex, step, 0);
      } else if (e.key === 'ArrowDown') {
        e.preventDefault();
        moveVertex(selectedVertexIndex, -step, 0);
      } else if (e.key === 'ArrowRight') {
        e.preventDefault();
        moveVertex(selectedVertexIndex, 0, step);
      } else if (e.key === 'ArrowLeft') {
        e.preventDefault();
        moveVertex(selectedVertexIndex, 0, -step);
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [selectedVertexIndex, moveVertex]);

  // Guardar y publicar el perímetro al broker MQTT via Node-RED
  const handleSaveFence = async () => {
    setIsSaving(true);
    setToast(null);

    try {
      const vertices: FenceVertex[] = polygonCoords.map((coord) => ({
        lat: coord[0],
        lon: coord[1]
      }));

      const response = await sendGeofenceUpdate(vertices);

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

  // Reset del perímetro
  const handleResetFence = () => {
    setPolygonCoords(INITIAL_GEOFENCE);
    setSelectedVertexIndex(0);
  };

  // Default values if loading
  const lat = latestPoint?.latitude ?? -34.9205;
  const lon = latestPoint?.longitude ?? -57.9536;
  const fenceStatus = latestPoint?.fence_status ?? 'INSIDE';
  const deviceId = latestPoint?.device_id ?? 'vaca-01';
  const battery = latestPoint?.battery ?? 0;
  const rssi = latestPoint?.rssi ?? 0;
  const snr = latestPoint?.snr ?? 0;
  const wakeCount = latestPoint?.wake_count ?? 0;

  return (
    <div className="min-h-screen bg-hud-bg flex flex-col justify-between selection:bg-cyan-500 selection:text-black">
      {/* Toast de Alertas y Confirmaciones MQTT */}
      {toast && (
        <div
          className={`fixed top-5 right-5 z-[2000] max-w-md p-4 rounded-lg border shadow-2xl backdrop-blur-md flex items-start gap-3 animate-in fade-in slide-in-from-top-2 duration-300 ${
            toast.type === 'success'
              ? 'bg-emerald-950/95 border-emerald-500/80 text-emerald-300 shadow-neon-green'
              : 'bg-red-950/95 border-red-500/80 text-red-300 shadow-neon-red'
          }`}
        >
          {toast.type === 'success' ? (
            <CheckCircle2 className="w-5 h-5 text-emerald-400 shrink-0 mt-0.5" />
          ) : (
            <AlertTriangle className="w-5 h-5 text-red-400 shrink-0 mt-0.5" />
          )}
          <div className="flex-1 text-xs font-mono">
            <strong className="block uppercase tracking-wider font-bold mb-1">
              {toast.type === 'success' ? 'PERÍMETRO PUBLICADO A MQTT' : 'ERROR EN PUBLICACIÓN MQTT'}
            </strong>
            <p className="text-gray-200">{toast.message}</p>
          </div>
          <button onClick={() => setToast(null)} className="text-gray-400 hover:text-white">
            <X className="w-4 h-4" />
          </button>
        </div>
      )}

      {/* Header Industrial de Telemetría */}
      <Header
        fenceStatus={fenceStatus}
        deviceId={deviceId}
        isSimulated={isSimulated}
        onRefresh={refetch}
        viewMode={viewMode}
        onToggleViewMode={setViewMode}
        selectedDevice={selectedDevice}
        availableDevices={availableDevices}
        onSelectDevice={setSelectedDevice}
      />

      {/* Main Command Console Space */}
      <main className="flex-1 max-w-[1700px] w-full mx-auto px-4 sm:px-6 lg:px-8 py-6 space-y-6">
        {/* Loading Indicator bar */}
        {isLoading && (
          <div className="flex items-center gap-2 text-xs font-mono text-cyan-400 bg-cyan-500/10 border border-cyan-500/20 px-4 py-2 rounded-lg">
            <Loader2 className="w-4 h-4 animate-spin" />
            Sincronizando flujo de telemetría e historial desde InfluxDB...
          </div>
        )}

        {/* Row 1: KPI HUD Cards */}
        <section className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4">
          <MetricCard
            title="ENERGÍA DISPONIBLE"
            value={battery}
            unit="%"
            subtitle="ESP32-CAM LiPo Status"
            icon={<Battery className="w-5 h-5" />}
            accentColor={battery < 20 ? 'red' : 'amber'}
          />
          <MetricCard
            title="POTENCIA SEÑAL RF"
            value={rssi}
            unit="dBm"
            subtitle="Gateway LoRa/Wi-Fi RSSI"
            icon={<Radio className="w-5 h-5" />}
            accentColor="cyan"
          />
          <MetricCard
            title="RELACIÓN SEÑAL/RUIDO"
            value={snr}
            unit="dB"
            subtitle="Modulación SNR"
            icon={<Signal className="w-5 h-5" />}
            accentColor="emerald"
          />
          <MetricCard
            title="CICLOS DE TRANSMISIÓN"
            value={wakeCount}
            unit="wakes"
            subtitle="Deep Sleep Wake Counter"
            icon={<Activity className="w-5 h-5" />}
            accentColor="emerald"
          />
        </section>

        {/* Row 2: Geofence Map Console & Precision Vertex Controller */}
        <section className="grid grid-cols-1 lg:grid-cols-3 gap-6">
          {/* Columna Izquierda/Central: Mapa IoT */}
          <div className="lg:col-span-2 h-[580px] lg:h-[660px]">
            <GeofenceMap
              viewMode={viewMode}
              selectedDevice={selectedDevice}
              latestPointsByDevice={latestPointsByDevice}
              trailsByDevice={trailsByDevice}
              latitude={lat}
              longitude={lon}
              fenceStatus={fenceStatus}
              deviceId={deviceId}
              polygonCoords={polygonCoords}
              selectedVertexIndex={selectedVertexIndex}
              onSelectVertex={setSelectedVertexIndex}
              onCoordsChange={setPolygonCoords}
              showBoundingBox={showBoundingBox}
              onToggleBoundingBox={() => setShowBoundingBox(!showBoundingBox)}
            />
          </div>

          {/* Columna Derecha: Panel Táctico de Diagnóstico & Control Preciso de Vértices */}
          <div className="bg-hud-surface border border-hud-border rounded-xl p-5 flex flex-col justify-between space-y-6">
            {/* PARTE SUPERIOR: Coordenadas y Estado del Nodo */}
            <div>
              <div className="flex items-center justify-between pb-3 border-b border-hud-border mb-4">
                <span className="text-xs font-mono font-bold tracking-wider text-gray-300 uppercase flex items-center gap-2">
                  <Navigation className="w-4 h-4 text-emerald-400" />
                  COORDENADAS Y ESTADO
                </span>
                <span className="text-[10px] font-mono px-2 py-0.5 rounded bg-hud-card text-gray-400 border border-hud-border">
                  GRCh38 / WGS84
                </span>
              </div>

              <div className="space-y-3 text-sm font-mono">
                <div className="flex justify-between items-center bg-hud-card p-2.5 rounded-lg border border-hud-border">
                  <span className="text-gray-400 text-xs">LATITUD ACTUAL</span>
                  <span className="text-white font-bold">{lat.toFixed(6)}</span>
                </div>
                <div className="flex justify-between items-center bg-hud-card p-2.5 rounded-lg border border-hud-border">
                  <span className="text-gray-400 text-xs">LONGITUD ACTUAL</span>
                  <span className="text-white font-bold">{lon.toFixed(6)}</span>
                </div>
                <div className="flex justify-between items-center bg-hud-card p-2.5 rounded-lg border border-hud-border">
                  <span className="text-gray-400 text-xs">PERÍMETRO GEOCERCA</span>
                  <span
                    className={`font-bold ${
                      fenceStatus === 'OUTSIDE' ? 'text-red-400 animate-pulse' : 'text-emerald-400'
                    }`}
                  >
                    {fenceStatus === 'OUTSIDE' ? 'ALERTA FUERA DEL PREDIO' : 'SEGURO EN EL PREDIO'}
                  </span>
                </div>
                <div className="flex justify-between items-center bg-hud-card p-2.5 rounded-lg border border-hud-border">
                  <span className="text-gray-400 text-xs">ÚLTIMO REGISTRO</span>
                  <span className="text-gray-300 text-xs">
                    {latestPoint ? new Date(latestPoint.time).toLocaleTimeString() : 'N/A'}
                  </span>
                </div>
              </div>
            </div>

            {/* PARTE INFERIOR: CONTROL DE PERÍMETRO POR VÉRTICES (TECLADO / FLECHAS) */}
            <div className="pt-4 border-t border-hud-border flex-1 flex flex-col justify-between">
              <div>
                <div className="flex items-center justify-between mb-2">
                  <span className="text-xs font-mono font-bold tracking-wider text-amber-400 uppercase flex items-center gap-1.5">
                    <Keyboard className="w-4 h-4 text-amber-400" />
                    VÉRTICES DEL CORRAL (4 FIJOS)
                  </span>
                  <span className="text-[10px] font-mono text-gray-400 bg-hud-surface border border-hud-border px-2 py-0.5 rounded">
                    FIJOS
                  </span>
                </div>

                {/* Instrucción de Teclado */}
                <p className="text-[10px] font-mono text-gray-400 mb-2 leading-tight">
                  Haz clic en un vértice y usa las <strong className="text-cyan-300">flechas del teclado (↑ ↓ ← →)</strong> para moverlo en tiempo real.
                </p>

                {/* Lista interactiva de vértices */}
                <div className="grid grid-cols-2 gap-2 max-h-[160px] overflow-y-auto pr-1">
                  {polygonCoords.map((coord, idx) => {
                    const isSelected = selectedVertexIndex === idx;
                    return (
                      <button
                        key={`vertex-card-${idx}`}
                        onClick={() => setSelectedVertexIndex(idx)}
                        className={`text-left p-2 rounded-lg border font-mono transition-all ${
                          isSelected
                            ? 'bg-cyan-500/20 border-cyan-400 text-white shadow-md ring-1 ring-cyan-400/50'
                            : 'bg-hud-card hover:bg-hud-border border-hud-border text-gray-300'
                        }`}
                      >
                        <div className="flex items-center justify-between mb-0.5">
                          <span
                            className={`text-[11px] font-bold px-1.5 py-0.2 rounded ${
                              isSelected
                                ? 'bg-cyan-400 text-black'
                                : 'bg-hud-border text-gray-300'
                            }`}
                          >
                            V{idx + 1}
                          </span>
                          {isSelected && (
                            <span className="text-[9px] text-cyan-300 font-bold uppercase tracking-wider">
                              ACTIVO
                            </span>
                          )}
                        </div>
                        <div className="text-[10px] text-gray-300 truncate">
                          La: {coord[0].toFixed(5)}
                        </div>
                        <div className="text-[10px] text-gray-300 truncate">
                          Lo: {coord[1].toFixed(5)}
                        </div>
                      </button>
                    );
                  })}
                </div>

                {/* D-Pad de Ajuste Táctil para el vértice seleccionado */}
                {selectedVertexIndex !== null && polygonCoords[selectedVertexIndex] && (
                  <div className="mt-3 bg-hud-card border border-hud-border rounded-lg p-2 flex items-center justify-between text-xs font-mono">
                    <span className="text-[10px] text-gray-400">
                      AJUSTAR <strong className="text-cyan-300">V{selectedVertexIndex + 1}</strong>:
                    </span>
                    <div className="flex items-center gap-1">
                      <button
                        onClick={() => moveVertex(selectedVertexIndex, 0.00005, 0)}
                        className="p-1 rounded bg-hud-surface hover:bg-hud-border border border-hud-border text-gray-200"
                        title="Norte (↑)"
                      >
                        <ArrowUp className="w-3.5 h-3.5" />
                      </button>
                      <button
                        onClick={() => moveVertex(selectedVertexIndex, -0.00005, 0)}
                        className="p-1 rounded bg-hud-surface hover:bg-hud-border border border-hud-border text-gray-200"
                        title="Sur (↓)"
                      >
                        <ArrowDown className="w-3.5 h-3.5" />
                      </button>
                      <button
                        onClick={() => moveVertex(selectedVertexIndex, 0, -0.00005)}
                        className="p-1 rounded bg-hud-surface hover:bg-hud-border border border-hud-border text-gray-200"
                        title="Oeste (←)"
                      >
                        <ArrowLeft className="w-3.5 h-3.5" />
                      </button>
                      <button
                        onClick={() => moveVertex(selectedVertexIndex, 0, 0.00005)}
                        className="p-1 rounded bg-hud-surface hover:bg-hud-border border border-hud-border text-gray-200"
                        title="Este (→)"
                      >
                        <ArrowRight className="w-3.5 h-3.5" />
                      </button>
                    </div>
                  </div>
                )}
              </div>

              {/* Botones de Acción Final: GUARDAR EN MQTT y RESET */}
              <div className="mt-4 pt-3 border-t border-hud-border flex items-center gap-2">
                <button
                  onClick={handleResetFence}
                  disabled={isSaving}
                  className="px-3 py-2 rounded text-xs font-mono border border-hud-border bg-hud-card hover:bg-hud-border text-gray-300 hover:text-white transition-colors flex items-center gap-1.5"
                  title="Restaurar coordenadas iniciales"
                >
                  <RotateCcw className="w-3.5 h-3.5" />
                  RESET
                </button>

                <button
                  onClick={handleSaveFence}
                  disabled={isSaving}
                  className="flex-1 px-4 py-2 rounded text-xs font-mono font-bold tracking-wider transition-all flex items-center justify-center gap-1.5 bg-emerald-500 hover:bg-emerald-400 text-black shadow-neon-green disabled:opacity-50"
                >
                  {isSaving ? (
                    <>
                      <Loader2 className="w-4 h-4 animate-spin text-black" />
                      PUBLICANDO...
                    </>
                  ) : (
                    <>
                      <Save className="w-4 h-4 text-black" />
                      GUARDAR Y SUBIR AL BROKER MQTT
                    </>
                  )}
                </button>
              </div>
            </div>
          </div>
        </section>

        {/* Row 3: Historical Telemetry Charts */}
        <section>
          <TelemetryCharts history={history} />
        </section>
      </main>

      {/* Footer */}
      <footer className="w-full bg-hud-surface border-t border-hud-border px-6 py-4 mt-8 flex flex-col sm:flex-row items-center justify-between text-xs font-mono text-gray-400 gap-2">
        <div>
          SISTEMA DE GEOFENCING GANADERO IOT // UNLP PROYECTO FINAL
        </div>
        <div>
          DOCKER MICROSERVICES ARCHITECTURE // ARCH LINUX EDGE HOST
        </div>
      </footer>
    </div>
  );
}
