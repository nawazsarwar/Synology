import { describe, it, expect, afterEach, vi } from 'vitest'
import { loadLayout, saveLayout, clamp, KEY, DEFAULTS } from './layout.js'

afterEach(() => { vi.unstubAllGlobals(); localStorage.clear() })

describe('layout persistence', () => {
  it('round-trips', () => {
    const l = loadLayout(); l.cpu.x = 99
    saveLayout(l)
    expect(loadLayout().cpu.x).toBe(99)
  })
  it('falls back to defaults when storage throws', () => {
    vi.stubGlobal('localStorage', { getItem() { throw new Error('blocked') }, setItem() { throw new Error('blocked') } })
    expect(loadLayout()).toEqual(DEFAULTS)
    expect(() => saveLayout(DEFAULTS)).not.toThrow()
  })
  it('ignores corrupt JSON and invalid entries', () => {
    localStorage.setItem(KEY, '{nope')
    expect(loadLayout()).toEqual(DEFAULTS)
    localStorage.setItem(KEY, JSON.stringify({ cpu: { x: 'a' }, mem: { x: 1, y: 2, w: 300, h: 150 } }))
    const l = loadLayout()
    expect(l.cpu).toEqual(DEFAULTS.cpu)
    expect(l.mem.x).toBe(1)
  })
})

describe('clamp', () => {
  it('keeps rect in viewport', () => {
    expect(clamp({ x: 2000, y: -5, w: 300, h: 200 }, 1000, 800)).toEqual({ x: 700, y: 0, w: 300, h: 200 })
  })
})
