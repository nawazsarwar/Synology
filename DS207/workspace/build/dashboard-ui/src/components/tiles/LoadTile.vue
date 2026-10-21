<script setup>
import { computed } from 'vue'
import Gauge from '../../ui/Gauge.vue'
import { uptime } from '../../fmt.js'
import { colorFor } from '../../ui/status.js'
const props = defineProps({ load: { type: Array, default: () => [NaN, NaN, NaN] }, uptimeS: { type: Number, default: NaN } })
const f = (n) => (Number.isFinite(n) ? n.toFixed(2) : '—')
// Single-core box: load 1.0 == fully busy, so the ring reads load1 against 1.0.
const ring = computed(() => (Number.isFinite(props.load[0]) ? Math.min(props.load[0], 1) * 100 : NaN))
const color = computed(() => colorFor(ring.value, 70, 95))
</script>
<template>
  <div class="split">
    <Gauge :value="ring" :display="f(load[0])" :color="color" label="1m load" :size="108" />
    <div class="col">
      <div class="kv"><span class="kv__k">1 min</span><span class="kv__v mono">{{ f(load[0]) }}</span></div>
      <div class="kv"><span class="kv__k">5 min</span><span class="kv__v mono">{{ f(load[1]) }}</span></div>
      <div class="kv"><span class="kv__k">15 min</span><span class="kv__v mono">{{ f(load[2]) }}</span></div>
      <div class="kv"><span class="kv__k">Uptime</span><span class="kv__v mono">{{ uptime(uptimeS) }}</span></div>
    </div>
  </div>
</template>
