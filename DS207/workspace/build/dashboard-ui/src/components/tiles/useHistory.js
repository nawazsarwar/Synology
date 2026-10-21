import { reactive } from 'vue'
// Fixed-length ring of recent values per key; replaces array so watchers fire.
export function useHistory(len = 60) {
  const h = reactive({})
  return {
    h,
    push(key, v) {
      const a = (h[key] || new Array(len).fill(0)).slice(1)
      a.push(Number.isFinite(v) ? v : 0)
      h[key] = a
    }
  }
}
