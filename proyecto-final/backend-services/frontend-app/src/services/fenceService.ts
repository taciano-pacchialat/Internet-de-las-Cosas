export interface FenceVertex {
  lat: number;
  lon: number;
}

export interface FenceUpdateResponse {
  status: 'success' | 'error';
  message: string;
  topic?: string;
  timestamp?: number;
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
    const response = await fetch('/api/node-red/api/update-fence', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(payload)
    });

    if (!response.ok) {
      const errText = await response.text();
      throw new Error(`HTTP ${response.status}: ${errText || response.statusText}`);
    }

    const data: FenceUpdateResponse = await response.json();
    return data;
  } catch (error: any) {
    throw new Error(error.message || 'Error al conectar con Node-RED / Puente MQTT');
  }
}
