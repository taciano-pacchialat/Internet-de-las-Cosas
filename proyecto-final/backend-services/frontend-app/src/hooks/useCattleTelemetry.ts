import { useState, useEffect, useCallback } from 'react';
import type { CattlePositionPoint, TelemetryHistoryPoint } from '../types/telemetry';

export function useCattleTelemetry(refreshIntervalMs = 3000) {
  const [latestPoint, setLatestPoint] = useState<CattlePositionPoint | null>(null);
  const [latestPointsByDevice, setLatestPointsByDevice] = useState<CattlePositionPoint[]>([]);
  const [trailsByDevice, setTrailsByDevice] = useState<Record<string, CattlePositionPoint[]>>({});
  const [availableDevices, setAvailableDevices] = useState<string[]>([]);
  const [history, setHistory] = useState<TelemetryHistoryPoint[]>([]);
  const [isLoading, setIsLoading] = useState<boolean>(true);
  const [error, setError] = useState<string | null>(null);
  const [isSimulated, setIsSimulated] = useState<boolean>(false);

  const fetchTelemetry = useCallback(async () => {
    try {
      const query = `SELECT * FROM "cattle_position" ORDER BY time DESC LIMIT 200`;
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

        // Ordenar cronológicamente ascendente (del más antiguo al más reciente)
        const sortedChronological = points.slice().sort((a, b) => 
          new Date(a.time).getTime() - new Date(b.time).getTime()
        );

        // Agrupar por device_id
        const trails: Record<string, CattlePositionPoint[]> = {};
        const latestMap: Record<string, CattlePositionPoint> = {};

        for (const pt of sortedChronological) {
          if (!trails[pt.device_id]) {
            trails[pt.device_id] = [];
          }
          trails[pt.device_id].push(pt);
          latestMap[pt.device_id] = pt; // al ser cronológico, el último iterado es el más reciente
        }

        const devIds = Object.keys(trails).sort();
        const latestArr = devIds.map((id) => latestMap[id]);

        setAvailableDevices(devIds);
        setTrailsByDevice(trails);
        setLatestPointsByDevice(latestArr);

        // El punto más reciente global
        const latestGlobal = points[0];
        setLatestPoint(latestGlobal);

        const historyPoints: TelemetryHistoryPoint[] = sortedChronological.map((p) => {
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
          generateSimulatedPoints();
        }
      }
    } catch (err: any) {
      setError(err.message || 'Error fetching InfluxDB telemetry');
      if (!latestPoint) {
        generateSimulatedPoints();
      }
    } finally {
      setIsLoading(false);
    }
  }, [latestPoint]);

  const generateSimulatedPoints = () => {
    setIsSimulated(true);
    const now = Date.now();
    const simDevices = ['vaca-01', 'vaca-02'];
    const trails: Record<string, CattlePositionPoint[]> = {};
    const latestArr: CattlePositionPoint[] = [];

    // Coordenadas base
    const baseCoords: Record<string, [number, number]> = {
      'vaca-01': [-34.9205, -57.9536],
      'vaca-02': [-34.9198, -57.9545]
    };

    simDevices.forEach((dev) => {
      trails[dev] = [];
      const [baseLat, baseLon] = baseCoords[dev];
      for (let i = 0; i < 8; i++) {
        const pt: CattlePositionPoint = {
          time: new Date(now - (8 - i) * 15000).toISOString(),
          device_id: dev,
          latitude: baseLat + i * 0.0002,
          longitude: baseLon + i * 0.00015,
          rssi: -75 - i,
          snr: 9.8,
          battery: 96 - i * 0.2,
          wake_count: 10 + i,
          fence_status: 'INSIDE'
        };
        trails[dev].push(pt);
      }
      latestArr.push(trails[dev][trails[dev].length - 1]);
    });

    setAvailableDevices(simDevices);
    setTrailsByDevice(trails);
    setLatestPointsByDevice(latestArr);
    setLatestPoint(latestArr[0]);
  };

  useEffect(() => {
    fetchTelemetry();
    const interval = setInterval(fetchTelemetry, refreshIntervalMs);
    return () => clearInterval(interval);
  }, [fetchTelemetry, refreshIntervalMs]);

  return {
    latestPoint,
    latestPointsByDevice,
    trailsByDevice,
    availableDevices,
    history,
    isLoading,
    error,
    isSimulated,
    refetch: fetchTelemetry
  };
}
