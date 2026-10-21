<script setup>
import { computed } from 'vue'
import { kb } from '../../fmt.js'
import Meter from '../../ui/Meter.vue'
import Badge from '../../ui/Badge.vue'
import Icon from '../../ui/Icon.vue'
import { colorFor } from '../../ui/status.js'
const props = defineProps({
  state: { type: String, default: '' },
  volumes: { type: Array, default: () => [] },
  arrays: { type: Array, default: () => [] }
})
const overall = computed(() => props.state || (props.arrays.some((a) => a.state === 'degraded') ? 'degraded'
  : props.arrays.some((a) => a.state === 'recovering') ? 'recovering'
  : props.arrays.length ? 'ok' : ''))
const usedPct = (v) => (v.size_kb > 0 ? (v.used_kb / v.size_kb) * 100 : 0)
const tone = (s) => (s === 'ok' ? 'ok' : s === 'recovering' ? 'warn' : 'bad')
</script>
<template>
  <div v-if="overall === 'degraded'" class="banner degraded" role="alert">
    <Icon name="alert" :size="17" />
    <span><strong>Degraded</strong> — a RAID array is running without redundancy. Replace or re-add the failed disk.</span>
  </div>
  <div v-else-if="overall === 'recovering'" class="banner recovering">
    <Icon name="activity" :size="17" />
    <span><strong>Rebuilding</strong> — the array is restoring redundancy.</span>
  </div>

  <div class="arrays-row">
    <span v-for="a in arrays" :key="a.name" class="arr-chip">
      <span class="mono name">{{ a.name }}</span>
      <Badge :tone="tone(a.state)">{{ a.state }}</Badge>
    </span>
    <span v-if="!arrays.length" class="empty">No arrays reported.</span>
  </div>

  <div v-for="v in volumes" :key="v.mount" class="vol">
    <div class="kv"><span class="kv__k mono">{{ v.mount }}</span><span class="kv__v mono dim">{{ kb(v.used_kb) }} / {{ kb(v.size_kb) }}</span></div>
    <Meter :value="usedPct(v)" :color="colorFor(usedPct(v), 80, 92)" />
  </div>
</template>
<style scoped>
.arrays-row { display: flex; flex-wrap: wrap; gap: var(--sp-2) var(--sp-4); margin-bottom: var(--sp-4); }
.arr-chip { display: inline-flex; align-items: center; gap: var(--sp-2); }
.name { font-weight: var(--fw-semi); color: var(--c-ink); }
.vol { margin-top: var(--sp-3); display: flex; flex-direction: column; gap: 6px; }
</style>
