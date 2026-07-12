import React from 'react';

interface MetricCardProps {
  title: string;
  value: string | number;
  unit?: string;
  subtitle?: string;
  icon: React.ReactNode;
  accentColor?: 'emerald' | 'red' | 'amber' | 'cyan';
}

export const MetricCard: React.FC<MetricCardProps> = ({
  title,
  value,
  unit,
  subtitle,
  icon,
  accentColor = 'emerald'
}) => {
  const colorClasses = {
    emerald: 'text-emerald-400 border-emerald-500/20 bg-emerald-500/5',
    red: 'text-red-400 border-red-500/20 bg-red-500/5',
    amber: 'text-amber-400 border-amber-500/20 bg-amber-500/5',
    cyan: 'text-cyan-400 border-cyan-500/20 bg-cyan-500/5'
  };

  return (
    <div className="bg-hud-surface border border-hud-border rounded-xl p-5 flex flex-col justify-between hover:border-gray-600 transition-all duration-300 relative overflow-hidden group">
      {/* Top row */}
      <div className="flex items-center justify-between mb-3">
        <span className="text-xs font-mono uppercase tracking-wider text-gray-400">
          {title}
        </span>
        <div className={`p-2 rounded-lg border ${colorClasses[accentColor]}`}>
          {icon}
        </div>
      </div>

      {/* Main value readout */}
      <div className="flex items-baseline gap-1.5">
        <span className="text-3xl font-mono font-bold tracking-tight text-white">
          {value}
        </span>
        {unit && (
          <span className="text-sm font-mono text-gray-400 font-medium">
            {unit}
          </span>
        )}
      </div>

      {/* Footer / subtitle */}
      {subtitle && (
        <span className="text-xs font-mono text-gray-400 mt-2 block">
          {subtitle}
        </span>
      )}

      {/* Subtle bottom accent bar */}
      <div
        className={`absolute bottom-0 left-0 right-0 h-0.5 opacity-60 group-hover:opacity-100 transition-opacity ${
          accentColor === 'emerald'
            ? 'bg-emerald-400'
            : accentColor === 'red'
            ? 'bg-red-400'
            : accentColor === 'amber'
            ? 'bg-amber-400'
            : 'bg-cyan-400'
        }`}
      />
    </div>
  );
};
