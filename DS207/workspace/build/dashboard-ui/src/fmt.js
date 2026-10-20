export const dash = '—'
export function bytes(n) {
  if (!Number.isFinite(n)) return dash
  const u = ['B', 'KB', 'MB', 'GB', 'TB']
  let i = 0
  while (Math.abs(n) >= 1024 && i < u.length - 1) { n /= 1024; i++ }
  return (i === 0 ? Math.round(n) : n.toFixed(1)) + ' ' + u[i]
}
export function kb(n) { return Number.isFinite(n) ? bytes(n * 1024) : dash }
export function pct(n) { return Number.isFinite(n) ? n.toFixed(0) + '%' : dash }
export function uptime(s) {
  if (!Number.isFinite(s)) return dash
  const d = Math.floor(s / 86400), h = Math.floor((s % 86400) / 3600), m = Math.floor((s % 3600) / 60)
  return (d ? d + 'd ' : '') + h + 'h ' + m + 'm'
}
