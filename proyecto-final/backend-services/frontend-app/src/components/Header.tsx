import React, { useState, useEffect } from 'react';
import { Shield, RefreshCw, AlertTriangle, CheckCircle2 } from 'lucide-react';

interface HeaderProps {
  fenceStatus: 'INSIDE' | 'OUTSIDE' | 'UNKNOWN';
  deviceId: string;
  isSimulated: boolean;
  onRefresh: () => void;
}

export const Header: React.FC<HeaderProps> = ({
  fenceStatus,
  deviceId,
  isSimulated,
  onRefresh
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
    <header className="w-full bg-hud-surface border-b border-hud-border px-6 py-4 flex flex-col md:flex-row items-start md:items-center justify-between gap-4">
      {/* Brand & HUD Identity */}
      <div className="flex items-center gap-4">
        <div className="p-2.5 rounded-lg bg-hud-card border border-hud-border flex items-center justify-center">
          <Shield className="w-6 h-6 text-emerald-400" />
        </div>
        <div>
          <div className="flex items-center gap-2">
            <span className="text-xs font-mono tracking-widest text-gray-400 uppercase">
              TACTICAL TELEMETRY SYS // V2.4
            </span>
            {isSimulated && (
              <span className="text-[10px] font-mono px-2 py-0.5 rounded bg-amber-500/10 text-amber-400 border border-amber-500/30">
                DEV MOCK
              </span>
            )}
          </div>
          <h1 className="text-xl font-bold tracking-tight text-white flex items-center gap-2">
            GEOFENCE COMMAND CENTER
            <span className="text-xs font-mono font-normal text-gray-400">
              [{deviceId}]
            </span>
          </h1>
        </div>
      </div>

      {/* Primary Fence Alarm Indicator & Controls */}
      <div className="flex items-center gap-4 w-full md:w-auto justify-between md:justify-end">
        {/* Geofence Status Badge */}
        <div
          className={`px-4 py-2 rounded-lg border flex items-center gap-3 transition-all duration-300 ${
            isAlert
              ? 'bg-red-500/10 border-red-500/60 text-red-400 shadow-neon-red animate-pulse'
              : 'bg-emerald-500/10 border-emerald-500/40 text-emerald-400 shadow-neon-green'
          }`}
        >
          {isAlert ? (
            <AlertTriangle className="w-5 h-5 text-red-400" />
          ) : (
            <CheckCircle2 className="w-5 h-5 text-emerald-400" />
          )}
          <div className="flex flex-col">
            <span className="text-[10px] font-mono tracking-wider uppercase text-gray-400">
              FENCE PERIMETER
            </span>
            <span className="text-sm font-mono font-bold tracking-wide">
              {isAlert ? 'BREACH DETECTED (OUTSIDE)' : 'SECURE (INSIDE)'}
            </span>
          </div>
        </div>

        {/* Live Clock & Connection indicator */}
        <div className="hidden lg:flex flex-col items-end border-l border-hud-border pl-4">
          <div className="flex items-center gap-2">
            <span className="w-2 h-2 rounded-full bg-emerald-400 animate-pulse"></span>
            <span className="text-xs font-mono text-gray-300">LINK ACTIVE</span>
          </div>
          <span className="text-sm font-mono text-gray-400">{timeStr || '00:00:00'}</span>
        </div>

        {/* Manual Refresh Button */}
        <button
          onClick={onRefresh}
          className="p-2.5 rounded-lg bg-hud-card hover:bg-hud-border border border-hud-border text-gray-300 hover:text-white transition-colors flex items-center justify-center"
          title="Sincronizar telemetría"
        >
          <RefreshCw className="w-4 h-4" />
        </button>
      </div>
    </header>
  );
};
