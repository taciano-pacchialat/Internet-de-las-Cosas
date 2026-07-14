export interface FenceVertex {
  lat: number;
  lon: number;
}

export interface FenceUpdateResponse {
  status: 'success' | 'error';
  message: string;
  topic?: string;
  timestamp?: number;
  fence?: any;
}

export async function fetchCurrentGeofence(): Promise<[number, number][] | null> {
  try {
    let response = await fetch('/api/fence/current');
    if (!response.ok) {
      response = await fetch('/api/node-red/api/fence/current');
    }
    if (!response.ok) return null;
    const data = await response.json();
    if (data && data.fence && Array.isArray(data.fence.vertices) && data.fence.vertices.length >= 3) {
      return data.fence.vertices.map((v: any) => [Number(v.lat), Number(v.lon)]);
    }
    return null;
  } catch (err) {
    console.warn('No se pudo obtener el fence actual de InfluxDB:', err);
    return null;
  }
}

export async function sendGeofenceUpdate(vertices: FenceVertex[]): Promise<FenceUpdateResponse> {
  if (!vertices || vertices.length < 3) {
    throw new Error('El perímetro debe tener al menos 3 vértices para formar un polígono válido.');
  }

  const payload = {
    command: 'UPDATE_GEOFENCE',
    vertex_count: vertices.length,
    vertices: vertices.map((v) => ({
      lat: Number(v.lat.toFixed(6)),
      lon: Number(v.lon.toFixed(6))
    })),
    timestamp: Math.floor(Date.now() / 1000)
  };

  try {
    const response = await fetch('/api/fence', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(payload)
    });

    if (!response.ok) {
      // Fallback a /api/node-red/api/update-fence si /api/fence respondiera error en transición
      const resFallback = await fetch('/api/node-red/api/update-fence', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });
      if (!resFallback.ok) {
        throw new Error(`HTTP ${response.status}`);
      }
      return await resFallback.json();
    }

    const data: FenceUpdateResponse = await response.json();
    return data;
  } catch (error: any) {
    throw new Error(error.message || 'Error al conectar con Node-RED / Puente MQTT');
  }
}
