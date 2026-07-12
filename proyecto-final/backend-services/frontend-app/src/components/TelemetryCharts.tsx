import React from 'react';
import {
  ResponsiveContainer,
  LineChart,
  Line,
  XAxis,
  YAxis,
  Tooltip,
  CartesianGrid,
  Legend
} from 'recharts';
import type { TelemetryHistoryPoint } from '../types/telemetry';
import { BatteryCharging, Radio } from 'lucide-react';

interface TelemetryChartsProps {
  history: TelemetryHistoryPoint[];
}

export const TelemetryCharts: React.FC<TelemetryChartsProps> = ({ history }) => {
  return (
    <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
      {/* Battery Voltage / Discharge Historical Panel */}
      <div className="bg-hud-surface border border-hud-border rounded-xl p-5">
        <div className="flex items-center justify-between mb-4">
          <div className="flex items-center gap-2">
            <BatteryCharging className="w-4 h-4 text-amber-400" />
            <h3 className="text-sm font-mono font-bold text-white tracking-wide uppercase">
              BATERÍA DEL NODO EDGE (%)
            </h3>
          </div>
          <span className="text-xs font-mono text-gray-400">ÚLTIMAS 30 LECTURAS</span>
        </div>

        <div className="h-56 w-full">
          <ResponsiveContainer width="100%" height="100%">
            <LineChart data={history}>
              <CartesianGrid stroke="#1E283D" strokeDasharray="3 3" vertical={false} />
              <XAxis
                dataKey="timeFormatted"
                stroke="#64748b"
                tick={{ fill: '#94a3b8', fontSize: 10, fontFamily: 'JetBrains Mono' }}
              />
              <YAxis
                domain={[0, 100]}
                stroke="#64748b"
                tick={{ fill: '#94a3b8', fontSize: 10, fontFamily: 'JetBrains Mono' }}
              />
              <Tooltip
                contentStyle={{
                  backgroundColor: '#111622',
                  borderColor: '#1E283D',
                  fontFamily: 'JetBrains Mono',
                  fontSize: '12px'
                }}
              />
              <Line
                type="monotone"
                dataKey="battery"
                name="Batería (%)"
                stroke="#F59E0B"
                strokeWidth={2.5}
                dot={false}
                activeDot={{ r: 5, fill: '#F59E0B', stroke: '#fff' }}
              />
            </LineChart>
          </ResponsiveContainer>
        </div>
      </div>

      {/* RF Signal Quality (RSSI / SNR) Historical Panel */}
      <div className="bg-hud-surface border border-hud-border rounded-xl p-5">
        <div className="flex items-center justify-between mb-4">
          <div className="flex items-center gap-2">
            <Radio className="w-4 h-4 text-cyan-400" />
            <h3 className="text-sm font-mono font-bold text-white tracking-wide uppercase">
              SEÑAL RF LORA/WIFI (RSSI & SNR)
            </h3>
          </div>
          <span className="text-xs font-mono text-gray-400">CALIDAD DEL ENLACE</span>
        </div>

        <div className="h-56 w-full">
          <ResponsiveContainer width="100%" height="100%">
            <LineChart data={history}>
              <CartesianGrid stroke="#1E283D" strokeDasharray="3 3" vertical={false} />
              <XAxis
                dataKey="timeFormatted"
                stroke="#64748b"
                tick={{ fill: '#94a3b8', fontSize: 10, fontFamily: 'JetBrains Mono' }}
              />
              <YAxis
                stroke="#64748b"
                tick={{ fill: '#94a3b8', fontSize: 10, fontFamily: 'JetBrains Mono' }}
              />
              <Tooltip
                contentStyle={{
                  backgroundColor: '#111622',
                  borderColor: '#1E283D',
                  fontFamily: 'JetBrains Mono',
                  fontSize: '12px'
                }}
              />
              <Legend
                wrapperStyle={{
                  fontSize: '11px',
                  fontFamily: 'JetBrains Mono',
                  color: '#94a3b8'
                }}
              />
              <Line
                type="monotone"
                dataKey="rssi"
                name="RSSI (dBm)"
                stroke="#06B6D4"
                strokeWidth={2}
                dot={false}
              />
              <Line
                type="monotone"
                dataKey="snr"
                name="SNR (dB)"
                stroke="#10B981"
                strokeWidth={2}
                dot={false}
              />
            </LineChart>
          </ResponsiveContainer>
        </div>
      </div>
    </div>
  );
};
