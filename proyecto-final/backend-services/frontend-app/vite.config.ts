import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import tailwindcss from '@tailwindcss/vite';

export default defineConfig({
  plugins: [react(), tailwindcss()],
  server: {
    proxy: {
      '/api/influx': {
        target: 'http://localhost:8086',
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/api\/influx/, ''),
      },
      '/api/node-red': {
        target: 'http://localhost:1880',
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/api\/node-red/, ''),
      },
      '/api/fence': {
        target: 'http://localhost:1880',
        changeOrigin: true,
      },
      '/api/history': {
        target: 'http://localhost:1880',
        changeOrigin: true,
      },
    },
  },
});
