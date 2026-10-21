<script setup>
import { computed } from 'vue'
// Linear meter. value 0..100, color is a CSS color/token string.
const props = defineProps({
  value: { type: Number, default: 0 },
  color: { type: String, default: 'var(--c-brand)' }
})
const w = computed(() => (Number.isFinite(props.value) ? Math.min(100, Math.max(0, props.value)) : 0))
</script>
<template>
  <div class="meter"><div class="meter__fill" :style="{ width: w + '%', '--mcolor': color }"></div></div>
</template>
