<script setup>
defineProps({ services: { type: Array, default: () => [] } })
</script>
<template>
  <ul class="list">
    <li v-for="s in services" :key="s.name">
      <span><i :class="['dot', s.up ? 'up' : 'down']"></i>{{ s.name }}</span>
      <span><b>{{ s.up ? 'up' : 'down' }}</b> <small class="muted">{{ s.version || '—' }}</small></span>
    </li>
    <li v-if="!services.length" class="muted">—</li>
  </ul>
</template>
