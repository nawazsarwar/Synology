<script setup>
import { computed, watch } from 'vue'
import Spark from './Spark.vue'
import { useHistory } from './useHistory.js'
import { bytes } from '../../fmt.js'
const props = defineProps({ rates: { type: Array, default: () => [] } })
const { h, push } = useHistory(60)
const main = computed(() => props.rates.find((r) => r.iface === 'eth0') || props.rates.find((r) => r.iface !== 'lo') || props.rates[0])
watch(() => props.rates, () => {
  push('rx', main.value?.rx_bps); push('tx', main.value?.tx_bps)
}, { immediate: true })
const series = computed(() => [
  { label: 'rx', color: '#4ade80', data: h.rx || [] },
  { label: 'tx', color: '#60a5fa', data: h.tx || [] }
])
</script>
<template>
  <div class="row"><span class="rx">▼ {{ main ? bytes(main.rx_bps) + '/s' : '—' }}</span><span class="tx">▲ {{ main ? bytes(main.tx_bps) + '/s' : '—' }}</span></div>
  <Spark :series="series" />
  <div class="muted">{{ main ? main.iface : 'no interface' }}</div>
</template>
