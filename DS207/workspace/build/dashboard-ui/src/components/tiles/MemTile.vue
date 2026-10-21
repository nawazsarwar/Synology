<script setup>
import { computed } from 'vue'
import Gauge from '../../ui/Gauge.vue'
import { kb } from '../../fmt.js'
import { colorFor } from '../../ui/status.js'
const props = defineProps({ system: { type: Object, default: null } })
const total = computed(() => props.system?.mem_total)
const used = computed(() => (props.system ? props.system.mem_total - props.system.mem_free - props.system.mem_cached : NaN))
const free = computed(() => props.system?.mem_free)
const cached = computed(() => props.system?.mem_cached)
const p = computed(() => (total.value > 0 ? (used.value / total.value) * 100 : NaN))
const color = computed(() => colorFor(p.value, 75, 90))
</script>
<template>
  <div class="split">
    <Gauge :value="p" :color="color" unit="%" label="used" :size="108" />
    <div class="col">
      <div class="kv"><span class="kv__k">Used</span><span class="kv__v mono">{{ kb(used) }}</span></div>
      <div class="kv"><span class="kv__k">Free</span><span class="kv__v mono">{{ kb(free) }}</span></div>
      <div class="kv"><span class="kv__k">Cached</span><span class="kv__v mono">{{ kb(cached) }}</span></div>
      <div class="kv"><span class="kv__k">Total</span><span class="kv__v mono">{{ kb(total) }}</span></div>
    </div>
  </div>
</template>
