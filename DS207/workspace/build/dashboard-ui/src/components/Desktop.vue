<script setup>
import { reactive, computed } from 'vue'
import { useMetricsStore } from '../stores/metrics.js'
import { WIDGETS, APPS } from '../os/registry.js'
import TopBar from '../os/TopBar.vue'
import Dock from '../os/Dock.vue'
import Icon from '../ui/Icon.vue'
import Led from '../ui/Led.vue'

// The OS shell: global top bar, a fluid responsive grid of Dashboard widgets,
// and the app dock. The grid reflows across every screen size in pure CSS
// (repeat(auto-fill, minmax(...))) — no layout math, no breakpoints.
defineProps({ user: { type: String, default: '' } })
defineEmits(['logout'])

const m = useMetricsStore()
const collapsed = reactive({})

const degraded = computed(() => (m.storage.arrays || []).some((a) => a.state === 'degraded'))
const recovering = computed(() => (m.storage.arrays || []).some((a) => a.state === 'recovering'))
const offline = computed(() => !!m.error)
const health = computed(() => {
  if (degraded.value) return { level: 'bad', text: 'RAID degraded' }
  if (offline.value) return { level: 'warn', text: 'Connection issue' }
  if (recovering.value) return { level: 'warn', text: 'Array rebuilding' }
  return { level: 'ok', text: 'All systems nominal' }
})

function propsFor(id) {
  switch (id) {
    case 'cpu': return { busy: m.system ? m.cpuBusy : NaN }
    case 'mem': return { system: m.system }
    case 'load': return { load: m.system ? m.load : [NaN, NaN, NaN], uptimeS: m.system ? m.system.uptime_s : NaN }
    case 'smart': return { disks: m.disks }
    case 'storage': return { volumes: m.storage.volumes || [], arrays: m.storage.arrays || [] }
    case 'net': return { rates: m.netRates }
    case 'diskio': return { rates: m.diskRates }
    case 'services': return { services: m.services }
    case 'sessions': return { sessions: m.sessions }
    case 'files': return { files: m.sessions.open_files || [] }
    case 'wan': return { wan: m.wan }
    default: return {}
  }
}
const isAlarm = (id) => id === 'storage' && degraded.value
const toggleCollapse = (id) => { collapsed[id] = !collapsed[id] }
</script>

<template>
  <div class="shell">
    <TopBar :health="health" :user="user" @logout="$emit('logout')" />

    <main class="desktop">
      <section
        v-for="w in WIDGETS" :key="w.id"
        :class="['win', w.id, { 'is-alarm': isAlarm(w.id), 'is-wide': w.wide, 'is-rowstart': w.rowstart, 'is-collapsed': collapsed[w.id] }]"
      >
        <div class="win__bar">
          <Led v-if="isAlarm(w.id)" state="bad" pulse />
          <Icon v-else :name="w.icon" :size="15" class="win__icon" />
          <span class="win__title">{{ w.title }}</span>
          <span class="spacer"></span>
          <button class="win__btn" type="button" :title="collapsed[w.id] ? 'Expand' : 'Collapse'"
                  @click="toggleCollapse(w.id)">
            <Icon :name="collapsed[w.id] ? 'chevron' : 'minus'" :size="15" />
          </button>
        </div>
        <div v-show="!collapsed[w.id]" class="win__body">
          <component :is="w.comp" v-bind="propsFor(w.id)" />
        </div>
      </section>
    </main>

    <Dock :apps="APPS" active="dashboard" />
  </div>
</template>

<style scoped>
.shell { position: fixed; inset: 0; display: flex; flex-direction: column; }
</style>
