<script setup>
import { computed } from 'vue'
import Icon from '../../ui/Icon.vue'
const props = defineProps({ wan: { type: Object, default: () => ({}) } })
const pub = computed(() => props.wan?.public_count ?? 0)
const lan = computed(() => props.wan?.lan_count ?? 0)
const conns = computed(() => props.wan?.public_conns || [])
</script>
<template>
  <div class="counts">
    <div class="count" :class="['public', { alert: pub > 0 }]">
      <div class="count__n">{{ pub }}</div>
      <div class="count__l">Public<span v-if="pub > 0"> ⚠</span></div>
    </div>
    <div class="count lan">
      <div class="count__n">{{ lan }}</div>
      <div class="count__l">LAN: {{ lan }}</div>
    </div>
  </div>

  <ul v-if="conns.length" class="datalist">
    <li v-for="c in conns" :key="c.local + c.remote">
      <span class="lead"><Icon name="shield" :size="15" class="dim" /> <span class="mono">{{ c.remote }}</span></span>
      <span class="cap mono">via {{ c.local }}</span>
    </li>
  </ul>
  <div v-else class="calm"><Icon name="check" :size="15" /> No connections from outside your LAN</div>
</template>
<style scoped>
.counts { display: flex; gap: var(--sp-3); margin-bottom: var(--sp-3); }
.count { flex: 1; padding: var(--sp-3); border-radius: var(--r-sm); background: rgba(130,165,195,0.06); border: 1px solid var(--c-line); }
.count__n { font: var(--fw-black) var(--fs-h1)/1 var(--font-ui); font-variant-numeric: tabular-nums; color: var(--c-ink-strong); }
.count__l { font-size: var(--fs-xs); color: var(--c-ink-mute); margin-top: 3px; }
.count.public.alert { background: var(--c-bad-soft); border-color: color-mix(in srgb, var(--c-bad) 45%, transparent); }
.count.public.alert .count__n, .count.public.alert .count__l { color: var(--c-bad-hi); }
.calm { display: flex; align-items: center; gap: var(--sp-2); font-size: var(--fs-sm); color: var(--c-ok); }
</style>
