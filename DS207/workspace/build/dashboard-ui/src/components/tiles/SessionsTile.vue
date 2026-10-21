<script setup>
import Icon from '../../ui/Icon.vue'
defineProps({ sessions: { type: Object, default: () => ({ smb: [], ssh: [] }) } })
</script>
<template>
  <div class="section-label">SMB · {{ (sessions.smb || []).length }}</div>
  <ul class="datalist">
    <li v-for="(s, i) in sessions.smb || []" :key="'s' + i">
      <span class="lead"><Icon name="user" :size="15" class="dim" /> <span>{{ s.user }}</span></span>
      <span class="mono dim">{{ s.ip }}{{ s.share ? ' · ' + s.share : '' }}</span>
    </li>
    <li v-if="!(sessions.smb || []).length" class="empty">No file-sharing sessions.</li>
  </ul>

  <div class="section-label">SSH · {{ (sessions.ssh || []).length }}</div>
  <ul class="datalist">
    <li v-for="(s, i) in sessions.ssh || []" :key="'h' + i">
      <span class="lead"><Icon name="user" :size="15" class="dim" /> <span>{{ s.user }}</span></span>
      <span class="mono dim">{{ s.ip }}</span>
    </li>
    <li v-if="!(sessions.ssh || []).length" class="empty">No shell sessions.</li>
  </ul>
</template>
