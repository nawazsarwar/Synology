<script setup>
import { ref, onMounted, onUnmounted } from 'vue'
import { useMetricsStore } from './stores/metrics.js'
import { getJSON } from './api/client.js'
import Desktop from './components/Desktop.vue'
import Login from './views/Login.vue'
import './styles.css'

const m = useMetricsStore()
const authed = ref(false)
const checking = ref(true) // probing for an existing session cookie

function enter() {
  authed.value = true
  m.startPolling()
}
// Used for logout and for any 401: stop the timers first so requests cease.
function leave() {
  m.stopPolling()
  authed.value = false
}
function logout() {
  fetch('api/logout', { method: 'POST', credentials: 'same-origin' }).catch(() => {}) // best effort; endpoint may not exist
  leave()
}

onMounted(async () => {
  window.addEventListener('ds207:unauthorized', leave)
  try {
    if ((await getJSON('api/system')) !== null) enter()
  } catch (e) {
    /* unreachable or errored: stay on Login */
  }
  checking.value = false
})
onUnmounted(() => {
  window.removeEventListener('ds207:unauthorized', leave)
  m.stopPolling()
})
</script>

<template>
  <Desktop v-if="authed" @logout="logout" />
  <Login v-else-if="!checking" @authed="enter" />
</template>
