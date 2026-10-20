<script setup>
import { onMounted, onUnmounted } from 'vue'
import { useMetricsStore } from './stores/metrics.js'

const m = useMetricsStore()
onMounted(() => m.startPolling())
onUnmounted(() => m.stopPolling())
</script>

<template>
  <main>
    <h1>DS207</h1>
    <p>Dashboard scaffold (tiles: Task 14, login: Task 15).</p>
  </main>
</template>
