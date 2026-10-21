<script setup>
import { computed, watch } from 'vue'
import Sparkline from '../../ui/Sparkline.vue'
import { useHistory } from './useHistory.js'
import { bytes } from '../../fmt.js'
const props = defineProps({ rates: { type: Array, default: () => [] } })
const { h, push } = useHistory(60)
const main = computed(() => props.rates.find((r) => r.dev === 'sda') || props.rates[0])
// sectors/s -> bytes/s (512 B sectors)
const B = (s) => (Number.isFinite(s) ? s * 512 : 0)
watch(() => props.rates, () => { push('rd', B(main.value?.rd_sps)); push('wr', B(main.value?.wr_sps)) }, { immediate: true })
// Hex (not CSS vars): these feed the canvas sparkline. Kept in sync with
// --series-3 / --series-4 in tokens.css.
const READ = '#F0B23A', WRITE = '#9B8CFF'
const series = computed(() => [
  { label: 'read', color: READ, data: h.rd || [] },
  { label: 'write', color: WRITE, data: h.wr || [] }
])
</script>
<template>
  <div class="io-row">
    <span><span class="swatch" :style="{ background: READ }"></span>Read <b class="mono">{{ main ? bytes(B(main.rd_sps)) + '/s' : '—' }}</b></span>
    <span><span class="swatch" :style="{ background: WRITE }"></span>Write <b class="mono">{{ main ? bytes(B(main.wr_sps)) + '/s' : '—' }}</b></span>
  </div>
  <Sparkline :series="series" :height="150" />
  <div class="cap mono" style="margin-top:var(--sp-2)">{{ main ? main.dev : 'no disk' }}</div>
</template>
