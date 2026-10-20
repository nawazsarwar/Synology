# DS207 Monitoring Dashboard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship a reversible, own-port (`:8181`) read-only monitoring dashboard for the DS207, installed as another `/volume1/@modernsmb` overlay module, that shows live NAS health/activity including the modern Samba/SSH/curl stack.

**Architecture:** Privilege-separated. A root `collector` reads `/proc` + whitelisted tools and writes JSON snapshot files to `.../ui/state/`; it is never on the network. An unprivileged, shell-free `uiserver` serves the Vue static SPA and those snapshot files over plain HTTP on `:8181`, behind a password login. The Vue 3 SPA (built on the dev machine, shipped static) renders draggable desktop-OS tiles that poll `/api/*`.

**Tech Stack:** C (static musl, `zig cc -target powerpc-linux-musl`, big-endian PPC 603e, `syscompat.o` shim) for the two daemons; the existing Samba 4.15.13 tree for an `smbstatus`; Vue 3 + Pinia + uPlot + Vite for the SPA; POSIX `sh` for the installer.

**Spec:** `DS207/documents/WEBUI_DASHBOARD_DESIGN.md`

## Global Constraints

- Target: PowerPC `mpc824x` 603e, **big-endian**, kernel **2.6.24**, 64 MB RAM. All on-box binaries: static, musl, `-mcpu=603e -no-pie -static`, force-link `syscompat.o` (reuse `workspace/build/scripts/syscompat.c`). QEMU `-r 2.6.24` pre-flight before deploy.
- Never touch flash, the stock DSM UI (`/usr/syno/synoman`), or DSM's Apache. Everything under `/volume1/@modernsmb/`.
- Install root: `/volume1/@modernsmb/ui/` with `www/` (SPA), `state/` (JSON snapshots), `etc/` (password hash, config), `bin/` (collector, uiserver), `var/` (logs, pids).
- Listen port: **8181**, plain HTTP, bind all interfaces (LAN-only by documentation, like the rest of the project).
- `uiserver` runs as **`nobody`** (drops privilege after bind); `collector` runs as root but **never binds a socket** and only runs a **fixed whitelist of commands with fixed arguments** — no request data ever reaches it.
- Read-only: no endpoint changes NAS state in v1.
- Private (RFC1918) ranges for WAN classification: `10.0.0.0/8`, `172.16.0.0/12`, `192.168.0.0/16`, `127.0.0.0/8`, `169.254.0.0/16`.
- Refresh cadences written by the collector: light metrics ~2 s, sessions ~5 s, services ~60 s, SMART ~300 s (SSD; no standby concern).
- No watchdog on the UI and no coupling to the Samba revert path: a UI crash must never affect Samba.
- Commits frequent, present-tense, no attribution lines.

## Review Focus

- **Path traversal on static serving:** a request like `GET /../../etc/shadow` or URL-encoded `%2e%2e` must be rejected, never served. → Task 9 tests.
- **Unauthenticated access:** any `/api/*` or SPA asset fetched without a valid session cookie must return 401/redirect, never data. → Task 10 tests.
- **Malformed/partial `/proc` and tool output:** a short `/proc/net/dev`, a `smartctl` line without a temperature, an empty `smbstatus` (no sessions) must yield valid JSON (empty/զero fields), never a crash or truncated file. → Tasks 2–7 tests (each parser gets a malformed-input case).
- **Snapshot read during write:** `uiserver` reading a `state/*.json` while `collector` rewrites it must never serve a half-written file. → Task 8 (atomic write: temp file + `rename`) + Task 9 test reads a file mid-rotation.
- **Degraded array surfaced, not hidden:** `/proc/mdstat` showing `[U_]`/`[_U]` must report `degraded`, not `ok`. → Task 4 test with the live degraded fixture.

---

## File Structure

