<script setup>
import { computed } from 'vue'
// 270° instrument ring. value 0..100 (NaN -> empty ring, '—'). color is a CSS
// color/token string; caller decides severity via ui/status.js.
const props = defineProps({
  value: { type: Number, default: NaN },
  color: { type: String, default: 'var(--c-brand)' },
  unit: { type: String, default: '%' },
  label: { type: String, default: '' },
  // Override the centered text (e.g. a load average) while the ring still
  // reflects `value`; when set, the unit is hidden.
  display: { type: String, default: '' },
  size: { type: Number, default: 116 }
})
const val = computed(() => (Number.isFinite(props.value) ? Math.min(100, Math.max(0, props.value)) : 0))
const text = computed(() => (props.display ? props.display : Number.isFinite(props.value) ? Math.round(props.value) : '—'))
const showUnit = computed(() => !props.display && Number.isFinite(props.value))
</script>
<template>
  <div class="gauge" :style="{ '--val': val, '--gcolor': color, '--g': size + 'px' }">
    <div class="gauge__center">
      <div class="gauge__value">{{ text }}<small v-if="showUnit">{{ unit }}</small></div>
      <div v-if="label" class="gauge__label">{{ label }}</div>
    </div>
  </div>
</template>
