<script setup>
import Icon from '../ui/Icon.vue'
// The dock is the app launcher and the visible roadmap: the Dashboard is live,
// later-phase apps sit here disabled with a "Soon" hint so the growth path is
// legible. Future phases flip `enabled` in the registry — the dock needs no edit.
defineProps({ apps: { type: Array, default: () => [] }, active: { type: String, default: 'dashboard' } })
defineEmits(['open'])
</script>

<template>
  <nav class="dock" aria-label="Applications">
    <template v-for="(app, i) in apps" :key="app.id">
      <span v-if="i === 1" class="dock__sep"></span>
      <button
        type="button"
        class="dock__item"
        :class="{ 'is-active': app.enabled && app.id === active, 'is-disabled': !app.enabled }"
        :disabled="!app.enabled"
        @click="app.enabled && $emit('open', app.id)"
      >
        <Icon :name="app.icon" :size="21" />
        <span class="dock__tip">{{ app.name }}<span v-if="app.soon"> · Soon</span></span>
      </button>
    </template>
  </nav>
</template>
