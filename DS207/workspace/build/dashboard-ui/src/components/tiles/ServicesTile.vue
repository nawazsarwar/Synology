<script setup>
import Led from '../../ui/Led.vue'
defineProps({ services: { type: Array, default: () => [] } })
// Tidy a raw version string for display ("Version 4.15.13" -> "4.15.13").
const clean = (v) => (v ? v.replace(/^Version\s+/i, '').replace(/^Dropbear\s+v?/i, '').trim() : '')
// Secondary line: version (or —) and the listening port when known.
function detail(s) {
  const parts = [clean(s.version) || '—']
  if (s.port) parts.push('port ' + s.port)
  return parts.join('  ·  ')
}
</script>
<template>
  <ul class="datalist">
    <li v-for="s in services" :key="s.name">
      <span class="lead">
        <Led :state="s.up ? 'ok' : 'bad'" />
        <span class="info">
          <span class="name">{{ s.name }}</span>
          <span class="detail mono">{{ detail(s) }}</span>
        </span>
      </span>
      <span class="state" :class="s.up ? 'up' : 'down'">{{ s.up ? 'up' : 'down' }}</span>
    </li>
    <li v-if="!services.length" class="empty">No services reported.</li>
  </ul>
</template>
<style scoped>
.lead { align-items: flex-start; }
.info { display: flex; flex-direction: column; gap: 1px; min-width: 0; }
.name { font-weight: var(--fw-semi); color: var(--c-ink); }
.detail { font-size: var(--fs-xs); color: var(--c-ink-mute); }
.state { font-weight: var(--fw-semi); font-size: var(--fs-sm); flex: none; }
.state.up { color: var(--c-ok); }
.state.down { color: var(--c-bad); }
</style>
