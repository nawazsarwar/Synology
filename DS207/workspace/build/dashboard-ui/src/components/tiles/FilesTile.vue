<script setup>
import Icon from '../../ui/Icon.vue'
defineProps({ files: { type: Array, default: () => [] } })
</script>
<template>
  <ul class="datalist">
    <li v-for="(f, i) in files" :key="i">
      <span class="lead"><Icon name="file" :size="15" class="dim" /> <span class="ellipsis" :title="f.path">{{ f.path }}</span></span>
      <span class="mono dim">{{ f.user }}</span>
    </li>
    <li v-if="!files.length" class="empty">No open files right now.</li>
  </ul>
</template>
