<script setup>
import { reactive, ref, computed, onMounted, onBeforeUnmount } from 'vue'
import { useMetricsStore } from '../stores/metrics.js'
import { loadLayout, saveLayout, clearLayout, clamp, DEFAULTS } from '../layout.js'
import CpuTile from './tiles/CpuTile.vue'
import MemTile from './tiles/MemTile.vue'
import LoadTile from './tiles/LoadTile.vue'
import NetTile from './tiles/NetTile.vue'
import DiskIoTile from './tiles/DiskIoTile.vue'
import StorageTile from './tiles/StorageTile.vue'
import SmartTile from './tiles/SmartTile.vue'
import SessionsTile from './tiles/SessionsTile.vue'
import FilesTile from './tiles/FilesTile.vue'
import ServicesTile from './tiles/ServicesTile.vue'

defineEmits(['logout'])
const m = useMetricsStore()
const layout = reactive(loadLayout())
const desk = ref(null)
const vw = ref(typeof window !== 'undefined' ? window.innerWidth : 1280)
const narrow = computed(() => vw.value < 760)
const top = ref(1)
const z = reactive({})

// WanTile (Task 17) mounts here: add { id:'wan', ... } and its component.
const tiles = computed(() => [
  { id: 'cpu', title: 'CPU', comp: CpuTile, props: { busy: m.system ? m.cpuBusy : NaN } },
  { id: 'mem', title: 'Memory', comp: MemTile, props: { system: m.system } },
  { id: 'load', title: 'Load & uptime', comp: LoadTile, props: { load: m.system ? m.load : [NaN, NaN, NaN], uptimeS: m.system ? m.system.uptime_s : NaN } },
  { id: 'storage', title: 'Storage', comp: StorageTile, props: { volumes: m.storage.volumes || [], arrays: m.storage.arrays || [] } },
  { id: 'net', title: 'Network', comp: NetTile, props: { rates: m.netRates } },
  { id: 'diskio', title: 'Disk I/O', comp: DiskIoTile, props: { rates: m.diskRates } },
  { id: 'smart', title: 'Disk health (SMART)', comp: SmartTile, props: { disks: m.disks } },
  { id: 'services', title: 'Services', comp: ServicesTile, props: { services: m.services } },
  { id: 'sessions', title: 'Sessions', comp: SessionsTile, props: { sessions: m.sessions } },
  { id: 'files', title: 'Open files', comp: FilesTile, props: { files: m.sessions.open_files || [] } }
])
const degraded = computed(() => (m.storage.arrays || []).some((a) => a.state === 'degraded'))
const offline = computed(() => !!m.error)

function bounds() { return { w: desk.value ? desk.value.clientWidth : vw.value, h: 99999 } }
function raise(id) { z[id] = ++top.value }

function start(e, id, mode) {
  if (narrow.value) return
  e.preventDefault()
  raise(id)
  const o = { ...layout[id] }, sx = e.clientX, sy = e.clientY
  const move = (ev) => {
    const dx = ev.clientX - sx, dy = ev.clientY - sy
    const b = bounds()
    layout[id] = clamp(mode === 'drag' ? { ...o, x: o.x + dx, y: o.y + dy } : { ...o, w: o.w + dx, h: o.h + dy }, b.w, b.h)
  }
  const up = () => {
    window.removeEventListener('pointermove', move)
    window.removeEventListener('pointerup', up)
    window.removeEventListener('pointercancel', up)
    saveLayout(layout)
  }
  window.addEventListener('pointermove', move)
  window.addEventListener('pointerup', up)
  window.addEventListener('pointercancel', up)
}

function reset() {
  clearLayout()
  for (const k in DEFAULTS) layout[k] = { ...DEFAULTS[k] }
}
const onResize = () => {
  vw.value = window.innerWidth
  const b = bounds()
  for (const k in layout) layout[k] = clamp(layout[k], b.w, b.h)
}
onMounted(() => { window.addEventListener('resize', onResize); onResize() })
onBeforeUnmount(() => window.removeEventListener('resize', onResize))

const height = computed(() => Math.max(0, ...Object.values(layout).map((r) => r.y + r.h)) + 16)
function style(id) {
  if (narrow.value) return {}
  const r = layout[id]
  return { left: r.x + 'px', top: r.y + 'px', width: r.w + 'px', height: r.h + 'px', zIndex: z[id] || 1 }
}
</script>

<template>
  <div class="desktop-wrap">
    <header class="taskbar">
      <b>DS207</b>
      <span v-if="degraded" class="pill bad">⚠ RAID degraded</span>
      <span v-if="offline" class="pill warn" :title="m.error">connection issue</span>
      <span class="spacer"></span>
      <button v-if="!narrow" @click="reset">Reset layout</button>
      <button class="logout" @click="$emit('logout')">Log out</button>
    </header>
    <div ref="desk" :class="['desktop', { narrow }]" :style="narrow ? {} : { height: height + 'px' }">
      <section v-for="t in tiles" :key="t.id" :class="['win', t.id, { alarm: t.id === 'storage' && degraded }]" :style="style(t.id)" @pointerdown="raise(t.id)">
        <div class="title" @pointerdown="start($event, t.id, 'drag')">{{ t.title }}</div>
        <div class="body"><component :is="t.comp" v-bind="t.props" /></div>
        <div v-if="!narrow" class="grip" @pointerdown.stop="start($event, t.id, 'resize')"></div>
      </section>
      <!-- slot: WanTile (Task 17) -->
      <slot name="extra" :layout="layout" />
    </div>
  </div>
</template>
