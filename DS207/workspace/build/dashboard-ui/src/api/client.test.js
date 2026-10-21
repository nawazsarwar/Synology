import { describe, it, expect, vi, afterEach } from 'vitest'
import { getJSON, TIMEOUT_MS } from './client.js'

afterEach(() => { vi.useRealTimers(); vi.unstubAllGlobals() })

describe('getJSON', () => {
  it('returns null on 401 and fires ds207:unauthorized', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue({ status: 401, ok: false }))
    const ev = vi.fn()
    window.addEventListener('ds207:unauthorized', ev)
    expect(await getJSON('api/system')).toBeNull()
    expect(ev).toHaveBeenCalled()
    window.removeEventListener('ds207:unauthorized', ev)
  })
  it('aborts a hung request after the timeout (rejects, never hangs)', async () => {
    vi.useFakeTimers()
    vi.stubGlobal('fetch', vi.fn((u, o) => new Promise((_, rej) => {
      o.signal.addEventListener('abort', () => rej(new DOMException('aborted', 'AbortError')))
    })))
    const p = getJSON('api/system')
    const assertion = expect(p).rejects.toThrow()
    await vi.advanceTimersByTimeAsync(TIMEOUT_MS + 1)
    await assertion
  })
})
