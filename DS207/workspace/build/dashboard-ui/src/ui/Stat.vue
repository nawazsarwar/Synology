<script setup>
// Hero readout: a big value with optional unit and a sub label.
defineProps({
  value: { type: [String, Number], default: '—' },
  unit: { type: String, default: '' },
  label: { type: String, default: '' }
})
</script>
<template>
  <div class="stat">
    <div class="stat__value">{{ value }}<span v-if="unit" class="stat__unit">{{ unit }}</span></div>
    <div v-if="label" class="stat__label">{{ label }}</div>
  </div>
</template>
