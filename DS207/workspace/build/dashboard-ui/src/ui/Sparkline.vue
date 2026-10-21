<script setup>
import { ref, onMounted, onBeforeUnmount, watch } from 'vue'
import uPlot from 'uplot'
import 'uplot/dist/uPlot.min.css'

// Area sparkline. series: [{ label, color, data: number[] }] — equal lengths.
// `color` MUST be a canvas-valid color (6-digit hex), not a CSS var/color-mix:
// uPlot paints on <canvas>, which cannot resolve CSS custom properties.
// Chart-less environments (jsdom tests) degrade silently to an empty div.
const props = defineProps({ series: { type: Array, default: () => [] }, height: { type: Number, default: 96 } })
const fillOf = (hex) => (typeof hex === 'string' && /^#[0-9a-f]{6}$/i.test(hex) ? hex + '2b' : 'transparent')
const el = ref(null)
let plot = null
let ro = null

function data() {
  const n = props.series[0] ? props.series[0].data.length : 0
  return [Array.from({ length: n }, (_, i) => i), ...props.series.map((s) => s.data)]
}
function build() {
  try {
    const opts = {
      width: el.value.clientWidth || 300,
      height: props.height,
      legend: { show: false }, cursor: { show: false }, select: { show: false },
      axes: [{ show: false }, { show: false }],
      scales: { x: { time: false }, y: { range: (u, mn, mx) => [0, Math.max(mx * 1.2, 1)] } },
      series: [{}, ...props.series.map((s) => ({
        stroke: s.color, width: 2, fill: fillOf(s.color), points: { show: false }
      }))]
    }
    plot = new uPlot(opts, data(), el.value)
  } catch (e) { plot = null }
}
onMounted(() => {
  build()
  if (typeof ResizeObserver !== 'undefined' && el.value) {
    ro = new ResizeObserver(() => { if (plot && el.value) plot.setSize({ width: el.value.clientWidth, height: props.height }) })
    ro.observe(el.value)
  }
})
watch(() => props.series.map((s) => s.data), () => { if (plot) plot.setData(data()) }, { deep: false })
onBeforeUnmount(() => { if (ro) ro.disconnect(); if (plot) plot.destroy() })
</script>
<template><div ref="el" class="spark"></div></template>
