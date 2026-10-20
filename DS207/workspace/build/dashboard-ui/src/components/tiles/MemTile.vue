<script setup>
import { computed } from 'vue'
import { kb, pct } from '../../fmt.js'
const props = defineProps({ system: { type: Object, default: null } })
const total = computed(() => props.system?.mem_total)
const used = computed(() => (props.system ? props.system.mem_total - props.system.mem_free - props.system.mem_cached : NaN))
const p = computed(() => (total.value > 0 ? (used.value / total.value) * 100 : NaN))
</script>
<template>
  <div class="big">{{ pct(p) }}</div>
  <div class="bar"><i :style="{ width: (Number.isFinite(p) ? p : 0) + '%' }"></i></div>
  <div class="muted">{{ kb(used) }} used of {{ kb(total) }}</div>
  <div class="muted">cached {{ kb(system?.mem_cached) }}</div>
</template>
