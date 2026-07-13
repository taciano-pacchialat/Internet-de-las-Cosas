import React, { useState, useEffect } from 'react';
import { RefreshCw, AlertTriangle, CheckCircle2, Radio, Route, Filter } from 'lucide-react';

interface HeaderProps {
  fenceStatus: 'INSIDE' | 'OUTSIDE' | 'UNKNOWN';
  deviceId: string;
  isSimulated: boolean;
  onRefresh: () => void;
  viewMode: 'Live' | 'Trail';
  onToggleViewMode: (mode: 'Live' | 'Trail') => void;
  selectedDevice: string;
  availableDevices: string[];
  onSelectDevice: (deviceId: string) => void;
}

export const Header: React.FC<HeaderProps> = ({
  fenceStatus,
  isSimulated,
  onRefresh,
  viewMode,
  onToggleViewMode,
  selectedDevice,
  availableDevices,
  onSelectDevice
}) => {
  const [timeStr, setTimeStr] = useState('');

  useEffect(() => {
    const updateTime = () => {
      setTimeStr(
        new Date().toLocaleTimeString('es-AR', {
          hour: '2-digit',
          minute: '2-digit',
          second: '2-digit'
        })
      );
    };
    updateTime();
    const interval = setInterval(updateTime, 1000);
    return () => clearInterval(interval);
  }, []);

  const isAlert = fenceStatus === 'OUTSIDE';

  return (
    <header className="w-full bg-gray-800 border-b border-gray-700 px-6 py-4 flex flex-col xl:flex-row items-start xl:items-center justify-between gap-4 shadow-lg">
      {/* Brand & Industrial Title */}
      <div className="flex items-center gap-4">
        <div className={`w-3 h-3 rounded-full ${
          isAlert ? 'bg-red-500 animate-pulse shadow-red-500/50' : 'bg-emerald-500'
        }`} />
        <div>
          <div className="flex items-center gap-2">
            <span className="text-[11px] font-mono tracking-widest text-gray-400 uppercase">
              INDUSTRIAL TELEMETRY GATEWAY // ESP32-LORA
            </span>
            {isSimulated && (
              <span className="text-[10px] font-mono px-2 py-0.5 rounded bg-amber-500/20 text-amber-300 border border-amber-500/40">
                SIMULADO
              </span>
            )}
          </div>
          <div className="flex items-center gap-3">
            <h1 className="text-xl font-bold text-gray-100 tracking-tight">
              CATTLE IOT // GEOFENCE CONSOLE
            </h1>
          </div>
        </div>
      </div>

      {/* Mode Switcher & Entity Selector */}
      <div className="flex flex-wrap items-center gap-4">
        {/* Toggle Switch Tabs: Live vs Trail */}
        <div className="flex items-center bg-gray-900 p-1 rounded-lg border border-gray-700">
          <button
            onClick={() => onToggleViewMode('Live')}
            className={`flex items-center gap-2 px-4 py-2 rounded-md text-xs font-semibold tracking-wide transition-all ${
              viewMode === 'Live'
                ? 'bg-cyan-600 text-white shadow-sm'
                : 'text-gray-400 hover:text-gray-200'
            }`}
          >
            <Radio className="w-4 h-4" />
            EN VIVO (LIVE)
          </button>
          <button
            onClick={() => onToggleViewMode('Trail')}
            className={`flex items-center gap-2 px-4 py-2 rounded-md text-xs font-semibold tracking-wide transition-all ${
              viewMode === 'Trail'
                ? 'bg-cyan-600 text-white shadow-sm'
                : 'text-gray-400 hover:text-gray-200'
            }`}
          >
            <Route className="w-4 h-4" />
            RUTAS (TRAIL)
          </button>
        </div>

        {/* Selector de Entidades (Solo visible o resaltado en modo Trail) */}
        {viewMode === 'Trail' && (
          <div className="flex items-center gap-2 bg-gray-900 px-3 py-1.5 rounded-lg border border-gray-700">
            <Filter className="w-4 h-4 text-cyan-400" />
            <span className="text-xs font-mono text-gray-400 uppercase">DISPOSITIVO:</span>
            <select
              value={selectedDevice}
              onChange={(e) => onSelectDevice(e.target.value)}
              className="bg-gray-800 border border-gray-600 rounded px-2.5 py-1 text-xs font-mono text-gray-200 focus:outline-none focus:border-cyan-500 cursor-pointer"
            >
              <option value="ALL">TODAS LAS VACAS (ALL)</option>
              {availableDevices.map((dev) => (
                <option key={dev} value={dev}>
                  {dev}
                </option>
              ))}
            </select>
          </div>
        )}
      </div>

      {/* Status & Controls */}
      <div className="flex items-center gap-4 w-full xl:w-auto justify-between xl:justify-end">
        {/* Geofence Status Badge */}
        <div
          className={`px-3.5 py-2 rounded-md border flex items-center gap-2.5 transition-all ${
            isAlert
              ? 'bg-red-900/40 border-red-500/70 text-red-200'
              : 'bg-emerald-900/40 border-emerald-500/60 text-emerald-200'
          }`}
        >
          {isAlert ? (
            <AlertTriangle className="w-4 h-4 text-red-400" />
          ) : (
            <CheckCircle2 className="w-4 h-4 text-emerald-400" />
          )}
          <div className="flex flex-col">
            <span className="text-[10px] font-mono uppercase text-gray-400">
              ESTADO DEL PERÍMETRO
            </span>
            <span className="text-xs font-mono font-bold">
              {isAlert ? 'ALERTA: FUERA DEL FENCE' : 'SEGURO EN PERÍMETRO'}
            </span>
          </div>
        </div>

        {/* Clock */}
        <div className="hidden lg:flex flex-col items-end border-l border-gray-700 pl-4 font-mono">
          <span className="text-xs text-emerald-400">STATUS: OK</span>
          <span className="text-xs text-gray-400">{timeStr || '00:00:00'}</span>
        </div>

        {/* Manual Refresh Button */}
        <button
          onClick={onRefresh}
          className="p-2 rounded-md bg-gray-700 hover:bg-gray-600 border border-gray-600 text-gray-200 transition-colors flex items-center justify-center"
          title="Actualizar datos"
        >
          <RefreshCw className="w-4 h-4" />
        </button>
      </div>
    </header>
  );
};
