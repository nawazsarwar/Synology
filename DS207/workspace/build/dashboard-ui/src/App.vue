<script setup>
import { onMounted, onUnmounted } from 'vue'
import { useMetricsStore } from './stores/metrics.js'
import Desktop from './components/Desktop.vue'
import './styles.css'

const m = useMetricsStore()
onMounted(() => m.startPolling())
onUnmounted(() => m.stopPolling())
</script>

<template>
  <Desktop />
</template>
