import { describe, it, expect, beforeEach } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'
import { useMetricsStore } from './metrics.js'

beforeEach(() => setActivePinia(createPinia()))

describe('net rates', () => {
  it('computes bytes/s from successive cumulative samples', () => {
    const s = useMetricsStore()
    s.ingest('net', [{ iface: 'eth0', rx_bytes: 1000, tx_bytes: 500 }], 10000)
    s.ingest('net', [{ iface: 'eth0', rx_bytes: 5000, tx_bytes: 1500 }], 12000)
    expect(s.netRates).toEqual([{ iface: 'eth0', rx_bps: 2000, tx_bps: 500 }])
  })
  it('counter reset yields 0, never negative', () => {
    const s = useMetricsStore()
    s.ingest('net', [{ iface: 'eth0', rx_bytes: 9000, tx_bytes: 9000 }], 0)
    s.ingest('net', [{ iface: 'eth0', rx_bytes: 100, tx_bytes: 50 }], 2000)
    expect(s.netRates[0].rx_bps).toBe(0)
    expect(s.netRates[0].tx_bps).toBe(0)
  })
  it('has no rate until two samples', () => {
    const s = useMetricsStore()
    s.ingest('net', [{ iface: 'eth0', rx_bytes: 1, tx_bytes: 1 }], 0)
    expect(s.netRates[0].rx_bps).toBe(0)
  })
})

describe('diskio rates', () => {
  it('computes sectors/s and resets to 0', () => {
    const s = useMetricsStore()
    s.ingest('diskio', [{ dev: 'sda', rd_sectors: 100, wr_sectors: 200 }], 0)
    s.ingest('diskio', [{ dev: 'sda', rd_sectors: 300, wr_sectors: 100 }], 2000)
    expect(s.diskRates[0]).toEqual({ dev: 'sda', rd_sps: 100, wr_sps: 0 })
  })
})

describe('cpu busy %', () => {
  const sys = (t, i) => ({ cpu_total: t, cpu_idle: i, mem_total: 1, mem_free: 1, mem_cached: 0, load1: 0, load5: 0, load15: 0, uptime_s: 1 })
  it('busy = 1 - dIdle/dTotal', () => {
    const s = useMetricsStore()
    s.ingest('system', sys(1000, 800), 0)
    s.ingest('system', sys(1200, 850), 2000) // dTotal 200, dIdle 50 -> 75%
    expect(s.cpuBusy).toBeCloseTo(75, 5)
  })
  it('cpu reset yields 0', () => {
    const s = useMetricsStore()
    s.ingest('system', sys(1000, 800), 0)
    s.ingest('system', sys(100, 50), 2000)
    expect(s.cpuBusy).toBe(0)
  })
})
