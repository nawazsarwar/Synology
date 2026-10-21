// Theme: dark / light / auto. We resolve 'auto' to a concrete theme in JS and
// set data-theme="dark|light" on <html>, so the CSS only needs a default (dark)
// and a single [data-theme="light"] override — no media-query duplication.
const KEY = 'ds207.theme'
export const THEMES = ['dark', 'light', 'auto']

const mql = () => (typeof matchMedia === 'function' ? matchMedia('(prefers-color-scheme: light)') : null)

export function getPref() {
  try { const v = localStorage.getItem(KEY); return THEMES.includes(v) ? v : 'auto' } catch (e) { return 'auto' }
}
export function resolve(pref) {
  if (pref === 'light' || pref === 'dark') return pref
  const m = mql()
  return m && m.matches ? 'light' : 'dark'
}
export function apply(pref = getPref()) {
  if (typeof document !== 'undefined') document.documentElement.setAttribute('data-theme', resolve(pref))
}
export function setPref(pref) {
  try { localStorage.setItem(KEY, pref) } catch (e) { /* ignore */ }
  apply(pref)
}
// Re-apply when the OS theme flips, but only while the user is on 'auto'.
export function watchSystem() {
  const m = mql()
  if (!m) return
  const onChange = () => { if (getPref() === 'auto') apply('auto') }
  if (m.addEventListener) m.addEventListener('change', onChange)
  else if (m.addListener) m.addListener(onChange)
}
