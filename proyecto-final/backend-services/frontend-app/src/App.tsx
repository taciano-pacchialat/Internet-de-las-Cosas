import { useCattleTelemetry } from './hooks/useCattleTelemetry';
import { Header } from './components/Header';
import { MetricCard } from './components/MetricCard';
import { GeofenceMap } from './components/GeofenceMap';
import { TelemetryCharts } from './components/TelemetryCharts';
import {
  Battery,
  Radio,
  Signal,
  Activity,
  Navigation,
  Database,
  Loader2
} from 'lucide-react';

export default function App() {
  const {
    latestPoint,
    history,
    isLoading,
    isSimulated,
    refetch
  } = useCattleTelemetry(3000);

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
    <div className="min-h-screen bg-hud-bg flex flex-col justify-between selection:bg-emerald-500 selection:text-black">
      {/* Top Tactical Header */}
      <Header
        fenceStatus={fenceStatus}
        deviceId={deviceId}
        isSimulated={isSimulated}
        onRefresh={refetch}
      />

      {/* Main Command Console Space */}
      <main className="flex-1 max-w-[1700px] w-full mx-auto px-4 sm:px-6 lg:px-8 py-6 space-y-6">
        {/* Loading Indicator bar */}
        {isLoading && (
          <div className="flex items-center gap-2 text-xs font-mono text-cyan-400 bg-cyan-500/10 border border-cyan-500/20 px-4 py-2 rounded-lg">
            <Loader2 className="w-4 h-4 animate-spin" />
            Sincronizando flujo de telemetría desde InfluxDB...
          </div>
        )}

        {/* Row 1: KPI HUD Cards (CSS Grid) */}
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

        {/* Row 2: Live Geofence Map Console */}
        <section className="grid grid-cols-1 lg:grid-cols-3 gap-6">
          <div className="lg:col-span-2">
            <GeofenceMap
              latitude={lat}
              longitude={lon}
              fenceStatus={fenceStatus}
              deviceId={deviceId}
            />
          </div>

          {/* Quick Diagnostics Panel / Log Feed */}
          <div className="bg-hud-surface border border-hud-border rounded-xl p-5 flex flex-col justify-between">
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

              <div className="space-y-4 text-sm font-mono">
                <div className="flex justify-between items-center bg-hud-card p-3 rounded-lg border border-hud-border">
                  <span className="text-gray-400 text-xs">LATITUD ACTUAL</span>
                  <span className="text-white font-bold">{lat.toFixed(6)}</span>
                </div>
                <div className="flex justify-between items-center bg-hud-card p-3 rounded-lg border border-hud-border">
                  <span className="text-gray-400 text-xs">LONGITUD ACTUAL</span>
                  <span className="text-white font-bold">{lon.toFixed(6)}</span>
                </div>
                <div className="flex justify-between items-center bg-hud-card p-3 rounded-lg border border-hud-border">
                  <span className="text-gray-400 text-xs">PERÍMETRO GEOCERCA</span>
                  <span
                    className={`font-bold ${
                      fenceStatus === 'OUTSIDE' ? 'text-red-400 animate-pulse' : 'text-emerald-400'
                    }`}
                  >
                    {fenceStatus === 'OUTSIDE' ? 'ALERTA FUERA DEL PREDIO' : 'SEGURO EN EL PREDIO'}
                  </span>
                </div>
                <div className="flex justify-between items-center bg-hud-card p-3 rounded-lg border border-hud-border">
                  <span className="text-gray-400 text-xs">ÚLTIMO REGISTRO</span>
                  <span className="text-gray-300 text-xs">
                    {latestPoint ? new Date(latestPoint.time).toLocaleTimeString() : 'N/A'}
                  </span>
                </div>
              </div>
            </div>

            {/* InfluxDB Integration Badge */}
            <div className="mt-6 pt-4 border-t border-hud-border flex items-center justify-between text-xs font-mono text-gray-400">
              <span className="flex items-center gap-2">
                <Database className="w-3.5 h-3.5 text-cyan-400" />
                TSDB: InfluxDB 1.8
              </span>
              <span>MEASUREMENT: cattle_position</span>
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
