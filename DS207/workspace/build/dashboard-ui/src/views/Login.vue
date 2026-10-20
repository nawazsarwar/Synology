<script setup>
import { ref } from 'vue'
import { login } from '../api/client.js'

const emit = defineEmits(['authed'])
const password = ref('')
const error = ref('')
const busy = ref(false)

async function submit() {
  if (busy.value) return
  error.value = ''
  busy.value = true
  try {
    const ok = await login(password.value)
    if (ok) {
      password.value = ''
      emit('authed')
    } else {
      error.value = 'Incorrect password.'
    }
  } catch (e) {
    error.value = 'Could not reach the NAS. Check the connection and try again.'
  } finally {
    busy.value = false
  }
}
</script>

<template>
  <main class="login-wrap">
    <form class="login" @submit.prevent="submit">
      <h1>DS207</h1>
      <p class="muted">Sign in to view the dashboard</p>
      <input v-model="password" type="password" placeholder="Password" autocomplete="current-password" autofocus required />
      <button type="submit" :disabled="busy">{{ busy ? 'Signing in…' : 'Sign in' }}</button>
      <p class="login-error" role="alert">{{ error }}</p>
    </form>
  </main>
</template>