```
DS207/workspace/build/dashboard/
  src/
    json.h / json.c         # tiny JSON string builder (append helpers, escaping)
    parse_system.h/.c       # cpu/mem/load/uptime  -> JSON
    parse_net.h/.c          # /proc/net/dev        -> JSON
    parse_diskio.h/.c       # /proc/diskstats      -> JSON
    parse_storage.h/.c      # df + /proc/mdstat     -> JSON (RAID state)
    parse_smart.h/.c        # smartctl -a output   -> JSON (health/temp/model)
    parse_services.h/.c     # ps/netstat + version strings -> JSON
    parse_sessions.h/.c     # smbstatus + who      -> JSON
    parse_wan.h/.c          # /proc/net/tcp        -> JSON (peers, public flag)
    collector.c             # main: read sources, call parsers, atomic-write state/*.json
    uiserver.c              # main: bind:8181, drop priv, serve www/ + state/, auth
    http.h/.c               # minimal HTTP/1.0 request parse + response write
    auth.h/.c               # password hash verify + session token
  test/
    test.h                  # TEST/ASSERT macros, runner
    test_parse_*.c          # one per parser, fixture-driven
    test_http.c             # path-traversal + request parse
    test_auth.c             # login + session gate
    fixtures/               # sample /proc + tool outputs (incl. degraded mdstat)
  Makefile                  # test-native (host cc), build-ppc (zig cc cross)
  www/                      # build output target for the SPA (gitignored)

DS207/workspace/build/dashboard-ui/   # Vue source (built with Vite)
  package.json, vite.config.js
  src/ (main.js, App.vue, stores/, api/, components/tiles/, views/Login.vue)

DS207/workspace/build/scripts/
  build_dashboard_ppc.sh    # cross-compile collector + uiserver (+ syscompat.o, QEMU check)
  build_smbstatus_ppc.sh    # build smbstatus from the Samba tree against our state dirs

DS207/workspace/build/installer/ds207-modern/
  payload/ui/               # staged: collector, uiserver, smbstatus, www/ (gitignored binaries)
  install.sh                # + ui module
  payload/modernsmb.sh      # + ui-start/ui-stop/ui in status + boot
  payload/S99modernsmb      # (unchanged; boot path already calls modernsmb.sh boot)
```

---

## Task 1: Scaffold + native test harness + JSON builder

**Files:**
- Create: `DS207/workspace/build/dashboard/src/json.h`, `src/json.c`, `test/test.h`, `test/test_json.c`, `Makefile`

