<script setup>
defineProps({ sessions: { type: Object, default: () => ({ smb: [], ssh: [] }) } })
</script>
<template>
  <h4>SMB ({{ (sessions.smb || []).length }})</h4>
  <ul class="list">
    <li v-for="(s, i) in sessions.smb || []" :key="'s' + i"><span>{{ s.user }}</span><span class="muted">{{ s.ip }} {{ s.share ? '· ' + s.share : '' }}</span></li>
    <li v-if="!(sessions.smb || []).length" class="muted">none</li>
  </ul>
  <h4>SSH ({{ (sessions.ssh || []).length }})</h4>
  <ul class="list">
    <li v-for="(s, i) in sessions.ssh || []" :key="'h' + i"><span>{{ s.user }}</span><span class="muted">{{ s.ip }}</span></li>
    <li v-if="!(sessions.ssh || []).length" class="muted">none</li>
  </ul>
</template>
