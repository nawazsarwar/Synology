import { describe, it, expect, beforeEach, vi } from 'vitest'
import { mount, flushPromises } from '@vue/test-utils'
import { setActivePinia, createPinia } from 'pinia'
vi.mock('uplot', () => ({ default: class { setData() {} setSize() {} destroy() {} } }))
vi.mock('uplot/dist/uPlot.min.css', () => ({}))
vi.mock('./api/client.js', () => ({ login: vi.fn(), getJSON: vi.fn(), onUnauthorized: vi.fn() }))
import * as client from './api/client.js'
import App from './App.vue'
import { useMetricsStore } from './stores/metrics.js'

let pinia, store
beforeEach(() => {
  vi.clearAllMocks()
  pinia = createPinia()
  setActivePinia(pinia)
  store = useMetricsStore()
  vi.spyOn(store, 'startPolling').mockImplementation(() => {})
  vi.spyOn(store, 'stopPolling').mockImplementation(() => {})
  client.getJSON.mockResolvedValue(null) // session probe: not logged in
})

const mountApp = async () => {
  const w = mount(App, { global: { plugins: [pinia] } })
  await flushPromises()
  return w
}
const submit = async (w, pw) => {
  await w.find('input[type=password]').setValue(pw)
  await w.find('form').trigger('submit')
  await flushPromises()
}

describe('auth gate', () => {
  it('unauthenticated: shows Login, no dashboard, no polling', async () => {
    const w = await mountApp()
    expect(w.find('form.login').exists()).toBe(true)
    expect(w.find('.desktop').exists()).toBe(false)
    expect(store.startPolling).not.toHaveBeenCalled()
  })
  it('correct password calls login, shows dashboard, starts polling', async () => {
    client.login.mockResolvedValue(true)
    const w = await mountApp()
    await submit(w, 'secret')
    expect(client.login).toHaveBeenCalledWith('secret')
    expect(w.find('form.login').exists()).toBe(false)
    expect(w.find('.desktop').exists()).toBe(true)
    expect(store.startPolling).toHaveBeenCalledTimes(1)
  })
  it('wrong password shows error and stays on Login', async () => {
    client.login.mockResolvedValue(false)
    const w = await mountApp()
    await submit(w, 'nope')
    expect(w.find('.login-error').text()).toMatch(/incorrect/i)
    expect(w.find('.desktop').exists()).toBe(false)
    expect(store.startPolling).not.toHaveBeenCalled()
  })
  it('network error shows error, no unhandled rejection', async () => {
    client.login.mockRejectedValue(new TypeError('Failed to fetch'))
    const w = await mountApp()
    await submit(w, 'x')
    expect(w.find('.login-error').text()).toMatch(/reach|network|connect/i)
    expect(w.find('.desktop').exists()).toBe(false)
  })
  it('ds207:unauthorized returns to Login and stops polling', async () => {
    client.login.mockResolvedValue(true)
    const w = await mountApp()
    await submit(w, 'secret')
    window.dispatchEvent(new CustomEvent('ds207:unauthorized'))
    await flushPromises()
    expect(store.stopPolling).toHaveBeenCalled()
    expect(w.find('form.login').exists()).toBe(true)
    expect(w.find('.desktop').exists()).toBe(false)
  })
  it('logout stops polling and returns to Login', async () => {
    client.login.mockResolvedValue(true)
    const w = await mountApp()
    await submit(w, 'secret')
    await w.find('button.logout').trigger('click')
    await flushPromises()
    expect(store.stopPolling).toHaveBeenCalled()
    expect(w.find('form.login').exists()).toBe(true)
  })
  it('existing valid session skips Login', async () => {
    client.getJSON.mockResolvedValue({ ok: 1 })
    const w = await mountApp()
    expect(w.find('.desktop').exists()).toBe(true)
    expect(store.startPolling).toHaveBeenCalledTimes(1)
  })
})
