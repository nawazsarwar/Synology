<script setup>
import { pct } from '../../fmt.js'
defineProps({ busy: { type: Number, default: NaN } })
</script>
<template>
  <div class="big">{{ pct(busy) }}</div>
  <div class="bar"><i :style="{ width: (Number.isFinite(busy) ? busy : 0) + '%' }"></i></div>
  <div class="muted">CPU busy</div>
</template>
