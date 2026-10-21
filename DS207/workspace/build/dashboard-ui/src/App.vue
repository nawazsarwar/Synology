<script setup>
import { ref, onMounted, onUnmounted } from 'vue'
import { useMetricsStore } from './stores/metrics.js'
import { getJSON } from './api/client.js'
import Desktop from './components/Desktop.vue'
import Login from './views/Login.vue'

const USER_KEY = 'ds207.user'
const m = useMetricsStore()
const authed = ref(false)
const checking = ref(true) // probing for an existing session cookie
const user = ref(readUser())

function readUser() { try { return localStorage.getItem(USER_KEY) || '' } catch (e) { return '' } }
function rememberUser(name) { try { if (name) localStorage.setItem(USER_KEY, name) } catch (e) { /* ignore */ } }

function enter(name) {
  if (name) { user.value = name; rememberUser(name) }
  authed.value = true
  m.startPolling()
}
// Used for logout and for any 401: stop the timers first so requests cease.
function leave() {
  m.stopPolling()
  authed.value = false
}
function logout() {
  fetch('api/logout', { method: 'POST', credentials: 'same-origin' }).catch(() => {}) // best effort
  try { localStorage.removeItem(USER_KEY) } catch (e) { /* ignore */ }
  user.value = ''
  leave()
}

onMounted(async () => {
  window.addEventListener('ds207:unauthorized', leave)
  try {
    if ((await getJSON('api/system')) !== null) enter() // existing session: keep remembered name
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
  <Desktop v-if="authed" :user="user" @logout="logout" />
  <Login v-else-if="!checking" @authed="enter" />
</template>
