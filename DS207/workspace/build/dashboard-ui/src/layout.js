export const KEY = 'ds207.layout.v1'
export const DEFAULTS = {
  cpu: { x: 16, y: 16, w: 260, h: 170 },
  mem: { x: 292, y: 16, w: 260, h: 170 },
  load: { x: 568, y: 16, w: 260, h: 170 },
  storage: { x: 16, y: 202, w: 400, h: 240 },
  net: { x: 432, y: 202, w: 396, h: 240 },
  diskio: { x: 844, y: 202, w: 340, h: 240 },
  smart: { x: 844, y: 16, w: 340, h: 170 },
  services: { x: 16, y: 458, w: 300, h: 230 },
  sessions: { x: 332, y: 458, w: 420, h: 230 },
  files: { x: 768, y: 458, w: 416, h: 230 },
  wan: { x: 1200, y: 16, w: 300, h: 230 }
}
export const MIN_W = 180
export const MIN_H = 110

export function clamp(r, vw, vh) {
  const w = Math.min(Math.max(r.w, MIN_W), Math.max(vw, MIN_W))
  const h = Math.max(r.h, MIN_H)
  const x = Math.min(Math.max(r.x, 0), Math.max(vw - w, 0))
  const y = Math.max(r.y, 0)
  return { x, y, w, h }
}

const num = (v) => typeof v === 'number' && Number.isFinite(v)
const valid = (r) => r && num(r.x) && num(r.y) && num(r.w) && num(r.h)

// Never throws: storage may be missing, blocked, or hold junk.
export function loadLayout() {
  const out = {}
  for (const k in DEFAULTS) out[k] = { ...DEFAULTS[k] }
  try {
    const raw = localStorage.getItem(KEY)
    if (!raw) return out
    const saved = JSON.parse(raw)
    for (const k in out) if (valid(saved && saved[k])) out[k] = { ...saved[k] }
  } catch (e) { /* defaults */ }
  return out
}

export function saveLayout(layout) {
  try { localStorage.setItem(KEY, JSON.stringify(layout)) } catch (e) { /* ignore */ }
}

export function clearLayout() {
  try { localStorage.removeItem(KEY) } catch (e) { /* ignore */ }
}
