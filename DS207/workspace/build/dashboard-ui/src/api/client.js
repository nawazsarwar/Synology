// Minimal API client. A 401 never throws a crash: it fires the registered
// unauthorized hook (and a window event) so the app can route to login.
export const TIMEOUT_MS = 5000
let unauthorizedHandler = null

export function onUnauthorized(fn) {
  unauthorizedHandler = fn
}

function notifyUnauthorized() {
  if (unauthorizedHandler) unauthorizedHandler()
  if (typeof window !== 'undefined' && window.dispatchEvent) {
    window.dispatchEvent(new CustomEvent('ds207:unauthorized'))
  }
}

// Relative path (no leading slash) keeps it working from any mount point.
export async function getJSON(path) {
  const url = path.replace(/^\/+/, '')
  // Bound every request so a hung connection can't stall callers (or the login probe).
  const ctl = new AbortController()
  const timer = setTimeout(() => ctl.abort(), TIMEOUT_MS)
  let res
  try {
    res = await fetch(url, { credentials: 'same-origin', signal: ctl.signal })
  } finally {
    clearTimeout(timer)
  }
  if (res.status === 401) {
    notifyUnauthorized()
    return null
  }
  if (!res.ok) throw new Error(`HTTP ${res.status} for ${path}`)
  return res.json()
}

export async function login(password) {
  const res = await fetch('api/login', {
    method: 'POST',
    credentials: 'same-origin',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ password })
  })
  return res.ok
}
