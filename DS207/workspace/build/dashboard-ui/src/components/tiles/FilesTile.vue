<script setup>
defineProps({ files: { type: Array, default: () => [] } })
</script>
<template>
  <ul class="list files">
    <li v-for="(f, i) in files" :key="i"><span class="path" :title="f.path">{{ f.path }}</span><span class="muted">{{ f.user }}</span></li>
    <li v-if="!files.length" class="muted">No open files</li>
  </ul>
</template>
