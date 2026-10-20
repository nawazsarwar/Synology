<script setup>
import { computed } from 'vue'
import { kb } from '../../fmt.js'
const props = defineProps({
  state: { type: String, default: '' },
  volumes: { type: Array, default: () => [] },
  arrays: { type: Array, default: () => [] }
})
const overall = computed(() => props.state || (props.arrays.some((a) => a.state === 'degraded') ? 'degraded' : props.arrays.some((a) => a.state === 'recovering') ? 'recovering' : props.arrays.length ? 'ok' : ''))
const usedPct = (v) => (v.size_kb > 0 ? Math.round((v.used_kb / v.size_kb) * 100) : 0)
</script>
<template>
  <div v-if="overall === 'degraded'" class="alarm degraded" role="alert">
    <strong>⚠ Degraded</strong> — RAID array is running without redundancy. Replace/re-add the failed disk.
  </div>
  <div v-else-if="overall === 'recovering'" class="alarm recovering">Recovering — array is rebuilding</div>
  <ul class="list">
    <li v-for="a in arrays" :key="a.name" :class="['arr', a.state]">
      <span>{{ a.name }} <small>{{ a.level }}</small></span>
      <b>{{ a.state }}</b>
    </li>
    <li v-if="!arrays.length" class="muted">—</li>
  </ul>
  <div v-for="v in volumes" :key="v.mount" class="vol">
    <div class="row"><span>{{ v.mount }}</span><span class="muted">{{ kb(v.used_kb) }} / {{ kb(v.size_kb) }}</span></div>
    <div class="bar"><i :style="{ width: usedPct(v) + '%' }" :class="{ hot: usedPct(v) >= 90 }"></i></div>
  </div>
</template>
