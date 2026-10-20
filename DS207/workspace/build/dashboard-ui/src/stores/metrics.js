import { defineStore } from 'pinia'
import { getJSON } from '../api/client.js'

// Rate from two cumulative counters. Reset (cur < prev) or bad dt => 0.
export function rate(prev, cur, dtSec) {
  if (!(dtSec > 0) || cur < prev) return 0
  return (cur - prev) / dtSec
}

// busy% = 100 * (1 - dIdle/dTotal), clamped 0..100; reset => 0.
export function cpuBusyPct(prev, cur) {
  const dT = cur.cpu_total - prev.cpu_total
  const dI = cur.cpu_idle - prev.cpu_idle
  if (dT <= 0 || dI < 0) return 0
  return Math.min(100, Math.max(0, (1 - dI / dT) * 100))
}

const FAST_MS = 2000
const SERVICES_MS = 60000
const DISKS_MS = 300000
const FAST = ['system', 'net', 'diskio', 'storage', 'sessions']

export const useMetricsStore = defineStore('metrics', {
  state: () => ({
    system: null,
    net: [],
    diskio: [],
    storage: { volumes: [], arrays: [] },
    disks: [],
    services: [],
    sessions: { smb: [], open_files: [], ssh: [] },
    cpuBusy: 0,
    netRates: [],
    diskRates: [],
    error: null
  }),
  getters: {
    memUsedKb: (s) => (s.system ? s.system.mem_total - s.system.mem_free - s.system.mem_cached : 0),
    load: (s) => (s.system ? [s.system.load1, s.system.load5, s.system.load15] : [0, 0, 0])
  },
  actions: {
    // Ingest one snapshot taken at time tMs; derives rates vs. previous one.
    ingest(name, data, tMs = Date.now()) {
      this._t = this._t || {}
      const prevT = this._t[name]
      const dt = prevT === undefined ? 0 : (tMs - prevT) / 1000
      this._t[name] = tMs
      if (name === 'system') {
        const prev = this.system
        this.cpuBusy = prev && dt > 0 ? cpuBusyPct(prev, data) : 0
        this.system = data
      } else if (name === 'net') {
        const prev = new Map(this.net.map((n) => [n.iface, n]))
        this.netRates = data.map((n) => {
          const p = prev.get(n.iface)
          return {
            iface: n.iface,
            rx_bps: p ? rate(p.rx_bytes, n.rx_bytes, dt) : 0,
            tx_bps: p ? rate(p.tx_bytes, n.tx_bytes, dt) : 0
          }
        })
        this.net = data
      } else if (name === 'diskio') {
        const prev = new Map(this.diskio.map((d) => [d.dev, d]))
        this.diskRates = data.map((d) => {
          const p = prev.get(d.dev)
          return {
            dev: d.dev,
            rd_sps: p ? rate(p.rd_sectors, d.rd_sectors, dt) : 0,
            wr_sps: p ? rate(p.wr_sectors, d.wr_sectors, dt) : 0
          }
        })
        this.diskio = data
      } else {
        this[name] = data
      }
    },
    async fetchOne(name) {
      try {
        const data = await getJSON(`api/${name}`)
        if (data !== null) {
          this.ingest(name, data)
          this.error = null
        }
      } catch (e) {
        this.error = String(e.message || e)
      }
    },
    startPolling() {
      this.stopPolling()
      const timers = []
      const every = (names, ms) => {
        const run = () => names.forEach((n) => this.fetchOne(n))
        run()
        timers.push(setInterval(run, ms))
      }
      every(FAST, FAST_MS)
      every(['services'], SERVICES_MS)
      every(['disks'], DISKS_MS)
      this._timers = timers
    },
    stopPolling() {
      ;(this._timers || []).forEach(clearInterval)
      this._timers = []
    }
  }
})
