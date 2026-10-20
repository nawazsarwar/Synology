<script setup>
import { uptime } from '../../fmt.js'
defineProps({ load: { type: Array, default: () => [NaN, NaN, NaN] }, uptimeS: { type: Number, default: NaN } })
const f = (n) => (Number.isFinite(n) ? n.toFixed(2) : '—')
</script>
<template>
  <div class="big">{{ f(load[0]) }}</div>
  <div class="muted">1m · 5m · 15m</div>
  <div>{{ f(load[0]) }} · {{ f(load[1]) }} · {{ f(load[2]) }}</div>
  <div class="muted">up {{ uptime(uptimeS) }}</div>
</template>
