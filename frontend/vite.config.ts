/// <reference types="vitest/globals" />
/// <reference types="vitest/config" />
import path from 'node:path'
import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// В AP-режиме устройство всегда 192.168.4.1 (стандарт arduino-esp32 softAP,
// см. CLAUDE.md §3/Plan/09) — переопределяется VITE_DEVICE_IP при отладке
// в STA/домашней сети (например, прошивка временно поднята в тестовом режиме).
const deviceIp = process.env.VITE_DEVICE_IP || '192.168.4.1'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
  server: {
    proxy: {
      '/api': `http://${deviceIp}`,
    },
  },
  test: {
    environment: 'jsdom',
    globals: true,
    setupFiles: ['./src/test/setup.ts'],
  },
})
