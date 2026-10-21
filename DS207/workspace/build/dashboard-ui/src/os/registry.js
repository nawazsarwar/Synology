// =============================================================================
// Application + widget registry — the extension point for every future phase.
// Adding a monitoring widget (P1), a control app (P2), or an admin console view
// (P3) means adding an entry HERE; the shell renders from this file and nothing
// in the layout or design system changes. The dashboard uses a fluid CSS grid,
// so a widget only declares its component, chrome, and whether it wants to span
// two columns on wider screens (`wide`).
// =============================================================================
import CpuTile from '../components/tiles/CpuTile.vue'
import MemTile from '../components/tiles/MemTile.vue'
import LoadTile from '../components/tiles/LoadTile.vue'
import SmartTile from '../components/tiles/SmartTile.vue'
import StorageTile from '../components/tiles/StorageTile.vue'
import NetTile from '../components/tiles/NetTile.vue'
import DiskIoTile from '../components/tiles/DiskIoTile.vue'
import ServicesTile from '../components/tiles/ServicesTile.vue'
import SessionsTile from '../components/tiles/SessionsTile.vue'
import FilesTile from '../components/tiles/FilesTile.vue'
import WanTile from '../components/tiles/WanTile.vue'

// Reading order. `wide: true` lets a widget span two grid columns when at least
// two fit; it collapses to one on narrow screens (handled in CSS).
export const WIDGETS = [
  { id: 'cpu', title: 'Processor', icon: 'cpu', comp: CpuTile },
  { id: 'mem', title: 'Memory', icon: 'memory', comp: MemTile },
  { id: 'load', title: 'Load & uptime', icon: 'activity', comp: LoadTile },
  { id: 'smart', title: 'Disk health', icon: 'thermometer', comp: SmartTile },
  { id: 'storage', title: 'Storage & RAID', icon: 'storage', comp: StorageTile, wide: true },
  { id: 'net', title: 'Network', icon: 'network', comp: NetTile },
  { id: 'diskio', title: 'Disk activity', icon: 'disk', comp: DiskIoTile },
  { id: 'services', title: 'Services', icon: 'server', comp: ServicesTile, rowstart: true },
  { id: 'sessions', title: 'Sessions', icon: 'users', comp: SessionsTile },
  { id: 'files', title: 'Open files', icon: 'file', comp: FilesTile },
  { id: 'wan', title: 'Beyond LAN', icon: 'shield', comp: WanTile }
]

// Dock apps. Only the Dashboard ships in P1; the rest advertise the roadmap and
// get wired on in later phases (flip `enabled` + point at the app's view).
export const APPS = [
  { id: 'dashboard', name: 'Dashboard', icon: 'dashboard', enabled: true },
  { id: 'files', name: 'File Browser', icon: 'file', enabled: false, soon: true },
  { id: 'storage', name: 'Storage Manager', icon: 'storage', enabled: false, soon: true },
  { id: 'network', name: 'Network Center', icon: 'network', enabled: false, soon: true },
  { id: 'control', name: 'Control Panel', icon: 'server', enabled: false, soon: true }
]
