<script setup>
import { computed } from 'vue'
const props = defineProps({ wan: { type: Object, default: () => ({}) } })
const pub = computed(() => props.wan?.public_count ?? 0)
const lan = computed(() => props.wan?.lan_count ?? 0)
const conns = computed(() => props.wan?.public_conns || [])
</script>
<template>
  <div class="row">
    <span :class="['public', { alert: pub > 0 }]">Public: <b>{{ pub }}</b></span>
    <span class="lan">LAN: <b>{{ lan }}</b></span>
  </div>
  <ul v-if="conns.length" class="list">
    <li v-for="c in conns" :key="c.local + c.remote"><span>{{ c.remote }}</span><small class="muted">via {{ c.local }}</small></li>
  </ul>
  <div v-else class="muted calm">No connections from outside your LAN</div>
</template>
