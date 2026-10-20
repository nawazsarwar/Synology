import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// base './' => relative asset paths, so the C uiserver can serve from any mount.
// Output goes to ../dashboard/www so the installer stages a single tree.
export default defineConfig({
  base: './',
  plugins: [vue()],
  build: { outDir: '../dashboard/www', emptyOutDir: true },
  test: { environment: 'jsdom', exclude: ['**/node_modules/**', '**/._*', 'dist/**'] }
})
