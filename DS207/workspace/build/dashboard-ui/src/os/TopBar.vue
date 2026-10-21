<script setup>
import { ref, onMounted, onBeforeUnmount } from 'vue'
import Icon from '../ui/Icon.vue'
import Led from '../ui/Led.vue'
import { getPref, setPref, THEMES } from './theme.js'

// Global bar: brand, the overall-health annunciator (the one hero element),
// a live clock, and the signed-in user with a menu (theme switch + log out).
const props = defineProps({
  health: { type: Object, default: () => ({ level: 'ok', text: 'All systems nominal' }) },
  user: { type: String, default: '' }
})
defineEmits(['logout'])

const menuOpen = ref(false)
const theme = ref(getPref())
const clock = ref('')
let timer = null

function tick() {
  const d = new Date()
  const p = (n) => String(n).padStart(2, '0')
  clock.value = `${p(d.getHours())}:${p(d.getMinutes())}`
}
function pickTheme(t) { theme.value = t; setPref(t) }
function onDocClick(e) { if (!e.target.closest('.userchip') && !e.target.closest('.menu')) menuOpen.value = false }
onMounted(() => { tick(); timer = setInterval(tick, 15000); document.addEventListener('click', onDocClick) })
onBeforeUnmount(() => { clearInterval(timer); document.removeEventListener('click', onDocClick) })

const initial = () => (props.user ? props.user[0].toUpperCase() : '•')
const THEME_ICON = { dark: 'activity', light: 'thermometer', auto: 'clock' }
const THEME_LABEL = { dark: 'Dark', light: 'Light', auto: 'Auto' }
</script>

<template>
  <header class="topbar">
    <div class="brand">
      <span class="brand__mark"><Icon name="activity" :size="17" :stroke="2.4" /></span>
      <div>
        <div class="brand__name">DS207</div>
        <div class="brand__sub">DiskStation</div>
      </div>
    </div>

    <span class="annunciator" :class="`is-${health.level}`" role="status">
      <Led :state="health.level" :pulse="health.level === 'bad'" lg />
      <span class="annunciator__txt">{{ health.text }}</span>
    </span>

    <span class="topbar__spacer"></span>

    <span class="clock"><Icon name="clock" :size="14" /><span>{{ clock }}</span></span>

    <button class="userchip" type="button" @click.stop="menuOpen = !menuOpen" :aria-expanded="menuOpen">
      <span class="avatar">{{ initial() }}</span>
      <span class="userchip__name">{{ user || 'Console' }}</span>
      <Icon name="chevron" :size="15" />
    </button>

    <div class="menu" v-show="menuOpen">
      <div class="menu__head">
        <div style="font-weight:600">{{ user || 'Signed in' }}</div>
        <div class="mute" style="font-size:var(--fs-xs)">Read-only monitor</div>
      </div>

      <div class="menu__label">Appearance</div>
      <div class="theme-switch" role="group" aria-label="Theme">
        <button
          v-for="t in THEMES" :key="t" type="button"
          class="theme-opt" :class="{ 'is-on': theme === t }"
          @click="pickTheme(t)"
        >
          <Icon :name="THEME_ICON[t]" :size="15" />
          <span>{{ THEME_LABEL[t] }}</span>
        </button>
      </div>

      <button class="menu__item logout" type="button" @click="$emit('logout')">
        <Icon name="logout" :size="16" /> Log out
      </button>
    </div>
  </header>
</template>

<style scoped>
.annunciator__txt { overflow: hidden; text-overflow: ellipsis; }
.menu__label { font-size: var(--fs-xs); font-weight: 600; color: var(--c-ink-mute); padding: 2px var(--sp-3); margin-top: var(--sp-1); }
.theme-switch { display: flex; gap: 4px; padding: 4px var(--sp-2) var(--sp-2); }
.theme-opt {
  flex: 1; display: flex; flex-direction: column; align-items: center; gap: 4px;
  padding: 8px 4px; border: 1px solid var(--c-line); border-radius: var(--r-sm);
  background: var(--c-surface-2); color: var(--c-ink-dim);
  font-size: var(--fs-xs); font-weight: 600; cursor: pointer;
  transition: border-color var(--motion-fast) var(--ease), color var(--motion-fast) var(--ease), background var(--motion-fast) var(--ease);
}
.theme-opt:hover { color: var(--c-ink); border-color: var(--c-line-strong); }
.theme-opt.is-on { color: var(--c-brand-hi); border-color: var(--c-brand); background: var(--c-brand-soft); }
.menu__item { margin-top: 4px; }
</style>