**Interfaces:**
- Produces:
  - `typedef struct { char *buf; size_t len, cap; } jb;`
  - `void jb_init(jb*); void jb_free(jb*);`
  - `void jb_raw(jb*, const char *s);` — append literal
  - `void jb_kstr(jb*, const char *key, const char *val);` — `"key":"escaped-val"` with comma handling
  - `void jb_knum(jb*, const char *key, long long val);`
  - `void jb_kreal(jb*, const char *key, double val);`
  - All escape `"` `\` and control chars per JSON.
- `test.h` provides `TEST(name)`, `ASSERT(cond)`, `ASSERT_STR_EQ(a,b)`, and a `main` that runs registered tests and returns non-zero on any failure.
- `Makefile`: `make test-native` compiles host (`cc -I src`) and runs `test/*`; `make build-ppc` invokes `scripts/build_dashboard_ppc.sh` (stub for now).

- [ ] **Step 1: Write failing test** `test/test_json.c`
```c
TEST(json_escapes_and_commas) {
  jb b; jb_init(&b);
  jb_raw(&b, "{"); jb_kstr(&b, "name", "a\"b"); jb_knum(&b, "n", 42); jb_raw(&b, "}");
  ASSERT_STR_EQ(b.buf, "{\"name\":\"a\\\"b\",\"n\":42}");
  jb_free(&b);
}
```
- [ ] **Step 2: Run** `make test-native` → Expected: FAIL (no `json.c`).
- [ ] **Step 3: Implement** `json.c` per the Interfaces signatures. Growable buffer via `realloc`; `jb_k*` prepend a comma when `len>0` and the last char is not `{` or `[`.
- [ ] **Step 4: Run** `make test-native` → Expected: PASS.
- [ ] **Step 5: Commit** `feat(dashboard): json builder + native test harness`.

---

## Task 2: System parser (cpu/mem/load/uptime)

**Files:** Create `src/parse_system.h/.c`, `test/test_parse_system.c`, `test/fixtures/{proc_stat,meminfo,loadavg,uptime}.txt`

**Interfaces:**
- Consumes: `json.h`.
- Produces: `int build_system_json(const char *stat, const char *meminfo, const char *loadavg, const char *uptime, char *out, size_t outlen);` → bytes written, or -1 on overflow. CPU is reported as **raw cumulative jiffies** (`cpu_total`, `cpu_idle`); the client computes the busy %. Memory in kB (`mem_total`, `mem_free`, `mem_cached`). `load1/5/15` as reals. `uptime_s` integer.

- [ ] **Step 1: Write failing tests** in `test_parse_system.c`:
```c
TEST(system_parses_fields) {
  char out[1024];
  int n = build_system_json(FX_STAT, FX_MEMINFO, "0.10 0.05 0.01 2/64 7562", "2913734.12 100.0", out, sizeof out);
  ASSERT(n > 0);
  ASSERT(strstr(out, "\"mem_total\":61456"));
  ASSERT(strstr(out, "\"load1\":0.1"));
  ASSERT(strstr(out, "\"cpu_total\":"));
}
TEST(system_handles_short_input) { char o[1024]; ASSERT(build_system_json("cpu 1 2 3\n","MemTotal: 10 kB\n","0 0 0 1/1 1","1.0 1.0",o,sizeof o) > 0); }
```
(`FX_STAT`/`FX_MEMINFO` are `#include`d fixture string literals.)
- [ ] **Step 2: Run** the two tests → FAIL.
- [ ] **Step 3: Implement** `build_system_json`. Sum the `cpu ` line fields for `cpu_total`; field 4 is `idle`. Grep `MemTotal/MemFree/Cached`. First token of uptime → `uptime_s`.
- [ ] **Step 4: Run** → PASS.
- [ ] **Step 5: Commit** `feat(dashboard): system metrics parser`.

---

## Task 3: Network + disk-I/O parsers

**Files:** Create `src/parse_net.h/.c`, `src/parse_diskio.h/.c`, their tests, fixtures `proc_net_dev.txt`, `proc_diskstats.txt`.

**Interfaces:**
- Produces:
  - `int build_net_json(const char *proc_net_dev, char *out, size_t outlen);` → JSON array of `{iface, rx_bytes, tx_bytes}` for non-`lo` interfaces (cumulative; client computes rate).
  - `int build_diskio_json(const char *proc_diskstats, char *out, size_t outlen);` → array of `{dev, rd_sectors, wr_sectors}` for whole disks only (names matching `sd[a-z]` or `md[0-9]`, skip `ram*`/partitions).

- [ ] **Step 1: Write failing tests**: `net` emits `eth0` with the fixture's `rx_bytes`/`tx_bytes` and omits `lo`; `diskio` emits `sda`/`md2`, omits `ram0` and `sda5`. Add a malformed case (header-only input → `[]`).
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement** both. `/proc/net/dev`: split on `:`, field 1 = rx bytes, field 9 = tx bytes. `/proc/diskstats`: cols 3=name, 6=rd sectors, 10=wr sectors.
- [ ] **Step 4: Run** → PASS.
- [ ] **Step 5: Commit** `feat(dashboard): net + disk-io parsers`.

---

## Task 4: Storage + RAID parser

**Files:** Create `src/parse_storage.h/.c`, test, fixtures `df.txt`, `proc_mdstat.txt` (use the **live degraded** `md1 [2/1] [U_]`).

**Interfaces:**
- Produces: `int build_storage_json(const char *df_out, const char *mdstat, char *out, size_t outlen);` → `{volumes:[{mount,size_kb,used_kb,avail_kb}], arrays:[{name,level,state}]}` where `state` ∈ `"ok"|"degraded"|"recovering"`.

- [ ] **Step 1: Write failing tests**:
```c
TEST(storage_flags_degraded) {
  char o[2048]; build_storage_json(FX_DF, FX_MDSTAT, o, sizeof o);
  ASSERT(strstr(o, "\"name\":\"md1\""));
  ASSERT(strstr(o, "\"state\":\"degraded\""));   // [U_]
  ASSERT(strstr(o, "\"name\":\"md2\"") && strstr(o, "\"state\":\"ok\""));  // [U]
}
TEST(storage_parses_volume1) { char o[2048]; build_storage_json(FX_DF,FX_MDSTAT,o,sizeof o); ASSERT(strstr(o,"/volume1")); }
```
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement.** df: parse `Mounted on` rows for `/` and `/volume*`. mdstat: for each `mdN : active <level> ...`, read the following `[n/m] [U_...]` line; any `_` → `degraded`; a `recovery`/`resync` line → `recovering`; else `ok`.
- [ ] **Step 4: Run** → PASS.
- [ ] **Step 5: Commit** `feat(dashboard): storage + RAID state parser`.

---

## Task 5: SMART parser

**Files:** Create `src/parse_smart.h/.c`, test, fixture `smartctl_a.txt` (capture `/usr/syno/bin/smartctl -a /dev/sda` from the box — the PM830 SSD).

**Interfaces:**
- Produces: `int build_smart_json(const char *dev, const char *smartctl_a, char *out, size_t outlen);` → `{dev, model, serial, health, temp_c}`. `health` from the `SMART overall-health` line (`PASSED`→`"ok"`, `FAILED`→`"fail"`, missing→`"unknown"`). `temp_c` from attribute `Temperature_Celsius` (or `194`); missing → `-1`.

- [ ] **Step 1: Write failing tests**: parses `model`/`health:"ok"`/`temp_c`; a fixture with no temperature line → `"temp_c":-1` and still valid JSON.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement** (string scans; smartctl 5.38 output format).
- [ ] **Step 4: Run** → PASS.
- [ ] **Step 5: Commit** `feat(dashboard): SMART parser`.

---

## Task 6: Services + versions parser

**Files:** Create `src/parse_services.h/.c`, test, fixture `services_probe.txt`.

**Interfaces:**
- Produces: `int build_services_json(const svc_probe *probes, int n, char *out, size_t outlen);` where `typedef struct { const char *name; int up; const char *version; } svc_probe;` → array of `{name, up, version}`. (The *collecting* of probes — running `ps`/`netstat`/`--version` — lives in `collector.c`, Task 8; this task only formats a gathered list, keeping it pure/testable.)

- [ ] **Step 1: Write failing test**: given 2 probes (one up with version, one down with `NULL` version) → correct JSON (`"up":true/false`, `"version":null` when absent).
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** → PASS.
- [ ] **Step 5: Commit** `feat(dashboard): services/versions formatter`.

---

## Task 7: Sessions parser (SMB + SSH)

**Files:** Create `src/parse_sessions.h/.c`, test, fixtures `smbstatus.txt`, `who.txt`, `smbstatus_empty.txt`.

**Interfaces:**
- Produces: `int build_sessions_json(const char *smbstatus_out, const char *who_out, char *out, size_t outlen);` → `{smb:[{user,ip,share}], open_files:[{user,path}], ssh:[{user,ip}]}`. Parses `smbstatus` sections (sessions, tree connects, locked files) and `who` columns.

- [ ] **Step 1: Write failing tests**: a populated fixture yields ≥1 `smb` entry + ≥1 `open_files` entry + ssh from `who`; `smbstatus_empty.txt` yields `{"smb":[],"open_files":[],"ssh":[...]}` (no crash).
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement** (capture the real `smbstatus` layout in the fixture first, from Task 12's binary; until then use the documented 4.x section format and adjust in Task 12).
- [ ] **Step 4: Run** → PASS.
- [ ] **Step 5: Commit** `feat(dashboard): sessions parser`.

---

## Task 8: Collector main (atomic snapshots, cadences, whitelist)

**Files:** Create `src/collector.c`; modify `Makefile` (add `collector` native target for a smoke run).

**Interfaces:**
- Consumes: all `build_*_json` + `json.h`.
- Produces: an executable `collector` that writes, into `argv[1]` (state dir), `system.json`, `net.json`, `diskio.json`, `storage.json`, `disks.json`, `services.json`, `sessions.json`, `wan.json` (wan added in Task 17). **Atomic write:** write `NAME.json.tmp` then `rename()` over `NAME.json`. One pass per invocation; a `--loop` flag sleeps and refreshes each file on its cadence (2/5/60/300 s) using a tick counter.
- Whitelist of commands it runs (fixed argv, no shell): `df`, `/usr/syno/bin/smartctl -a /dev/sda`, `<ui>/bin/smbstatus -s <conf>`, `who`, `netstat -ltn`, `ps w`, and fixed `--version`/`-V` calls. `/proc/*` read directly.

- [ ] **Step 1: Write failing test** `test/test_collector_smoke.c` (host): call an extracted `collect_once(const char *statedir)` against a temp dir seeded with fixture `/proc` files (inject via a `COLLECTOR_PROC_DIR` env override) and assert `system.json` exists and parses non-empty. *(Keep `collect_once` taking a proc-dir + tool-output provider so it stays host-testable; the real tool execution is a thin wrapper used only on the box.)*
- [ ] **Step 2: Run** `make test-native` → FAIL.
- [ ] **Step 3: Implement** `collector.c` with `collect_once()` + `main()` (`--loop` cadence). Reads proc paths relative to `COLLECTOR_PROC_DIR` (default `/proc`).
- [ ] **Step 4: Run** → PASS.
- [ ] **Step 5: Commit** `feat(dashboard): collector main with atomic snapshots`.

---

## Task 9: uiserver static + API file serving (+ path traversal)

**Files:** Create `src/http.h/.c`, `src/uiserver.c` (serving only; auth in Task 10), `test/test_http.c`.

**Interfaces:**
- Produces:
  - `int http_parse_request(const char *raw, http_req *out);` → method, path, decoded path, headers (Cookie).
  - `int safe_path(const char *url_path, const char *root, char *resolved, size_t n);` → 0 and a path **guaranteed inside `root`**, or -1 (reject `..`, encoded `..`, absolute escapes, NUL).
  - uiserver (this task): `GET /` → `www/index.html`; `GET /assets/*` → files under `www/`; `GET /api/<x>` → `state/<x>.json`; correct `Content-Type`; 404 otherwise.

- [ ] **Step 1: Write failing tests** in `test_http.c`:
```c
TEST(rejects_traversal) { char r[512];
  ASSERT(safe_path("/../../etc/shadow", "/w/www", r, sizeof r) == -1);
  ASSERT(safe_path("/..%2f..%2fetc/passwd", "/w/www", r, sizeof r) == -1);
  ASSERT(safe_path("/assets/app.js", "/w/www", r, sizeof r) == 0); }
TEST(maps_api_to_state) { http_req q; http_parse_request("GET /api/system HTTP/1.0\r\n\r\n", &q); ASSERT_STR_EQ(q.path, "/api/system"); }
```
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement** `http.c` (percent-decode, then reject any `..` segment or non-`root` prefix) and the serving branch of `uiserver.c`.
- [ ] **Step 4: Run** → PASS.
- [ ] **Step 5: Commit** `feat(dashboard): uiserver static+api serving with traversal guard`.

---

## Task 10: uiserver auth (login + session gate)

**Files:** Create `src/auth.h/.c`, `test/test_auth.c`; modify `uiserver.c`.

**Interfaces:**
- Produces:
  - `int auth_verify(const char *password, const char *hash_file);` → 1 if password matches the stored hash (`crypt()` sha-based salt stored in `ui/etc/passwd.hash`).
  - `char *auth_new_token(void);` / `int auth_check_token(const char *tok);` → in-memory session set (bounded), 24 h expiry.
  - uiserver gating: `POST /api/login` (form/JSON `{password}`) → `Set-Cookie: sid=<tok>; HttpOnly; SameSite=Strict` on success, 401 on failure; every other `/api/*` and the SPA shell require a valid `sid` cookie else 401 (for `/api/*`) or serve `login.html`/redirect (for navigations).

- [ ] **Step 1: Write failing tests**: `auth_verify` true for the right password, false for wrong, false for missing hash file; a token round-trips through `auth_check_token`; an unknown token fails.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement** `auth.c` (use `crypt()`; if unavailable in the musl build, fall back to a vendored SHA-256 + salt — decide at build, note in code). Wire the gate into `uiserver.c`.
- [ ] **Step 4: Run** → PASS.
- [ ] **Step 5: Commit** `feat(dashboard): uiserver password login + session gate`.

---

## Task 11: uiserver main (bind, drop privilege, loop) + cross-build + QEMU

**Files:** Modify `src/uiserver.c` (add `main`: bind `:8181`, `setgid/setuid` to `nobody`, accept loop); create `scripts/build_dashboard_ppc.sh`; modify `Makefile` (`build-ppc`).

**Interfaces:**
- Produces: on-box binaries `collector`, `uiserver`. `uiserver argv`: `--root <ui dir> --port 8181 --user nobody`. Binds first (needs no root for 8181, but drops to `nobody` regardless), then drops privilege, then serves.
- `build_dashboard_ppc.sh`: `zig cc -target powerpc-linux-musl -mcpu=603e -static -no-pie`, force-link `syscompat.o` (+ `compat64.o` if time funcs are used), `llvm-objcopy --strip-all`, then QEMU `-r 2.6.24` smoke (`uiserver --help`, `collector --once` on a seeded dir).

- [ ] **Step 1:** Write `build_dashboard_ppc.sh`; build both binaries.
- [ ] **Step 2: Verify** under QEMU: `qemu-ppc-static -r 2.6.24 ./uiserver --help` exits 0 (no "kernel too old"/"illegal instruction"); `collector --once /tmp/st` writes JSON.
- [ ] **Step 3: Implement** `main()` for uiserver (privilege drop order: bind → `setgroups([])` → `setgid` → `setuid` → verify `getuid()!=0`).
- [ ] **Step 4: Verify** again under QEMU; `make test-native` still green.
- [ ] **Step 5: Commit** `feat(dashboard): uiserver main + PPC cross-build + QEMU preflight`.

---

## Task 12: our-Samba smbstatus build + live-fixture confirm

**Files:** Create `scripts/build_smbstatus_ppc.sh`; update `test/fixtures/smbstatus.txt` and `test/test_parse_sessions.c` with the **real** captured output.

**Interfaces:**
- Produces: on-box `smbstatus` (static PPC) that reads `/volume1/@modernsmb` state via `-s <our smb.conf>`. Confirms Task 7's parser against reality.

- [ ] **Step 1:** Build `smbstatus` from the existing Samba 4.15.13 build tree (same toolchain/flags as `smbd`); strip; QEMU `-r 2.6.24` smoke.
- [ ] **Step 2: On the box:** deploy it, hold an SMB session + an open file from a client, run `smbstatus -s <conf>`, capture output.
- [ ] **Step 3:** If the captured layout differs from `smbstatus.txt`, update the fixture + adjust `parse_sessions.c`; re-run `make test-native`.
- [ ] **Step 4: Verify** sessions + open file appear.
- [ ] **Step 5: Commit** `feat(dashboard): smbstatus build + real sessions fixture`.

---

## Task 13: Vue SPA scaffold + API client + Pinia store

**Files:** Create `dashboard-ui/` (Vite + Vue 3), `src/api/client.js`, `src/stores/metrics.js`, `vite.config.js` (base `./`, build to `../dashboard/www`).

**Interfaces:**
- Produces:
  - `api/client.js`: `getJSON(path)` (credentials: same-origin; on 401 → route to login), `login(password)`.
  - `stores/metrics.js` (Pinia): polling actions `startPolling()` that fetch `system/net/diskio/storage/sessions` at 2 s, `disks/services` at their slower cadence, computing **rates** from successive cumulative counters; state getters per tile.
- Build output lands in `dashboard/www/` so the installer stages one tree.

- [ ] **Step 1: Write failing test** (Vitest) for the store's **rate computation**: feeding two `net` samples (Δbytes over Δt) yields the expected bytes/s; a counter reset (new < old) yields `0`, not negative.
- [ ] **Step 2: Run** `npm test` → FAIL.
- [ ] **Step 3: Implement** scaffold, client, store.
- [ ] **Step 4: Run** `npm test` → PASS; `npm run build` emits to `dashboard/www`.
- [ ] **Step 5: Commit** `feat(dashboard-ui): scaffold + api client + metrics store`.

---

## Task 14: Desktop-OS tiles + charts + draggable layout

**Files:** Create `src/App.vue`, `src/components/tiles/*.vue` (CpuTile, MemTile, LoadTile, NetTile, DiskIoTile, StorageTile, SmartTile, SessionsTile, FilesTile, ServicesTile, WanTile), `src/components/Desktop.vue`, integrate uPlot.

**Interfaces:**
- Consumes: `stores/metrics.js` getters.
- Produces: a `Desktop.vue` that renders tiles as draggable/resizable windows on a desktop surface; positions/sizes persisted to `localStorage` (try/catch; renders with defaults if unavailable). Each tile is a dumb component reading its store slice. NetTile/DiskIoTile use uPlot live sparklines. StorageTile shows the array state with a clear **degraded** warning styling.

- [ ] **Step 1: Write failing test** (Vitest + Vue Test Utils): `StorageTile` renders a visible "Degraded" indicator when its prop `state==='degraded'`.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement** tiles, Desktop drag/persist, uPlot wiring.
- [ ] **Step 4: Run** → PASS; `npm run build`.
- [ ] **Step 5: Commit** `feat(dashboard-ui): desktop tiles, charts, draggable layout`.

---

## Task 15: Login view + auth flow

**Files:** Create `src/views/Login.vue`; wire routing/guard in `App.vue`/`main.js`.

**Interfaces:**
- Consumes: `api/client.js` `login()`; 401 handling from Task 13.
- Produces: an unauthenticated visitor sees the login screen; a correct password sets the cookie (server-side) and reveals the desktop; logout clears the session.

- [ ] **Step 1: Write failing test** (Vitest): submitting the login form calls `client.login` with the entered password and, on resolve, flips an `authed` flag.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** → PASS; `npm run build`.
- [ ] **Step 5: Commit** `feat(dashboard-ui): login view + auth guard`.

---

## Task 16: Installer integration (ui module)

**Files:** Modify `installer/ds207-modern/install.sh` (add `install_ui`/`uninstall` wiring + menu/CLI `ui`), `payload/modernsmb.sh` (add `ui_start`/`ui_stop`, `ui-start`/`ui-stop` cases, `ui` in `status`, start in `boot`); stage `payload/ui/` (collector, uiserver, smbstatus, `www/`); set an install-time password prompt that writes `ui/etc/passwd.hash`.

**Interfaces:**
- Consumes: the built binaries + `www/`.
- Produces: `sh install.sh ui` installs under `/volume1/@modernsmb/ui`, prompts for a dashboard password (`[ -t 0 ]`), starts `collector --loop` + `uiserver`; `status` shows `dashboard : RUNNING on :8181`; `uninstall` stops both and removes `ui/` (stock DSM UI untouched). Boot hook starts both; **no watchdog, no Samba coupling**.

- [ ] **Step 1:** Implement the installer + control-script changes (mirror `install_ssh`/`install_curl` + `ssh_start`/`ssh_stop` patterns already in the files).
- [ ] **Step 2: Deploy** to the box, run `sh install.sh ui`, set a password.
- [ ] **Step 3: Verify** `modernsmb.sh status` lists the dashboard; `curl :8181/api/login` sets a cookie; an authenticated `GET /api/system` returns JSON; browser shows tiles.
- [ ] **Step 4: Verify** `ui-stop`/`ui-start`, reboot persistence, and `uninstall` leaves DSM `:5000` working.
- [ ] **Step 5: Commit** `feat(installer): dashboard (ui) module`.

---

## Task 17: WAN tile (spike → implement)

**Files:** Create `src/parse_wan.h/.c`, test, fixture `proc_net_tcp.txt`; wire into `collector.c` (`wan.json`) and `WanTile.vue`.

**Interfaces:**
- Produces: `int build_wan_json(const char *proc_net_tcp, char *out, size_t outlen);` → `{public_conns:[{local,remote}], public_count, lan_count}` classifying each connection's remote address as RFC1918/loopback/link-local (LAN) vs public, decoding the hex `addr:port` in `/proc/net/tcp`.

- [ ] **Step 1: Spike (timeboxed):** on the box, confirm `/proc/net/tcp` is populated and whether `iptables` exists for byte-accounting. Record the a/b decision in the design doc's Open-risk section. Default **(b)** — connections + public flag — if accounting isn't cleanly available.
- [ ] **Step 2: Write failing test**: a fixture with one `192.168.x` peer and one public peer → `public_count:1`, `lan_count:1`, and the public remote decoded correctly (hex→dotted, little-endian).
- [ ] **Step 3: Run** → FAIL.
- [ ] **Step 4: Implement** parser + collector wiring + `WanTile.vue`; `make test-native` + `npm test` green; rebuild.
- [ ] **Step 5: Commit** `feat(dashboard): WAN connection awareness (v1: public-peer flag)`.

---

## Task 18: On-box aggressive + regressive battery + docs

**Files:** Modify `DS207/documents/PROJECT_LOG.md` (new trial section), `INSTALL.md` + `README.md` (dashboard module), `.gitignore` (ui payload binaries + `dashboard/www`), `VERSION` (→ v0.3.0).

- [ ] **Step 1: Aggressive** (on the box): unauth `/api/system` → 401; path-traversal `GET /..%2f..%2fetc/passwd` → rejected; junk flood to `:8181` → `uiserver` survives + still serves; 4 concurrent authed clients; snapshot-during-write never serves truncated JSON; privilege check (`uiserver` runs as `nobody`, `collector` as root but no socket).
- [ ] **Step 2: Resource** : watch `free` + CPU during 5 min of polling — collector/uiserver stay small, CPU not pegged; SMART cache honored (disk not hammered).
- [ ] **Step 3: Regressive** : DSM `:5000` + `:80` still serve; SMB/SSH/curl modules unaffected (all RUNNING, ports listening); no cores; `uninstall` restores clean state; a UI crash does **not** trigger the Samba revert.
- [ ] **Step 4:** Record results in `PROJECT_LOG.md`; update `INSTALL.md`/`README.md`/`.gitignore`/`VERSION`.
- [ ] **Step 5: Commit** `test(dashboard): on-box battery + docs; bundle v0.3.0`.

---

## Self-Review

**Spec coverage:** every design tile → a parser task (2–7,17) + a Vue tile (14,17); collector (8), uiserver serve/auth/main (9,10,11), smbstatus (12), SPA scaffold/login (13,15), installer + switch/uninstall semantics (16), LAN-only/plain-HTTP/read-only/privsep (constraints + 9–11), no-watchdog/no-Samba-coupling (16,18), phased roadmap (design only, out of v1 scope). Covered.

**Review Focus coverage:** path traversal → T9; unauth → T10; malformed proc/tool output → T2–T7 malformed cases; snapshot-during-write → T8 atomic write + T18 §1; degraded array surfaced → T4. All pinned.

**Type consistency:** `build_*_json(const char *..., char *out, size_t outlen) -> int` uniform across T2–T7,17; `jb` API fixed in T1; `safe_path`/`http_parse_request` fixed in T9 and reused in T10/T11; store getters consumed by tiles in T14/T15/T17 match T13.

**Proportion:** plan is tasks + signatures + test assertions, code blocks only for JSON-shape and traversal assertions the implementer must not reinvent. In proportion to a multi-component spec.
