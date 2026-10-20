import { describe, it, expect } from 'vitest'
import { mount } from '@vue/test-utils'
import StorageTile from './StorageTile.vue'
import SmartTile from './SmartTile.vue'
import ServicesTile from './ServicesTile.vue'

describe('StorageTile', () => {
  const vol = [{ mount: '/volume1', size_kb: 1000, used_kb: 500, avail_kb: 500 }]
  it('shows a visible Degraded indicator when state is degraded', () => {
    const w = mount(StorageTile, { props: { state: 'degraded', volumes: vol, arrays: [{ name: 'md2', level: 'raid1', state: 'degraded' }] } })
    expect(w.text()).toContain('Degraded')
    expect(w.find('.degraded').exists()).toBe(true)
  })
  it('does not show Degraded when ok', () => {
    const w = mount(StorageTile, { props: { state: 'ok', volumes: vol, arrays: [{ name: 'md2', level: 'raid1', state: 'ok' }] } })
    expect(w.text()).not.toContain('Degraded')
    expect(w.find('.degraded').exists()).toBe(false)
  })
  it('derives degraded from arrays when no state prop', () => {
    const w = mount(StorageTile, { props: { arrays: [{ name: 'md0', level: 'raid1', state: 'degraded' }] } })
    expect(w.text()).toContain('Degraded')
  })
  it('tolerates missing data', () => {
    expect(() => mount(StorageTile, { props: {} })).not.toThrow()
  })
})

describe('SmartTile', () => {
  it('renders n/a when temp_c is -1', () => {
    const w = mount(SmartTile, { props: { disks: [{ dev: 'sda', model: 'PM830', health: 'ok', temp_c: -1 }] } })
    expect(w.text()).toContain('n/a')
    expect(w.text()).not.toContain('-1')
  })
  it('renders the temperature when present', () => {
    const w = mount(SmartTile, { props: { disks: [{ dev: 'sda', model: 'X', health: 'ok', temp_c: 38 }] } })
    expect(w.text()).toContain('38')
    expect(w.text()).not.toContain('n/a')
  })
})

describe('ServicesTile', () => {
  it('shows up/down and a dash for null version', () => {
    const w = mount(ServicesTile, { props: { services: [{ name: 'smbd', up: true, version: '4.15' }, { name: 'afpd', up: false, version: null }] } })
    expect(w.text()).toContain('4.15')
    expect(w.text()).toContain('—')
  })
})

import WanTile from './WanTile.vue'
describe('WanTile', () => {
  it('shows calm state when no public connections', () => {
    const w = mount(WanTile, { props: { wan: { public_conns: [], public_count: 0, lan_count: 2 } } })
    expect(w.text()).toContain('No connections from outside')
    expect(w.text()).toContain('LAN: 2')
  })
  it('lists public peers', () => {
    const w = mount(WanTile, { props: { wan: { public_conns: [{ local: '192.168.1.11:50000', remote: '8.8.8.8:443' }], public_count: 1, lan_count: 0 } } })
    expect(w.text()).toContain('8.8.8.8:443')
    expect(w.find('.alert').exists()).toBe(true)
  })
  it('tolerates missing data', () => {
    expect(() => mount(WanTile, { props: {} })).not.toThrow()
  })
})
