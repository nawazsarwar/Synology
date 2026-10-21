<script setup>
// Small status chip. tone: neutral | ok | warn | bad | brand.
defineProps({ tone: { type: String, default: 'neutral' } })
const cls = { ok: 'is-ok', warn: 'is-warn', bad: 'is-bad', brand: 'is-brand' }
</script>
<template>
  <span class="badge" :class="cls[tone]"><slot /></span>
</template>
