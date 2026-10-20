<script setup>
import { ref, onMounted, onBeforeUnmount, watch } from 'vue'
import uPlot from 'uplot'
import 'uplot/dist/uPlot.min.css'

// series: [{label, color, data:number[]}]; all data arrays same length.
const props = defineProps({ series: { type: Array, default: () => [] }, height: { type: Number, default: 90 } })
const el = ref(null)
let plot = null
let ro = null

function build() {
  try {
    const w = el.value.clientWidth || 300
    const opts = {
      width: w,
      height: props.height,
      legend: { show: false },
      cursor: { show: false },
      select: { show: false },
      axes: [{ show: false }, { show: false }],
      scales: { x: { time: false }, y: { range: (u, mn, mx) => [0, Math.max(mx * 1.15, 1)] } },
      series: [{}, ...props.series.map((s) => ({ stroke: s.color, width: 1.5, fill: s.color + '22', points: { show: false } }))]
    }
    plot = new uPlot(opts, data(), el.value)
  } catch (e) { plot = null } // no canvas (tests) => silently no chart
}
function data() {
  const n = props.series[0] ? props.series[0].data.length : 0
  return [Array.from({ length: n }, (_, i) => i), ...props.series.map((s) => s.data)]
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
