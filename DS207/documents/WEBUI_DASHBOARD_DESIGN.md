# Design — DS207 Modern Monitoring Dashboard ("desktop-OS" landing)

Status: IMPLEMENTED & DEPLOYED (Phase 1) · Date: 2026-10-21 · see the
[As-implemented](#as-implemented-2026-10-21) addendum for how the shipped UI differs from this
original design.

## Intent & why

The DS207's stock web UI (DSM 3.1, ExtJS 1.1.1 on Apache 2.2) is dated and gives a poor
at-a-glance picture of the machine's health. The owner wants a modern, good-looking
**monitoring dashboard** — a desktop-OS-style landing page with draggable widget tiles —
that shows the real-time health and activity of the NAS, including the modern Samba/SSH/curl
stack we added (which the stock UI cannot see).

This is **monitoring**, not administration. It does not replace DSM. It runs *alongside* the
stock UI, exactly like the Samba/SSH/curl modules run alongside their stock counterparts:
stock is never modified, ours is a reversible overlay, and the two coexist.

## Goals

1. A single-page **dashboard** reachable in a browser on the LAN, showing live:
   - CPU %, RAM, load average, uptime, model/kernel
   - Network throughput per interface (in/out, live graph)
   - Disk I/O throughput (read/write)
   - Storage: per-volume usage + **RAID/array health** (`/proc/mdstat`)
   - Per-disk **SMART health + temperature**
   - **Connected users** — SMB sessions (our modern Samba) + SSH/console (`who`)
   - **Active files** being read/written (open files from our Samba)
   - **Service status + versions** — smbd, nmbd, dropbear, curl, and key stock DSM services
   - **WAN (beyond-LAN) traffic** — see Open Risk below
2. A **desktop-OS feel**: draggable, rearrangeable widget tiles on a "desktop"; layout
   persists per browser.
3. Installed/removed/reverted through the **same installer** as the other modules, with the
   same safety guarantees (stock UI untouched, fully reversible).

## Non-goals (v1)

- Not a DSM replacement: no user/share/network admin, no File Station, no Control Panel.
- **Read-only**: v1 performs no write/control actions on the NAS (no reboot, no config edits).
- Not internet-facing: LAN-only, same as the rest of the project.
- No historical database / long-term graphing in v1 (live + short in-memory window only).

## Anti-goals (these would make it a failure even if it "works")

- Touching or risking the stock DSM UI / Apache / system partition.
- Any path where the 266 MHz CPU is pegged or disks are kept spinning by the dashboard's
  own polling (must not degrade the NAS it monitors).
- Any command-injection surface (see Security).

## Constraints (the box)

- PowerPC `mpc824x` 603e, **big-endian**, 266 MHz, **64 MB RAM**, kernel **2.6.24**.
- Build: `zig cc -target powerpc-linux-musl`, static, with the existing `syscompat.c` shim
  for 2.6.24 ABI safety (reused — same as Samba/Dropbear/curl).
- **No busybox `httpd`**; only web server present is DSM's own Apache (must stay untouched).
- **No `conntrack`** (`/proc/net/nf_conntrack` and `ip_conntrack` absent) → WAN byte-split is
  the one hard metric.
- Stock `smbstatus` exists but is **blind to our modern smbd** (separate state dirs).
- Stock `smartctl` is **v5.38 (2008)** — it does **not** honor `-n standby` (it queries the
  disk regardless). Moot here: the installed drive is a **Samsung PM830 SSD** (no spin-up
  cost), so a long SMART cache is enough to satisfy the "don't keep disks busy" anti-goal.
  (If a spinning disk is ever fitted, cross-compiling modern smartmontools 7.x — which can
  skip standby disks — is a later option.)
- Everything installs under `/volume1/@modernsmb/` (data volume; survives DSM reinstall).

## Architecture (decided)

Shipped as one new installer module `ui`, built from a **privilege-separated** pair plus the
SPA. This split (adopted after design review) keeps the network-facing process unprivileged
and shell-free — the single most important security decision here.

### 1. `collector` — root, NOT on the network (new build)
- Small static musl C binary (PPC BE, project toolchain + `syscompat.o`).
- Run periodically from the boot hook / a loop as root, but **never binds a socket**.
- Gathers metrics and writes **JSON snapshot files** into `/volume1/@modernsmb/ui/state/`:
  - `/proc/stat`, `/proc/meminfo`, `/proc/loadavg`, `/proc/uptime`
  - `/proc/net/dev`, `/proc/diskstats`, `/proc/mdstat`, `df`
  - `/usr/syno/bin/smartctl` per-disk health/temp — **cached long** (see SMART note below)
  - our-Samba `smbstatus` (sessions + open files)
  - `who` (SSH/console)
  - service probes: `ps`/`netstat` up/down + fixed `--version`/`-V` calls
- Different metrics refresh on different cadences (light ~2 s; SMART minutes; services ~60 s),
  each a separate snapshot file so the fast path never waits on a slow probe.
- **Fixed whitelist of commands with fixed arguments only** — no request data ever reaches it
  (it isn't on the network at all), so there is no injection surface.

### 2. `uiserver` — unprivileged, network-facing (new build)
- Small static musl C binary; **drops privilege** (runs as `nobody`/non-root).
- Listens on **:8181** (own port; DSM UI on :5000 never touched).
- Does only two things: serve the static SPA from `/volume1/@modernsmb/ui/www/`, and serve the
  pre-written JSON snapshot files from `.../ui/state/` under `GET /api/*`.
- **Never execs a command, never reads `/proc`, never has root.** It is a static file server
  with auth. A compromise of it yields read-only monitoring JSON, nothing more.
- Handles login/session (see Security) and gates `/api/*` + the SPA behind it.

### 3. our-Samba `smbstatus` (new build, used by the collector)
- Built from the Samba 4.15.13 tree we already compiled, pointed at
  `/volume1/@modernsmb` state dirs, so it reports *our* smbd's sessions/locks.
- Confirmed low-risk: the state dir already holds populated 4.x tdbs
  (`smbXsrv_session_global.tdb`, `smbXsrv_open_global.tdb`, `smbXsrv_tcon_global.tdb`,
  `locking.tdb`, `leases.tdb`), so a standalone `smbstatus -s <our conf>` should report
  sessions + open files. Verify on the live box during implementation.

### 4. Vue 3 SPA (built on dev machine, shipped static)
- Vue 3 (Composition API) + **Pinia** (state) + **uPlot** (tiny live time-series charts) +
  a small drag library for the desktop-OS tiles.
- Built with **Vite** on the dev machine; only the static `dist/` ships to the NAS. The
  NAS never runs Node or a bundler. The browser (modern client) runs Vue — the box's age is
  irrelevant to the framework.
- Live tiles **poll** `/api` (~2 s for light metrics; SMART/services on their cache cadence).
  SSE push is a possible later optimization, not v1.
- Desktop-OS layout (tile positions/sizes) persisted in browser `localStorage`
  (per-viewer convenience; wrapped in try/catch; dashboard renders fine without it).

### Installer integration
- `install.sh` gains an **`ui`** module (menu + CLI): install / uninstall.
- `modernsmb.sh` gains `ui-start`, `ui-stop`, and a `ui` line in `status` (starts both the
  `collector` loop and `uiserver`).
- Boot hook starts the UI alongside the others.
- **No watchdog for the UI, and it is fully independent of the Samba revert path.** Unlike
  Samba, the dashboard is non-essential and read-only; if it crashes it must *never* trigger
  the Samba auto-revert. A plain backgrounded daemon (restart on next boot) is sufficient.
- **Switch semantics** (what the owner asked for): ours lives on :8181; the stock DSM UI on
  :5000 is never modified, so it is always available as the "original". `uninstall` removes
  our module cleanly; the stock UI is untouched throughout.

## Security (decided)

- **LAN-only** — documented like the rest; do not port-forward.
- **Privilege separation** (above): the only network-facing process (`uiserver`) runs
  unprivileged and never execs a command; the root `collector` is never on the network.
- **Password auth** set at install (independent of DSM). Stored hashed under
  `/volume1/@modernsmb/ui/etc/`. Login yields a session cookie/token; all `/api/*` require it.
- **Read-only**: no API endpoint changes NAS state in v1.
- **No command injection**: only the offline `collector` runs commands, from a **fixed
  whitelist with fixed arguments**; no request data is ever passed to a shell.

### Transport (decided) — plain HTTP on :8181 for v1
The dashboard serves over **plain HTTP** on the LAN in v1 (no handshake cost on the 266 MHz
CPU, consistent with the project's LAN-only stance; the data behind it is read-only
monitoring). Accepted residual risk: a LAN sniffer could capture the dashboard password/
cookie. **HTTPS (mbedTLS, already built for curl) is a later opt-in toggle** and the
`uiserver` design leaves room for it without redesign.

## API (initial shape — final schemas left to builder)

| Endpoint | Returns |
|---|---|
| `POST /api/login` | sets auth cookie from the install password |
| `GET /api/system` | cpu %, mem, load, uptime, model, kernel |
| `GET /api/net` | per-iface cumulative bytes in/out (client computes rate) |
| `GET /api/diskio` | per-disk read/write sectors (client computes rate) |
| `GET /api/storage` | volumes (df) + RAID state (mdstat) |
| `GET /api/disks` | per-disk SMART health + temp (cached) |
| `GET /api/sessions` | SMB users + open files (our smbstatus) + SSH (who) |
| `GET /api/services` | name / up-down / version for modern + key stock services |
| `GET /api/wan` | WAN in/out — see Open Risk |

## Open risk / spike (WAN traffic)

No `conntrack` on the box, so a clean LAN-vs-internet **byte** split isn't available out of
the box. A short spike will decide between:
- **(a)** add iptables byte-accounting rules for non-RFC1918 traffic → real WAN byte totals,
  at the cost of adding firewall rules (must verify `iptables` present + rule persistence); or
- **(b)** parse `/proc/net/tcp` to list current remote peers and flag any public-IP
  connection → lightweight, shows *connections*, not byte totals.
Default if the spike is inconclusive: ship **(b)** in v1 (connections + public-IP flag),
label WAN byte-totals as a later enhancement.

## What's left to the builder

- Final `/api` payload schemas and the caching cadences.
- The WAN spike outcome (a vs b).
- Confirming `uPlot` + the chosen drag library (swap for equivalents if they fit better).
- The exact desktop-OS visual (tile set, grid, theme) — a clickable mockup to be produced and
  reviewed during implementation.
- Whether live updates stay on polling or move to SSE (polling is the v1 default).

## Roadmap (phased vision)

The owner's long-term intent is for this custom front-end to **grow slowly into a full
replacement** for the stock DSM top UI — a real control panel — not stay a read-only
dashboard forever. It is built in phases so each one is small and provably working before the
next; the own-port, overlay, stock-untouched architecture is chosen precisely so it can grow
without ever risking the stock UI, and so we can stop at any phase if it's enough.

- **Phase 1 (this design):** read-only monitoring dashboard, desktop-OS landing. Low risk,
  low weight, proves the architecture (collector + uiserver + Vue SPA + installer module).
- **Phase 2 (later, if desired):** control actions (restart our services, disconnect a
  session, etc.) — introduces authenticated *write* endpoints and privilege boundaries.
- **Phase 3 (later, if desired):** admin surfaces (users/shares/network) reimplemented against
  reverse-engineered DSM C-CGI endpoints or our own back-end, moving toward a full console.
- Growth is opt-in and incremental; Phase 1 stands on its own and the stock DSM UI remains the
  fallback for everything not yet built.

## As-implemented (2026-10-21)

Phase 1 is built, deployed to the real DS207, and verified serving live data at
`http://<nas-ip>:8181`. The architecture above shipped as designed (privilege-separated
`collector` + `uiserver` + static Vue SPA, own port, reversible overlay, stock UI untouched).
This section records where the shipped UI **departs from** the original design and what was
added during implementation.

![DS207 modern monitoring dashboard — as shipped](images/DS207-modern-interface.png)

### Departures from the original design

- **Fluid CSS grid replaced the draggable, rearrangeable tiles** (Goal 2). The freeform
  drag/absolute-position model fought responsiveness: a hand-rolled layout could not reflow
  cleanly across viewport widths. The shipped layout is a native CSS grid
  (`repeat(auto-fill, minmax(min(100%, 330px), 1fr))`) that reflows **4 → 3 → 2 → 1 columns**
  purely by viewport width — no JavaScript layout math, no breakpoints, no horizontal overflow.
  Per-browser `localStorage` layout persistence is therefore dropped for v1; drag-to-reorder
  can return later as an enhancement on top of the grid.
- **The `uiserver` no longer serves its own login page.** Early builds had a hardcoded HTML
  login baked into the C server that gated the whole app; it was the bare, unstyled page the
  owner rejected. The server now serves the SPA publicly and gates only `/api/*` (401 when
  unauthenticated); the **Vue SPA renders its own login** and POSTs to `/api/login`.

### Added during implementation (beyond the original design)

- **An enduring design system**, not a one-off skin — chosen because this front-end is meant
  to grow across phases (see Roadmap) and the baseline must not be re-designed each phase:
  - design tokens (`src/design/tokens.css`): palette, type scale, spacing, radius, elevation,
    motion, z-layers, with a full light-theme override block;
  - a base layer (reset, wallpaper, a11y, scrollbars) and a shared component stylesheet;
  - a dependency-free primitive library (`src/ui/`: Icon, Led, Badge, Stat, Gauge, Meter,
    Sparkline) and an app-shell layer (`src/os/`: TopBar, Dock, theme controller, widget
    registry) that the tiles are composed from.
- **Dark / light / auto theme switcher** (in the user menu), resolved before first paint and
  following the OS while on auto; persisted in `localStorage`.
- **A username field** on login (default `admin`), sent to `/api/login`, so the UI is
  multi-user-capable in appearance. *Backend auth still checks a single shared password in v1 —
  the username is cosmetic until a real multi-user backend lands (deferred).*
- **Self-hosted fonts** (Archivo + IBM Plex Mono, woff2, ~65 KB) — no CDN, consistent with the
  LAN-only stance.
- **Services tile shows version + port** per service (e.g. `smbd 4.15.13 · port 445`,
  `ssh Dropbear v2022.83 · port 2222`); `nmbd` inherits smbd's Samba version, and the dropbear
  version probe calls `dropbearmulti dropbear -V` (it is not on `PATH` as `dropbear`).

### Verified live

Real-time updates confirmed on the box (CPU/mem/load tick ~2 s; network and disk-I/O
sparklines scroll); responsive reflow confirmed at 1920 / 1512 / 1300 / 1000 / 600 px; all
three themes confirmed in-browser; services show live version + port. 28 Vue component tests
plus the full native C suite pass.

### Still open / deferred

- Multi-user auth backend (username currently cosmetic — single shared password).
- The shippable **installer** must `chmod -R a+rX` the staged `www/` (the `nobody` server must
  read it) and bundle the redesigned binaries + SPA; the live box is fixed, the installer
  payload update is pending.
- WAN byte-totals (v1 ships connection + public-IP flag, option (b) above).

## Size

Project (multi-component: a new C server, a Samba tool rebuild, a Vue app, installer wiring).
→ Built and deployed; Phase 1 complete.
