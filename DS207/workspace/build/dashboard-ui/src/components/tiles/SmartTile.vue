<script setup>
defineProps({ disks: { type: Array, default: () => [] } })
const temp = (t) => (typeof t === 'number' && t >= 0 ? t + '°C' : 'n/a')
</script>
<template>
  <ul class="list">
    <li v-for="d in disks" :key="d.dev">
      <span>{{ d.dev }} <small>{{ d.model || '' }}</small></span>
      <span><b :class="['health', d.health]">{{ d.health || 'unknown' }}</b> · {{ temp(d.temp_c) }}</span>
    </li>
    <li v-if="!disks.length" class="muted">—</li>
  </ul>
</template>
