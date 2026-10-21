<script setup>
// Status LED. state: ok | warn | bad | idle. pulse draws attention (alarms).
defineProps({
  state: { type: String, default: 'idle' },
  pulse: { type: Boolean, default: false },
  lg: { type: Boolean, default: false }
})
</script>
<template>
  <span class="led" :class="[`is-${state}`, { 'led--pulse': pulse, 'led--lg': lg }]"></span>
</template>
