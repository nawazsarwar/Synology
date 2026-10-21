<script setup>
import { computed, watch } from 'vue'
import Sparkline from '../../ui/Sparkline.vue'
import { useHistory } from './useHistory.js'
import { bytes } from '../../fmt.js'
const props = defineProps({ rates: { type: Array, default: () => [] } })
const { h, push } = useHistory(60)
const main = computed(() => props.rates.find((r) => r.iface === 'eth0') || props.rates.find((r) => r.iface !== 'lo') || props.rates[0])
watch(() => props.rates, () => { push('rx', main.value?.rx_bps); push('tx', main.value?.tx_bps) }, { immediate: true })
// Hex (not CSS vars): these feed the canvas sparkline. Kept in sync with
// --series-2 / --series-1 in tokens.css.
const IN = '#32D296', OUT = '#3BC8E0'
const series = computed(() => [
  { label: 'in', color: IN, data: h.rx || [] },
  { label: 'out', color: OUT, data: h.tx || [] }
])
const rate = (v) => (Number.isFinite(v) ? bytes(v) + '/s' : '—')
</script>
<template>
  <div class="io-row">
    <span><span class="swatch" :style="{ background: IN }"></span>In <b class="mono">{{ main ? rate(main.rx_bps) : '—' }}</b></span>
    <span><span class="swatch" :style="{ background: OUT }"></span>Out <b class="mono">{{ main ? rate(main.tx_bps) : '—' }}</b></span>
  </div>
  <Sparkline :series="series" :height="150" />
  <div class="cap mono" style="margin-top:var(--sp-2)">{{ main ? main.iface : 'no interface' }}</div>
</template>
