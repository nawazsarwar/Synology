<script setup>
import { computed } from 'vue'
import Gauge from '../../ui/Gauge.vue'
import { colorFor } from '../../ui/status.js'
const props = defineProps({ busy: { type: Number, default: NaN } })
const color = computed(() => colorFor(props.busy, 70, 90))
</script>
<template>
  <div class="v-center">
    <Gauge :value="busy" :color="color" unit="%" label="busy" :size="124" />
    <div class="cap">single core · PPC 603e</div>
  </div>
</template>
