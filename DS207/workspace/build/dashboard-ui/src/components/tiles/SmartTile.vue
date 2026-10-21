<script setup>
import { computed } from 'vue'
import Led from '../../ui/Led.vue'
import Badge from '../../ui/Badge.vue'
const props = defineProps({ disks: { type: Array, default: () => [] } })
const temp = (t) => (typeof t === 'number' && t >= 0 ? t + '°C' : 'n/a')
const tone = (h) => (h === 'ok' ? 'ok' : h === 'fail' ? 'bad' : 'warn')
const overall = computed(() => {
  if (!props.disks.length) return { level: 'idle', text: 'No disk data' }
  if (props.disks.some((d) => d.health === 'fail')) return { level: 'bad', text: 'Attention needed' }
  if (props.disks.some((d) => d.health && d.health !== 'ok')) return { level: 'warn', text: 'Check disks' }
  return { level: 'ok', text: 'All disks healthy' }
})
</script>
<template>
  <div class="head"><Led :state="overall.level" /> <span>{{ overall.text }}</span></div>
  <ul class="datalist">
    <li v-for="d in disks" :key="d.dev">
      <span class="lead">
        <span class="dev mono">{{ d.dev }}</span>
        <span class="model ellipsis dim">{{ d.model || '' }}</span>
      </span>
      <span class="right">
        <span class="mono dim">{{ temp(d.temp_c) }}</span>
        <Badge :tone="tone(d.health)">{{ d.health || 'unknown' }}</Badge>
      </span>
    </li>
    <li v-if="!disks.length" class="empty">SMART data is read every 5 minutes.</li>
  </ul>
</template>
<style scoped>
.head { display: flex; align-items: center; gap: var(--sp-2); font-weight: var(--fw-semi); margin-bottom: var(--sp-2); }
.dev { font-weight: var(--fw-semi); color: var(--c-ink); }
.model { font-size: var(--fs-xs); }
.right { display: flex; align-items: center; gap: var(--sp-3); flex: none; }
</style>
