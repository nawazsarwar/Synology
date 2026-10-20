<script setup>
import { computed, watch } from 'vue'
import Spark from './Spark.vue'
import { useHistory } from './useHistory.js'
import { bytes } from '../../fmt.js'
const props = defineProps({ rates: { type: Array, default: () => [] } })
const { h, push } = useHistory(60)
const main = computed(() => props.rates.find((r) => r.dev === 'sda') || props.rates[0])
// sectors/s -> bytes/s (512 B sectors)
const B = (s) => (Number.isFinite(s) ? s * 512 : 0)
watch(() => props.rates, () => { push('rd', B(main.value?.rd_sps)); push('wr', B(main.value?.wr_sps)) }, { immediate: true })
const series = computed(() => [
  { label: 'read', color: '#fbbf24', data: h.rd || [] },
  { label: 'write', color: '#f472b6', data: h.wr || [] }
])
</script>
<template>
  <div class="row"><span class="rd">R {{ main ? bytes(B(main.rd_sps)) + '/s' : '—' }}</span><span class="wr">W {{ main ? bytes(B(main.wr_sps)) + '/s' : '—' }}</span></div>
  <Spark :series="series" />
  <div class="muted">{{ main ? main.dev : 'no disk' }}</div>
</template>
