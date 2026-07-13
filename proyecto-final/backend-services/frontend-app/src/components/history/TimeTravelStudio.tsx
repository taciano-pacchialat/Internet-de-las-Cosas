import React, { useState, useEffect } from 'react';
import { History, Play, Pause, Rewind, FastForward, Calendar, Clock, Database, Radio } from 'lucide-react';

export interface HistoricalTelemetryPoint {
  time: number | string;
  lat: number;
  lon: number;
  rssi?: number;
  snr?: number;
  fence_status?: string;
  wake_count?: number;
}

export interface HistoricalDataState {
  timestamp: number;
  cattleTrail: HistoricalTelemetryPoint[];
  activeFence: { lat: number; lon: number }[] | null;
  currentPoint: HistoricalTelemetryPoint | null;
  loading: boolean;
}

interface TimeTravelStudioProps {
  onHistoricalDataChange: (data: HistoricalDataState) => void;
}

export const TimeTravelStudio: React.FC<TimeTravelStudioProps> = ({ onHistoricalDataChange }) => {
  const [selectedTimestamp, setSelectedTimestamp] = useState<number>(() => Math.floor(Date.now() / 1000));
  const [loading, setLoading] = useState<boolean>(false);
  const [isPlaying, setIsPlaying] = useState<boolean>(false);
  const [historyPoints, setHistoryPoints] = useState<HistoricalTelemetryPoint[]>([]);
  const [fenceAtTime, setFenceAtTime] = useState<{ lat: number; lon: number }[] | null>(null);

  // Convert Unix seconds to datetime-local input string YYYY-MM-DDTHH:mm
  const formatDateTimeLocal = (unixSec: number): string => {
    const d = new Date(unixSec * 1000);
    const pad = (n: number) => n.toString().padStart(2, '0');
    return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}T${pad(d.getHours())}:${pad(d.getMinutes())}`;
  };

  const [dateInputStr, setDateInputStr] = useState<string>(() => formatDateTimeLocal(selectedTimestamp));

  // Auto-play de cinta (avanza 1 minuto cada 2.5s en simulación analógica)
  useEffect(() => {
    if (!isPlaying) return;
    const interval = setInterval(() => {
      setSelectedTimestamp((prev) => {
        const next = prev + 60;
        setDateInputStr(formatDateTimeLocal(next));
        return next;
      });
    }, 2500);
    return () => clearInterval(interval);
  }, [isPlaying]);

  const fetchHistoricalSnapshot = async (tsSec: number) => {
    setLoading(true);
    try {
      const response = await fetch(`/api/history?timestamp=${tsSec}`);
      if (!response.ok) throw new Error('Error al obtener historial');
      const data = await response.json();

      const trail: HistoricalTelemetryPoint[] = Array.isArray(data.cattle) ? data.cattle : [];
      let fence: { lat: number; lon: number }[] | null = null;

      if (data.fence && data.fence.vertices) {
        fence = data.fence.vertices;
      } else if (data.fence && data.fence.polygon_json) {
        try {
          fence = JSON.parse(data.fence.polygon_json);
        } catch (e) {
          console.warn('No se pudo parsear polygon_json histórico');
        }
      }

      setHistoryPoints(trail);
      setFenceAtTime(fence);

      const latestPoint = trail.length > 0 ? trail[0] : null;

      onHistoricalDataChange({
        timestamp: tsSec,
        cattleTrail: trail,
        activeFence: fence,
        currentPoint: latestPoint,
        loading: false
      });
    } catch (err) {
      console.error('Error fetching history:', err);
      onHistoricalDataChange({
        timestamp: tsSec,
        cattleTrail: [],
        activeFence: null,
        currentPoint: null,
        loading: false
      });
    } finally {
      setLoading(false);
    }
  };

  // Trigger fetch when timestamp changes
  useEffect(() => {
    fetchHistoricalSnapshot(selectedTimestamp);
  }, [selectedTimestamp]);

  // Auto-play tape reel effect
  useEffect(() => {
    if (!isPlaying) return;
    const interval = setInterval(() => {
      setSelectedTimestamp(prev => {
        const next = prev - 60; // retrocede o avanza en cinta de 1 minuto
        setDateInputStr(formatDateTimeLocal(next));
        return next;
      });
    }, 1500);
    return () => clearInterval(interval);
  }, [isPlaying]);

  const handleDateInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const val = e.target.value;
    setDateInputStr(val);
    const ts = Math.floor(new Date(val).getTime() / 1000);
    if (!isNaN(ts)) {
      setSelectedTimestamp(ts);
    }
  };

  const jumpRelative = (deltaMinutes: number) => {
    const newTs = selectedTimestamp + deltaMinutes * 60;
    setSelectedTimestamp(newTs);
    setDateInputStr(formatDateTimeLocal(newTs));
  };

  return (
    <div className="w-full bg-[#1A1614] border-2 border-amber-800/50 rounded-xl p-5 shadow-[0_4px_25px_rgba(0,0,0,0.6)] text-amber-100 flex flex-col gap-4">
      {/* Tape Reel Header */}
      <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between border-b border-amber-900/40 pb-3 gap-2">
        <div className="flex items-center gap-3">
          <div className="p-2 rounded-lg bg-amber-950/80 border border-amber-700/60">
            <History className="w-5 h-5 text-amber-400" />
          </div>
          <div>
            <h2 className="font-fender text-2xl text-amber-400 tracking-wide">
              Tape Reel // Time-Travel Studio
            </h2>
            <p className="text-xs font-mono text-amber-200/60">
              EXPLORADOR HISTÓRICO SINCRONIZADO EN INFLUXDB (SERIES TEMPORALES)
            </p>
          </div>
        </div>

        <div className="flex items-center gap-2">
          <span className="text-xs font-mono px-3 py-1 rounded bg-black/60 border border-amber-900/60 text-amber-300 flex items-center gap-1.5">
            <Database className="w-3.5 h-3.5 text-amber-400" />
            INFLUXDB 1.8 READOUT
          </span>
          {loading && (
            <span className="text-xs font-mono text-amber-400 animate-pulse">
              [CARGANDO CINTA...]
            </span>
          )}
        </div>
      </div>

      {/* Analog Controls & Date-Time Selector */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-4 items-center bg-[#13100E] p-4 rounded-lg border border-amber-900/30">
        {/* Datetime Picker */}
        <div className="flex flex-col gap-1.5">
          <label className="text-[11px] font-mono text-amber-300 uppercase tracking-wider flex items-center gap-1.5">
            <Calendar className="w-3.5 h-3.5 text-amber-400" />
            FECHA Y HORA DE CINTA (EXACT INSTANT)
          </label>
          <input
            type="datetime-local"
            value={dateInputStr}
            onChange={handleDateInputChange}
            className="bg-[#1D1815] border border-amber-700/60 rounded px-3 py-2 text-sm font-mono text-amber-200 focus:outline-none focus:border-amber-400 shadow-inner"
          />
        </div>

        {/* Quick Jump & Tape Transport Buttons */}
        <div className="flex flex-col gap-1.5">
          <label className="text-[11px] font-mono text-amber-300 uppercase tracking-wider flex items-center justify-between">
            <span className="flex items-center gap-1.5">
              <Clock className="w-3.5 h-3.5 text-amber-400" />
              REPRODUCCIÓN CINTA
            </span>
            <span className="flex items-center gap-1 text-[10px] text-amber-400/80">
              <Radio className={`w-3 h-3 ${isPlaying ? 'text-emerald-400 animate-spin' : 'text-amber-500'}`} />
              {isPlaying ? 'PLAYING' : 'STOPPED'}
            </span>
          </label>
          <div className="grid grid-cols-5 gap-1.5">
            <button
              onClick={() => jumpRelative(-60)}
              title="Rebobinar 1 Hora"
              className="px-2 py-2 rounded bg-[#231E19] hover:bg-[#2F2822] border border-amber-800/60 text-xs font-mono text-amber-300 flex items-center justify-center transition-all"
            >
              <Rewind className="w-4 h-4" />
            </button>
            <button
              onClick={() => jumpRelative(-15)}
              className="px-1.5 py-2 rounded bg-[#231E19] hover:bg-[#2F2822] border border-amber-800/60 text-[11px] font-mono text-amber-300 transition-all text-center"
            >
              -15M
            </button>
            <button
              onClick={() => setIsPlaying(!isPlaying)}
              title={isPlaying ? 'Pausar Cinta' : 'Reproducir Cinta en Tiempo Real'}
              className={`px-2 py-2 rounded border text-xs font-mono font-bold flex items-center justify-center transition-all ${
                isPlaying
                  ? 'bg-amber-500 border-amber-400 text-black shadow-lg'
                  : 'bg-[#231E19] hover:bg-[#2F2822] border-amber-800/60 text-amber-300'
              }`}
            >
              {isPlaying ? <Pause className="w-4 h-4" /> : <Play className="w-4 h-4" />}
            </button>
            <button
              onClick={() => jumpRelative(15)}
              className="px-1.5 py-2 rounded bg-[#231E19] hover:bg-[#2F2822] border border-amber-800/60 text-[11px] font-mono text-amber-300 transition-all text-center"
            >
              +15M
            </button>
            <button
              onClick={() => jumpRelative(60)}
              title="Avanzar 1 Hora"
              className="px-2 py-2 rounded bg-[#231E19] hover:bg-[#2F2822] border border-amber-800/60 text-xs font-mono text-amber-300 flex items-center justify-center transition-all"
            >
              <FastForward className="w-4 h-4" />
            </button>
          </div>
        </div>

        {/* Analog Mixing Fader / Slider */}
        <div className="flex flex-col gap-1.5">
          <div className="flex justify-between text-[11px] font-mono text-amber-300 uppercase tracking-wider">
            <span>FADER TEMPORAL (-4 HS .. AHORA)</span>
            <span>{new Date(selectedTimestamp * 1000).toLocaleTimeString('es-AR')}</span>
          </div>
          <input
            type="range"
            min={Math.floor(Date.now() / 1000) - 14400}
            max={Math.floor(Date.now() / 1000)}
            value={selectedTimestamp}
            onChange={(e) => {
              const val = Number(e.target.value);
              setSelectedTimestamp(val);
              setDateInputStr(formatDateTimeLocal(val));
            }}
            className="w-full accent-amber-500 cursor-pointer h-2 bg-black rounded-lg border border-amber-900/60"
          />
          <div className="flex justify-between text-[10px] font-mono text-amber-400/50">
            <span>◄ HACE 4 HORAS</span>
            <span>TIEMPO REAL ►</span>
          </div>
        </div>
      </div>

      {/* Historical Telemetry Summary Panel */}
      <div className="grid grid-cols-2 sm:grid-cols-4 gap-3">
        <div className="p-3 rounded-lg bg-[#14110F] border border-amber-900/40 flex flex-col">
          <span className="text-[10px] font-mono text-amber-300/70">PUNTOS EN TRAYECTORIA</span>
          <span className="text-lg font-mono font-bold text-amber-400">
            {historyPoints.length} REGISTROS
          </span>
        </div>

        <div className="p-3 rounded-lg bg-[#14110F] border border-amber-900/40 flex flex-col">
          <span className="text-[10px] font-mono text-amber-300/70">PERÍMETRO HISTÓRICO</span>
          <span className="text-lg font-mono font-bold text-emerald-400">
            {fenceAtTime ? `${fenceAtTime.length} VÉRTICES (ACTIVO)` : 'SIN FENCE REGISTRADO'}
          </span>
        </div>

        <div className="p-3 rounded-lg bg-[#14110F] border border-amber-900/40 flex flex-col">
          <span className="text-[10px] font-mono text-amber-300/70">RSSI / SNR HISTÓRICO</span>
          <span className="text-lg font-mono font-bold text-amber-200">
            {historyPoints[0]?.rssi ? `${historyPoints[0].rssi} dBm` : 'N/A'}
          </span>
        </div>

        <div className="p-3 rounded-lg bg-[#14110F] border border-amber-900/40 flex flex-col">
          <span className="text-[10px] font-mono text-amber-300/70">ESTADO HISTÓRICO</span>
          <span className={`text-lg font-mono font-bold ${
            historyPoints[0]?.fence_status === 'OUTSIDE' ? 'text-red-400' : 'text-emerald-400'
          }`}>
            {historyPoints[0]?.fence_status || 'DESCONOCIDO'}
          </span>
        </div>
      </div>
    </div>
  );
};
