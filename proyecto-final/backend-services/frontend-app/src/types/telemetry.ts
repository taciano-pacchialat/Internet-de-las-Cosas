export interface CattlePositionPoint {
  time: string;
  device_id: string;
  latitude: number;
  longitude: number;
  rssi: number;
  snr: number;
  battery: number;
  wake_count: number;
  fence_status: 'INSIDE' | 'OUTSIDE' | 'UNKNOWN';
}

export interface TelemetryHistoryPoint {
  timeFormatted: string;
  battery: number;
  rssi: number;
  snr: number;
  wake_count: number;
}
