/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  darkMode: 'class',
  theme: {
    extend: {
      colors: {
        hud: {
          bg: '#0A0D14',
          surface: '#111622',
          card: '#151C2C',
          border: '#1E283D',
          accent: '#10B981',
          alert: '#EF4444',
          warning: '#F59E0B',
          cyan: '#06B6D4',
        }
      },
      fontFamily: {
        sans: ['Outfit', 'Inter', 'system-ui', 'sans-serif'],
        mono: ['JetBrains Mono', 'monospace'],
      },
      boxShadow: {
        'neon-green': '0 0 15px rgba(16, 185, 129, 0.35)',
        'neon-red': '0 0 20px rgba(239, 68, 68, 0.45)',
      }
    },
  },
  plugins: [],
}
