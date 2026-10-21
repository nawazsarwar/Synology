// Shared status mapping: a value's severity -> a design-token color.
// Keeps thresholds and the ok/warn/bad vocabulary in one place for every phase.
export function level(pct, warn = 70, bad = 90) {
  if (!Number.isFinite(pct)) return 'idle'
  if (pct >= bad) return 'bad'
  if (pct >= warn) return 'warn'
  return 'ok'
}

const TOKEN = { ok: 'var(--c-ok)', warn: 'var(--c-warn)', bad: 'var(--c-bad)', idle: 'var(--c-ink-faint)', brand: 'var(--c-brand)' }
export function tokenFor(lvl) { return TOKEN[lvl] || TOKEN.idle }

// Convenience: value -> color, with optional thresholds.
export function colorFor(pct, warn, bad) { return tokenFor(level(pct, warn, bad)) }
