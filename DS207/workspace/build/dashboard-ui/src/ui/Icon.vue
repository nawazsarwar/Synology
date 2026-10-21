<script setup>
// Dependency-free line-icon set. One SVG dictionary keeps the whole product on
// a single visual icon language; future phases add a path here, never a font.
const props = defineProps({
  name: { type: String, required: true },
  size: { type: [Number, String], default: 20 },
  stroke: { type: [Number, String], default: 1.8 }
})
// 24x24 viewBox, stroke paths (round join/cap set on the <svg>).
const P = {
  dashboard: 'M4 4h7v7H4zM13 4h7v4h-7zM13 11h7v9h-7zM4 14h7v6H4z',
  cpu: 'M6 6h12v12H6zM9 9h6v6H9M9 1v3M15 1v3M9 20v3M15 20v3M1 9h3M1 15h3M20 9h3M20 15h3',
  memory: 'M3 7h18v8H3zM6 15v3M10 15v3M14 15v3M18 15v3M7 10v2M12 10v2M17 10v2',
  activity: 'M3 12h4l3 8 4-16 3 8h4',
  thermometer: 'M14 14.76V5a2 2 0 0 0-4 0v9.76a4 4 0 1 0 4 0z',
  storage: 'M3 6c0-1.7 4-3 9-3s9 1.3 9 3-4 3-9 3-9-1.3-9-3zM3 6v6c0 1.7 4 3 9 3s9-1.3 9-3V6M3 12v6c0 1.7 4 3 9 3s9-1.3 9-3v-6',
  network: 'M5 12.5a10 10 0 0 1 14 0M8 15.5a6 6 0 0 1 8 0M2 9.5a15 15 0 0 1 20 0M12 19h.01',
  disk: 'M4 5h16v14H4zM8 9h8M8 13h8M8 17h5',
  shield: 'M12 3l7 3v5c0 5-3.5 8-7 9-3.5-1-7-4-7-9V6z',
  users: 'M16 20v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2M9 10a4 4 0 1 0 0-8 4 4 0 0 0 0 8M22 20v-2a4 4 0 0 0-3-3.87M16 3.13A4 4 0 0 1 16 11',
  file: 'M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8zM14 3v5h5M9 13h6M9 17h6',
  server: 'M3 4h18v6H3zM3 14h18v6H3zM7 7h.01M7 17h.01M11 7h6M11 17h6',
  power: 'M12 3v9M6.6 6.6a9 9 0 1 0 10.8 0',
  logout: 'M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4M16 17l5-5-5-5M21 12H9',
  lock: 'M5 11h14v10H5zM8 11V7a4 4 0 0 1 8 0v4',
  user: 'M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2M12 11a4 4 0 1 0 0-8 4 4 0 0 0 0 8',
  chevron: 'M6 9l6 6 6-6',
  arrowRight: 'M5 12h14M13 6l6 6-6 6',
  close: 'M6 6l12 12M18 6L6 18',
  minus: 'M5 12h14',
  check: 'M20 6L9 17l-5-5',
  alert: 'M12 3l10 18H2zM12 9v5M12 18h.01',
  clock: 'M12 7v5l3 2M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0z'
}
</script>

<template>
  <svg
    :width="size" :height="size" viewBox="0 0 24 24" fill="none"
    :stroke-width="stroke" stroke="currentColor"
    stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"
  >
    <path :d="P[name] || ''" />
  </svg>
</template>
