<script setup>
import { ref } from 'vue'
import { login } from '../api/client.js'
import Icon from '../ui/Icon.vue'
import Led from '../ui/Led.vue'

// Console unlock. Username + password (multi-account ready); the shell persists
// the signed-in name so the top bar can show who is logged in.
const emit = defineEmits(['authed'])
const username = ref('admin')
const password = ref('')
const error = ref('')
const busy = ref(false)

async function submit() {
  if (busy.value) return
  error.value = ''
  busy.value = true
  try {
    const ok = await login(username.value.trim(), password.value)
    if (ok) {
      const name = username.value.trim()
      password.value = ''
      emit('authed', name)
    } else {
      error.value = 'That username or password is incorrect.'
    }
  } catch (e) {
    error.value = 'Could not reach the NAS. Check the connection and try again.'
  } finally {
    busy.value = false
  }
}
</script>

<template>
  <main class="login-scene">
    <form class="login" @submit.prevent="submit">
      <div class="login__head">
        <span class="login__brandmark"><Icon name="activity" :size="24" :stroke="2.4" /></span>
        <div>
          <div class="login__title">DS207</div>
          <div class="login__sub">DiskStation · sign in to the console</div>
        </div>
      </div>

      <div class="field">
        <label class="field__label" for="login-user">Username</label>
        <div class="input-group">
          <span class="input-group__icon"><Icon name="user" :size="17" /></span>
          <input id="login-user" class="input" v-model="username" type="text"
                 autocomplete="username" autocapitalize="none" spellcheck="false" required />
        </div>
      </div>

      <div class="field">
        <label class="field__label" for="login-pass">Password</label>
        <div class="input-group">
          <span class="input-group__icon"><Icon name="lock" :size="17" /></span>
          <input id="login-pass" class="input" v-model="password" type="password"
                 autocomplete="current-password" placeholder="••••••••" autofocus required />
        </div>
      </div>

      <button class="btn btn--primary btn--block" type="submit" :disabled="busy">
        {{ busy ? 'Signing in…' : 'Sign in' }}
        <Icon v-if="!busy" name="arrowRight" :size="18" />
      </button>

      <p class="login-error" role="alert">
        <Icon v-if="error" name="alert" :size="15" /><span>{{ error }}</span>
      </p>

      <div class="login__foot">
        <Led state="ok" /> LAN-only · read-only monitor
      </div>
    </form>
  </main>
</template>
