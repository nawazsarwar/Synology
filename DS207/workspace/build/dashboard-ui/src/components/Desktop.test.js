import { describe, it, expect, beforeEach, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import { setActivePinia, createPinia } from 'pinia'
vi.mock('uplot', () => ({ default: class { setData() {} setSize() {} destroy() {} } }))
vi.mock('uplot/dist/uPlot.min.css', () => ({}))
import Desktop from './Desktop.vue'
import { useMetricsStore } from '../stores/metrics.js'

beforeEach(() => { setActivePinia(createPinia()); vi.unstubAllGlobals() })

describe('Desktop', () => {
  it('renders all 11 tiles with empty store and no localStorage', () => {
    vi.stubGlobal('localStorage', { getItem() { throw new Error('x') }, setItem() { throw new Error('x') } })
    const w = mount(Desktop)
    expect(w.findAll('.win').length).toBe(11)
  })
  it('flags degraded RAID in the top-bar annunciator', () => {
    const s = useMetricsStore()
    s.storage = { volumes: [], arrays: [{ name: 'md0', level: 'raid1', state: 'degraded' }] }
    const w = mount(Desktop)
    expect(w.find('.annunciator.is-bad').exists()).toBe(true)
    expect(w.text()).toContain('RAID degraded')
    expect(w.text()).toContain('Degraded')
  })
})
