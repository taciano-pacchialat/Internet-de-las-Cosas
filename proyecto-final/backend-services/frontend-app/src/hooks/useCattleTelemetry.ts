import { useState, useEffect, useCallback } from 'react';
import type { CattlePositionPoint, TelemetryHistoryPoint } from '../types/telemetry';

export function useCattleTelemetry(refreshIntervalMs = 3000) {
  const [latestPoint, setLatestPoint] = useState<CattlePositionPoint | null>(null);
  const [history, setHistory] = useState<TelemetryHistoryPoint[]>([]);
  const [isLoading, setIsLoading] = useState<boolean>(true);
  const [error, setError] = useState<string | null>(null);
  const [isSimulated, setIsSimulated] = useState<boolean>(false);

  const fetchTelemetry = useCallback(async () => {
    try {
      const query = `SELECT * FROM "cattle_position" ORDER BY time DESC LIMIT 30`;
      const url = `/api/influx/query?db=geofence_db&q=${encodeURIComponent(query)}`;
      
      const res = await fetch(url);
      if (!res.ok) {
        throw new Error(`HTTP ${res.status}: ${res.statusText}`);
      }

      const data = await res.json();
      const series = data?.results?.[0]?.series?.[0];

      if (series && series.values && series.values.length > 0) {
        const columns: string[] = series.columns;
        const idx = (col: string) => columns.indexOf(col);

        const points: CattlePositionPoint[] = series.values.map((row: any[]) => ({
          time: row[idx('time')] || new Date().toISOString(),
          device_id: String(row[idx('device_id')] || 'vaca-01'),
          latitude: Number(row[idx('latitude')]) || -34.9205,
          longitude: Number(row[idx('longitude')]) || -57.9536,
          rssi: Number(row[idx('rssi')]) || -80,
          snr: Number(row[idx('snr')]) || 9.5,
          battery: Number(row[idx('battery')]) || 95,
          wake_count: Number(row[idx('wake_count')]) || 1,
          fence_status: (row[idx('fence_status')] as any) || 'INSIDE'
        }));

        const latest = points[0];
        setLatestPoint(latest);

        const historyPoints: TelemetryHistoryPoint[] = points
          .slice()
          .reverse()
          .map((p) => {
            const date = new Date(p.time);
            const timeFormatted = date.toLocaleTimeString('es-AR', {
              hour: '2-digit',
              minute: '2-digit',
              second: '2-digit'
            });
            return {
              timeFormatted,
              battery: Math.round(p.battery * 10) / 10,
              rssi: Math.round(p.rssi),
              snr: Math.round(p.snr * 10) / 10,
              wake_count: p.wake_count
            };
          });

        setHistory(historyPoints);
        setIsSimulated(false);
        setError(null);
      } else {
        if (!latestPoint) {
          generateSimulatedPoint();
        }
      }
    } catch (err: any) {
      setError(err.message || 'Error fetching InfluxDB telemetry');
      if (!latestPoint) {
        generateSimulatedPoint();
      }
    } finally {
      setIsLoading(false);
    }
  }, [latestPoint]);

  const generateSimulatedPoint = () => {
    setIsSimulated(true);
    const now = new Date();
    const fallbackPoint: CattlePositionPoint = {
      time: now.toISOString(),
      device_id: 'vaca-01 (sim)',
      latitude: -34.9205,
      longitude: -57.9536,
      rssi: -78,
      snr: 9.8,
      battery: 96.5,
      wake_count: 14,
      fence_status: 'INSIDE'
    };
    setLatestPoint(fallbackPoint);
  };

  useEffect(() => {
    fetchTelemetry();
    const interval = setInterval(fetchTelemetry, refreshIntervalMs);
    return () => clearInterval(interval);
  }, [fetchTelemetry, refreshIntervalMs]);

  return { latestPoint, history, isLoading, error, isSimulated, refetch: fetchTelemetry };
}
