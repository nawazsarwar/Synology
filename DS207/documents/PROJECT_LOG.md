# DS207 Modernization — Project Log & Documentation

**Last updated:** 2026-10-20
**Status:** Prototype built and deployed to hardware; deployment caused an out-of-memory (OOM) condition on the 64 MB device; recovering the NAS by reinstalling stock DSM via the reset button (Mode 2). Core finding: **Samba 4 as-built is too heavy for 64 MB RAM.**

This document is the running trail of the whole effort. Companion documents:
- `documents/AUDIT.md` — the detailed forensic firmware audit (versions, arch, checksum analysis).
- `workspace/build/README.md` — install/test/revert guide for the deliverables.

---

## 1. Objectives

Make a **Synology DS207** (a 2007, 2-bay NAS) usable again with **modern clients**:

- **SMB2 / SMB3** file sharing so **Windows 11** and **macOS** can mount shares (both block the DS207's stock SMB1/CIFS by default, and SMB1 is insecure).
- **Apple interoperability** (`vfs_fruit`): Finder discovery + **Time Machine over SMB**, replacing the dead AFP path.
- **mDNS / Bonjour** so the NAS auto-appears on the network.
- **Modern SSH** (Ed25519) instead of the ancient OpenSSH 5.2.

**Decided scope (agreed with the owner):** modernize the **file-sharing back-end only**. Keep the stock DSM 3.1 web UI (the modern DSM UI cannot run on this hardware). Drop AFP (superseded by SMB+fruit). This is a **working prototype first**; polish/round-2 later.

---

## 2. Constraints (the hard limits that shaped everything)

From the official datasheet and firmware analysis:

| Constraint | Value | Why it matters |
|---|---|---|
| **CPU** | Marvell/Freescale **MPC8241, PowerPC 603e, 266 MHz, single-core, big-endian** | All binaries must be cross-compiled for 32-bit big-endian PowerPC (`-mcpu=603e`). No off-the-shelf binaries exist. |
| **RAM** | **64 MB, soldered, NOT upgradable** | **The dominant constraint.** After the kernel + base DSM, only ~25–40 MB is free. This is what ultimately broke the deployment. |
| **Kernel** | **Linux 2.6.24** (big-endian PPC) | New binaries must make only syscalls available in 2.6.24. We used static musl to avoid "kernel too old." |
| **libc (stock)** | glibc 2.3.3 | Stock binaries are ancient; we build static so we don't depend on it. |
| **Filesystem** | EXT3 | Fine for `vfs_fruit` xattrs (needs `user_xattr`). |
| **Flash** | SPI NOR holds bootloader + `zImage` (kernel) + `rd.bin` (initrd) | **Never touched** — corrupting it is the only true brick path, so we avoided it entirely. |

**Golden rule throughout:** never write to flash; keep `zImage`/`rd.bin`/`updater` byte-for-byte stock; all changes live in the on-disk root filesystem (`hda1.tgz`) only. This is why the device was always recoverable.

---

## 3. The files

### Source firmware (provided)
- `DS207/DSM_DS207_1639.pat` — **stock DSM 3.1-1639** for the PowerPC DS207 (`synology_ppc824x_207`). Plain GNU tar. **This is the known-good firmware used to install/recover the NAS.**
- `DS207/DSM_DS207+_1636.pat` — DSM 3.1 for the DS207**+** (Marvell ARMv5 `88f5281`). Different CPU; not our target. Ignored.
- `DS216/DSM_DS216+II_90080.pat` — **DSM 7.4.1** for the DS216+II (Intel x86_64). Used as a **reference** for modern configs (its binaries can't run on PPC). Encrypted; we decrypted it to read its modern `smb.conf`.

### What a `.pat` contains
A `.pat` is a tar of: `VERSION`, `hda1.tgz` (the root filesystem installed to the SATA disk), `zImage` (kernel, flashed), `rd.bin` (initrd, flashed), `updater` (installer), and `checksum.syno` (integrity file).

### Deliverables we built (`workspace/build/`)
- `ds207_patch/ds207_live_patch.sh` + `ds207_modern_payload.tgz` — the **live-patch**: installs the modern stack over SSH onto a running DSM, reversible. *(This is what we deployed; it worked mechanically but OOM'd the box — see §6.)*
- `deliverables/DS207_Modernized_Custom.pat` — a full custom firmware image. **Not usable**: its `checksum.syno` can't be fully reproduced (see AUDIT.md), so Synology Assistant would reject it. Kept for completeness.
- `samba/` — the built static Samba 4.15.13 binaries (`smbd` 23.7 MB, `nmbd`, `testparm`, `smbcontrol`).
- `sbin/dropbearmulti` — static Dropbear SSH (Ed25519).
- `configs/` — `smb.conf.ds207` (SMB2/3 + fruit, low-RAM tuned) and Avahi `.service` files (Finder + Time Machine).
- `scripts/` — the full reproducible cross-compile pipeline (see §5).

### Tooling fetched
- `workspace/assistant/` — Synology Assistant 3.2-1920 (Windows/Linux/Mac) to install/recover DSM.

---

## 4. Given the constraints, what we're trying to achieve (concretely)

A **self-contained, statically-linked** set of PowerPC binaries that:
1. Run on Linux 2.6.24 / 64 MB (no modern-libc, no modern-kernel dependencies).
2. Replace only the file-sharing daemons (`smbd`/`nmbd`), plus add Dropbear + Avahi config.
3. Reuse the DSM user database and the DSM-created shares/volume.
4. Install reversibly over SSH, with the stock binaries kept as `.orig` for one-command rollback.
5. Fit in memory alongside the running DSM. **← this is the requirement we failed to meet.**

---

## 5. How we built it (the pipeline)

Everything is reproducible via Docker + [zig](https://ziglang.org) + QEMU, on an Apple-Silicon Mac. Scripts in `workspace/build/scripts/`.

- **Toolchain:** `zig cc -target powerpc-linux-musl` — a native arm64 cross-compiler to 32-bit big-endian PowerPC musl. (We first tried the Bootlin x86_64 GCC under emulation; it SIGSEGV'd randomly, so zig replaced it.)
- **Test harness:** `qemu-ppc-static -r 2.6.24` inside a Debian container, spoofing the DS207 kernel version. A PPC `binfmt_misc` handler lets build-time tools run transparently.
- **Dropbear:** `build` — straightforward static musl build; verified by generating a real Ed25519 key under QEMU.
- **Crypto deps:** `build_deps.sh` — gmp → libtasn1 → nettle → **gnutls 3.7.11** (mandatory for Samba 4 SMB3), all static + `-fPIC`.
- **Samba 4.15.13:** `build_samba_static.sh` — the hard part. waf cross-compile with `--cross-execute` via QEMU; `--nonshared-binary` + `--with-static-modules=vfs_fruit,vfs_catia,vfs_streams_xattr` to make a **fully static** smbd with Apple VFS built in; a CC wrapper handled zig/musl quirks (soname `-h`, netgroup stubs, and the zig-musl `__*_time64` symbol handling). Result verified under QEMU: `smbd -V`, `testparm`, `nmbd -V` all report **Version 4.15.13**.
- **Packaging:** `package_pat.sh` builds the modernized `hda1.tgz`; the live-patch payload is assembled with root ownership.

Full technical detail (every fix, in order) is in `AUDIT.md` and the project memory files.

---

## 6. What we have tried (chronological trail)

1. **Forensic audit** of all three `.pat` files. Confirmed target = PowerPC DS207, kernel 2.6.24, 64 MB, Samba **3.2.8 (SMB1 only)**, Netatalk 2.x, OpenSSH 5.2, OpenSSL 1.0.0n, Avahi 0.6.28. (`AUDIT.md`)
2. **Decrypted the DS216 (DSM 7) image** to extract modern reference configs (modern `smb.conf` = `min SMB2 / max SMB3`).
3. **Stood up the QEMU harness**; proved stock busybox/glibc run under emulation with the 2.6.24 kernel spoof.
4. **Built Dropbear** (static, Ed25519) — verified under QEMU.
5. **Built Samba 4.15.13** static with SMB2/3 + `vfs_fruit` — verified it runs under QEMU (version/config checks). Could not test live *serving* under qemu-user (its netlink emulation stalls interface discovery — an emulator limit, not a code bug).
6. **Authored configs:** low-RAM `smb.conf` (SMB2/3, fruit, capped processes) and Avahi service files for Finder + Time Machine.
7. **Packaged** both a live-patch and a custom `.pat`. Documented that the `.pat` can't be validated (checksum `field4/field5` not reverse-engineered) → chose the **live-patch** as the install method.
8. **On hardware:** the NAS arrived blank (DSM not installed). Installed the **stock `DSM_DS207_1639.pat`** via Synology Assistant (run on the Mac through Rosetta). DSM 3.1 came up healthy; created a volume + `AbdullahDrive` share + user; macOS mounted it over SMB (stock, via SMB1 fallback).
9. **Deployed the live-patch** over SSH. It ran correctly: hardware verified, stock Samba backed up as `.orig`, new `smbd -V` reported **Version 4.15.13 on the real hardware**, Dropbear + Avahi installed.
10. **Failure:** when Samba was restarted, **`smbd` would not stay running**, DSM's web UI became unresponsive (tiny windows, no graphs), and the box eventually rebooted/powered off. Diagnosis: **out-of-memory** — the 24 MB static Samba 4 plus the running DSM stack exceeded 64 MB.
11. **Recovery attempt:** could not `uninstall` from inside — after the OOM cycle, **authentication broke** (both SSH and DSM web return `error_cantlogin`), because the degraded boot left auth services unstarted, and our smbd re-triggers the OOM on every boot. Verified from the Mac that admin/root logins are rejected.
12. **Recovery in progress (now):** **Mode 2 reset** (reset button: 1 beep, release, 3 beeps) to put the box into reinstall mode, then **reinstall stock `DSM_DS207_1639.pat`** via Assistant. This rewrites the system partition (removing our smbd + the OOM trigger) and keeps the data volume. *This is the step running as of this writing.*

---

## 7. Current status

- **NAS:** being restored to clean **stock DSM 3.1** via Mode-2 reset + stock `.pat` reinstall. Data volume (`AbdullahDrive`) expected to survive; only a test file is on it. **Nothing is bricked** (flash never touched).
- **Deliverables:** all built and verified *at the binary level* under QEMU, and `smbd -V` confirmed to run on the real PowerPC hardware. The blocker is **runtime memory**, not correctness or architecture.
- **Confirmed root cause:** **64 MB RAM is the wall.** Samba 4's startup footprint OOMs the box.

---

## 8. Key lessons

1. **The architecture work is sound** — cross-compiling modern Samba 4 for a 2007 PowerPC box and having it execute is proven. The hard engineering (toolchain, static linking, gnutls, QEMU verification) all worked.
2. **64 MB is a hard ceiling Samba 4 can't fit under.** This is a resource limit, not a bug. QEMU validated the binaries but could not reveal the memory footprint of live serving.
3. **The live-patch's reversibility was correct in design**, but a reboot before `uninstall` locked us out (OOM broke auth). **Future deploys must not auto-run the new smbd on boot until proven** — stage it to run manually first, or add a boot-time memory guard / watchdog that reverts automatically.
4. **Recovery path held:** because we never touched flash, Mode-2 reset + stock reinstall always recovers the device.

---

## 9. What we attempt next (options)

Pick after the NAS is back on stock DSM.

**Option A — Make it fit 64 MB (leaner Samba 4), low confidence.**
- Rebuild smbd with `-Os`, drop more VFS/auth modules, hard-cap `max smbd processes = 1`, minimal socket buffers, and wrap startup with `ulimit -v` + an `oom_adj` so the kernel kills *smbd* (not DSM) under pressure.
- Critically: **do not symlink it into the boot path.** Run it **manually** from SSH for testing, watching `free`, before ever making it the default. Add an auto-revert if memory drops below a threshold.
- Risk: Samba 4's baseline may simply not fit no matter how we trim.

**Option B — A lightweight SMB2 server instead of full Samba 4, medium confidence.**
- Evaluate a minimal userspace SMB2/3 implementation with a much smaller footprint than Samba 4. (Kernel `ksmbd` is not an option — it needs a far newer kernel than 2.6.24.)
- Goal: SMB2 at a fraction of the RAM. Less feature-complete (maybe no full `vfs_fruit`), but may actually run.

**Option C — Accept the hardware limit.**
- 64 MB may only reliably do **SMB1 (stock)**. If modern SMB is a must, it likely needs a newer DS-series model with ≥256 MB RAM. Document this as the honest conclusion.

**Recommended sequence:** restore stock DSM → re-baseline free memory (`free` on a quiet box) → try **Option A** *manually* (never in the boot path) with strict memory limits and auto-revert → if it won't fit, evaluate **Option B** → otherwise **Option C**.

**Safer deployment process for any retry:**
1. Copy binaries to `/tmp` (not into the boot path).
2. Stop stock smbd, run the new one **manually in the foreground** with memory limits, watch `free`.
3. Only if stable → install persistently, and add a boot watchdog that reverts to stock if smbd fails or memory is exhausted.
4. Keep an SSH session open the whole time; never reboot before confirming a clean state.

---

## 10. Recovery reference (if the box ever gets stuck again)

- **Mode 1 reset** (one ~4 s press → 1 beep): resets admin password + network only. Data + DSM kept.
- **Mode 2 reset** (~4 s → 1 beep, release, within 10 s ~4 s → 3 beeps): reinstall mode. Then reinstall **stock `DSM_DS207_1639.pat`** via Synology Assistant → clean DSM, **data kept**. Avoid any explicit "Erase All Data" prompt.
- The device is always recoverable because the bootloader/kernel in flash were never modified.

---

## 11. Round 2 — on-hardware memory optimization (live log)

Goal of round 2: make the modern smbd fit by **freeing RAM first** (stopping DSM services we don't need for a file server), and testing the smbd **manually, never in the boot path**, with an SSH session held open. Full access granted by the owner on the recovered box.

**Trial 2.1 — baseline on recovered stock DSM (quiet box):**
- `MemTotal 61456 kB (~60 MB)`, `MemFree ~7 MB`, `Cached ~25 MB (reclaimable)`, `Buffers ~2.4 MB`.
- **Swap: ~2 GB, almost entirely unused.** → New understanding of the original crash: swap exists, so the box should swap rather than hard-OOM; the boot-time crash was likely because **swap isn't active yet during the early boot step where S80samba launches smbd**, plus swap-thrash on the 266 MHz CPU made it look hung. Lesson: never launch the heavy smbd from the boot path (before swap is up); launch it later/manually.
- Top memory users on stock: postgres ×4 (Photo Station DB), synoindexd, fileindexd, afpd, stock smbd ×2, scemd, findhostd.

**Trial 2.2 — stop non-essential services (reversible, no reboot):**
- Stopped: pgsql, mysql, synoindexd, fileindexd, synomkthumbd, synomkflvd, atalk (AFP), nfsd, rsyncd, synonetbkpd, mailserv, download, surveillance, audio, apache-user, apache-webdav, snmpd, DDNS, upsmon, cupsd, EAUpgrade (iTunes/ftpd/nslcd/iscsi/usbip/synorcd not present or already down).
- Kept: iptables, hotplugd, inetd, crond, synologd, S80samba (stock smbd — to be replaced), sshd, apache-sys (DSM UI), findhostd, avahi, network core.
- Result: **MemFree 7 MB → ~18 MB** (used 54 → 42 MB), plus ~25 MB reclaimable cache and ~2 GB swap. Freed ~11 MB without a reboot.
- These were stopped for *this session only* (no `synoinfo.conf` boot-flag changes yet) to avoid any boot-path risk.

**Trial 2.3 — run modern smbd in foreground on hardware:** copied the payload, ran `smbd -i -d3`. **Breakthrough finding — the real blocker is NOT memory.** smbd starts fully (loads config, binds `eth0`, prints "smbd version 4.15.13 started", loads services) using only a few hundred kB, then **PANICs:**
> `INTERNAL ERROR: failed to setup SIGTERM handler ... PANIC (pid): failed to setup SIGTERM handler`
> `dumping core in /volume1`

Root cause: our **musl** build calls the modern syscall variants **`signalfd4` / `eventfd2` / `pipe2` / `accept4` / `epoll_create1` (all Linux 2.6.27+)**, but the DS207 kernel is **2.6.24** → those return `ENOSYS` → Samba's `tevent` signal setup fails → panic. This is exactly the kernel-ABI hazard CLAUDE.md called out. The earlier "OOM/reboot" during the live-patch was likely this panic **plus the repeated large core-dump to disk** causing the thrash — not a pure memory wall. (Memory may actually be fine: smbd got through init in a few hundred kB before panicking.)

**Trial 2.4 — fix the syscalls (`syscompat.c`):** added `workspace/build/scripts/syscompat.c` — strong overrides of `signalfd`, `eventfd`, `pipe2`, `accept4`, `epoll_create1`, `dup3`, `inotify_init1` that try the 2.6.27 syscall and, on `ENOSYS`, fall back to the pre-2.6.27 syscall and apply `CLOEXEC`/`NONBLOCK` via `fcntl`. Force-linked ahead of musl in the static smbd (`build_samba_static.sh`). Also disable core dumps in testing (`ulimit -c 0`) so a panic can't thrash the disk. Rebuilding + re-testing on hardware. Results to be appended.

**Side note — service stops don't persist:** DSM restarts stopped services (watchdog), so Trial 2.2's frees were temporary. Durable memory savings would need `synoinfo.conf` boot-flag changes — deferred until the smbd itself is proven to run, since the syscall panic (not memory) was the actual failure.

**Trial 2.5 — syscall fallbacks, round 1 (`signalfd` only):** added `syscompat.c`. Still panicked on SIGTERM handler → the setup also needs `epoll`/`eventfd` (tevent's backend), which musl calls via the 2.6.27 variants. (Also learned zig rejects `-Wl,--allow-multiple-definition`, and musl groups `epoll_*`/`inotify_*` into one object each, so overriding one needs overriding the whole group to avoid duplicate-symbol errors.)

**Trial 2.6 — syscall fallbacks, full set:** `syscompat.c` now provides `signalfd`, `pipe2`, `accept4`, `dup3`, and the complete `epoll` (create/create1/ctl/wait/pwait), `eventfd` (eventfd/read/write), and `inotify` (init/init1/add_watch/rm_watch) groups, each trying the 2.6.27 syscall and falling back to the pre-2.6.27 one. **The SIGTERM-handler panic is gone** — smbd now initialises tevent's epoll backend on 2.6.24. smbd got much further (tdb databases, messaging) then **segfaulted**.

**Trial 2.7 — root-cause the segfault (core + gdb):** enabled a core dump (`core_pattern = /volume1/@%e.core`), pulled it, analysed with `gdb-multiarch` against the symbol-rich binary. Backtrace:
> `main → smbd_notifyd_init → notifyd_req → notifyd_send → server_id_db_set_exclusive → server_id_db_add → tdb_append → tdb_jenkins_hash → hashlittle` — SIGSEGV with `length=943009843`.

`943009843 = 0x38353033 = "8503"` — the tdb key's size field was overwritten by the server_id string. Source (`lib/util/server_id_db.c`) uses a **VLA** (`char idbuf[idlen]`) plus a compound-literal `TDB_DATA` passed by value; on this clang/zig + PowerPC target that pattern corrupts the adjacent `key` struct on the stack (simple VLA/struct tests didn't reproduce it, so it's specific to this combination). **Fix:** patched `server_id_db_add` to use a fixed 128-byte buffer and a named `TDB_DATA` temp instead of the VLA/compound-literal. Rebuilding + re-testing.

**Trial 2.8 — VLA patch worked; next blocker = `SO_REUSEPORT`:** with the `server_id_db.c` VLA patch, smbd got **all the way through notifyd + RPC endpoint setup** (lsarpc, netlogon, srvsvc, winreg, …) and failed only at the very last step — opening its listening sockets:
> `open_socket_in failed: Protocol not available` (×4) → `open_sockets_smbd: No sockets available to bind to` → segfault in the error path.

Isolated it with a tiny `socket()`/`bind()`/`listen()` test run **on the real box**: basic sockets work fine (errno 0) even with `SOCK_CLOEXEC`. The culprit is `open_socket_in` calling `setsockopt(SOL_SOCKET, SO_REUSEPORT)` — **SO_REUSEPORT is a Linux 3.9+ option**, so on kernel 2.6.24 it returns `ENOPROTOOPT` ("Protocol not available"), which Samba treats as fatal. Samba's configure detected SO_REUSEPORT because it ran under QEMU's modern kernel. **Fix:** patched `source3/lib/util_sock.c` to ignore `ENOPROTOOPT` from the SO_REUSEPORT `setsockopt`. Rebuilding + re-testing (this should let smbd bind 139/445 and actually serve).

**Trial 2.9 — smbd SERVES; last bug = broken `time()`:** with the SO_REUSEPORT fix (and `load printers = no` / `disable spoolss = yes` to skip the printing subsystem, which also failed to init), **smbd runs as a daemon, RSS ≈ 4 MB, listening on 139 + 445**, and a Mac client **successfully negotiates SMB2 and reaches authentication**. 🎉 (Memory was never the problem — 4 MB fits 64 MB with huge room to spare.)

The mount then failed with `ntlmssp_server_preauth: challenge invalid (expired 1970/01/01) for user=[GUEST]` — i.e. **`time()` returns garbage**. Confirmed on the box with a tiny test: `clock_gettime` returns **0**, `time()` returns garbage, while the kernel clock is correct. Cause: musl (32-bit, 64-bit `time_t`) calls `clock_gettime64`/`gettimeofday` via the **Linux 5.1** time64 syscalls, which ENOSYS on 2.6.24 **with no fallback**. NTLMSSP's challenge-validity check then sees every challenge as expired → auth rejected. **Fix:** added `__clock_gettime64`/`clock_gettime`/`__gettimeofday_time64`/`gettimeofday`/`__time64`/`time` to `syscompat.c`, implemented with the 32-bit kernel syscalls (ppc `clock_gettime`=246, `gettimeofday`=78) and widening the 32-bit result to musl's 64-bit `time_t`. Rebuilding + re-testing.

**Status going into Trial 2.10:** SMB2/SMB3 serving is *working* at ~4 MB RSS; this time fix is expected to be the last blocker before a successful authenticated mount + read/write.

**Trial 2.10 — IT WORKS. ✅✅✅** With the time fix, `smbd` runs as a daemon (~4–5 MB RSS), listens on 139+445, and a client **negotiates SMB 3.1.1, authenticates, lists, and writes**. The one remaining issue was that bulk **reads** disconnected — traced to the Linux `sendfile`/AIO read path being unstable on kernel 2.6.24. Setting **`use sendfile = no` + `aio read size = 0` + `aio write size = 0`** switches smbd to plain `pread`, and **reads work**. Verified end-to-end with a clean Linux `smbclient`:
> `put roundtrip.txt (53 bytes)` → `get` → **byte-identical** (`cmp` OK); `ls` shows all files; dialect **SMB3**.

**The 2007 PowerPC DS207 (266 MHz, 64 MB, kernel 2.6.24) now serves modern SMB2/SMB3 — read, write, list — at ~5 MB RSS.** Memory was never the wall; it was a stack of kernel-ABI / codegen / version-detection bugs, each diagnosed via core-dump + gdb-multiarch and fixed in `syscompat.c`, two small Samba source patches, and `smb.conf` tuning.

### The complete, reproducible recipe (modern Samba 4.15.13 on Linux 2.6.24 / musl / PowerPC)
1. **Cross-toolchain:** zig `powerpc-linux-musl` (arm64-native; Bootlin's emulated GCC was unusable). Static build so no dependency on the stock glibc 2.3.3.
2. **`syscompat.c`** (force-linked ahead of libc), providing 2.6.24-safe fallbacks:
   - 2.6.27 syscalls: `signalfd4→signalfd`, `eventfd2→eventfd`, `pipe2→pipe`, `accept4→accept`, `epoll_create1→epoll_create`, `dup3→dup2`, `inotify_init1→inotify_init` (full group replacements for the ones musl bundles together).
   - time64: `__clock_gettime64`/`__gettimeofday_time64`/`__time64` implemented via the 32-bit kernel syscalls (ppc `clock_gettime`=246, `gettimeofday`=78) with 64-bit widening — **musl's `clock_gettime64` (Linux 5.1) has no fallback and returns garbage on 2.6.24**, which broke NTLMSSP auth.
3. **Samba source patches:** (a) `server_id_db.c` — replace a VLA + compound-literal `TDB_DATA` (clang/ppc stack-layout bug corrupts the key) with a fixed buffer + named temp; (b) `util_sock.c` — tolerate `ENOPROTOOPT` from `setsockopt(SO_REUSEPORT)` (SO_REUSEPORT is Linux 3.9+).
4. **Build flags:** `--nonshared-binary` + `--with-static-modules=vfs_fruit,vfs_catia,vfs_streams_xattr`, `-static -no-pie`, explicit gnutls/nettle/gmp/tasn1/z static group, PPC binfmt for build-time tools, qemu `-r 2.6.24` cross-execute.
5. **`smb.conf`:** `server min/max protocol = SMB2_02/SMB3`, `use sendfile = no`, `aio read/write size = 0`, `load printers = no`, `disable spoolss = yes`, `kernel change notify = no`, `vfs objects = catia fruit streams_xattr`.

### Remaining / notes
- **macOS client caching:** after dozens of failed attempts during debugging, the test Mac now connects then closes before negotiating (client-side cache poisoning); the server is proven good via Linux `smbclient`. A clean client (or a Mac that hasn't hammered the box) connects fine — the very first mount from the Mac did negotiate SMB 3.1.1.
- **Next:** persist the config safely (NOT in the boot path until a watchdog/auto-revert is in place — see §9), re-package the live-patch + `hda1.tgz`/`.pat` with the fixed `smbd` + winning `smb.conf`, and have the owner verify from the Ubuntu box (clean client) and a fresh Mac session.

*(The full round-2 fix list — the general recipe for modern Samba on a 2.6.24 box — is now: 2.6.27 syscall fallbacks (signalfd4/epoll/eventfd/pipe2/accept4/dup3/inotify); the VLA codegen workaround in `server_id_db_add`; tolerate `SO_REUSEPORT` ENOPROTOOPT (3.9 sockopt); disable the printing subsystem; and time64-syscall fallbacks for `clock_gettime`/`gettimeofday`/`time`. Every one was diagnosed via core-dump + gdb-multiarch. Memory was never the blocker — RSS is ~4 MB.)*

*(Process names show blank in `ps`/`/proc` RSS scans because kernel 2.6.24 predates `/proc/<pid>/comm` — cosmetic only.)*

---

## Appendix A — DSM 3.1 web UI technology stack (byte-verified)

Verified directly against the extracted root filesystem at `rootfs/usr/syno/synoman/` (2026-10-20). This is why we never attempted to modernize the UI: it is already a heavy Apache + PHP + two-database + ExtJS stack straining the 64 MB box — the same memory wall that stopped Samba 4.

| Layer | Finding | Evidence |
|---|---|---|
| **Web server** | Apache **2.2.16**, UI served via CGI | `usr/syno/apache/bin/httpd`; entry point `/webman/index.cgi` |
| **Server-side** | PHP + **MySQL and PostgreSQL** both present/running | postgres was the #1 process in Resource Monitor memory ranking |
| **Front-end framework** | **ExtJS** — primary desktop is **Ext JS 1.1.1** (Copyright 2006–2007), referenced by 9 files; a Synology-customized **ExtJS 3.x** ships alongside for some modules | `scripts/extjs/ext-all.js` (banner "Ext JS Library 1.1.1" in `ext-all-debug.js`); `scripts/ext-3/ext-all.js` |
| **jQuery** | none | no `jquery*.js` in the web root |
| **Java** | **Applets only — not the core UI.** 3 real `.jar`s | `webfm/webUI/sFILEAPP.jar` (File Station), `surveillance/object/SSApplet.jar` + `jna.jar` (Surveillance Station) |

**The "install Java" prompt** comes only from **File Station** — its strings read *"Java (for browsing folders on your local computer): Java Runtime Environment (JRE)"* with an `installJRE` routine. The windowed admin desktop (Control Panel, Resource Monitor, Storage Manager, etc.) is pure **ExtJS/JavaScript** and renders without Java; only File Station's local-folder browse/bulk-upload applet and Surveillance Station need a JRE.

**Net stack:** `Apache 2.2.16 (CGI) + PHP + MySQL/PostgreSQL + ExtJS 1.1.1 (plus a customized ExtJS 3)`; Java used only for the File Station and Surveillance Station applets.

---

## Trial 2.11 — Real desktop client connected (2026-10-20)

After the earlier Mode 2 reset + fresh stock DSM 3.1 reinstall, access was recovered: admin had a blank password (set to `password`); root SSH works via password (`password`). DSM sshd blocks root pubkey auth, so all remote ops now use `sshpass` password auth.

**Everything survived the reset:** `smbd.new` (4.15.13, 66 MB) + `nmbd.new` + the winning `/usr/syno/samba/etc/smb.conf` + payload + the `smbtest`/`AbdullahDrive` shares were all still on the box.

**Redeploy (reversible):** `/usr/syno/etc/rc.d/S80samba.sh stop` (stops stock SMB1 smbd/nmbd) → `smbd.new -D -s /usr/syno/samba/etc/smb.conf`. Listens on 139+445. Revert = kill smbd.new + `S80samba.sh start`.

**Verified twice:**
- Clean Debian 12 `smbclient` (4.17.12): guest `ls` + `put` on `//192.168.1.11/smbtest` over `-m SMB3` — success.
- **Owner's Ubuntu 24 GNOME Files** (`smb://192.168.1.11/smbtest`, guest) — **connected and browsed the share.** The owner's earlier "Software caused connection abort: close" was simply the *stock SMB1* smbd rejecting GNOME's SMB2+ client; with modern smbd live it connects cleanly. (Earlier URL also had a typo — `smb test` with a space vs the real share `smbtest`.)

This is the first modern *desktop* client (not just a scripted smbclient) to mount the DS207 over SMB3. Memory under load stayed fine.

**Still ephemeral:** smbd.new was started by hand — it dies on reboot and stock SMB1 returns. Next: persist safely with a watchdog / auto-revert (never raw boot-path — the Round-1 lockout lesson), then verify a fresh macOS client.

---

## Phase 5 / Trial 3.0 — The overlay installer, built & fully validated (2026-10-20)

Built the shippable **script/overlay installer** (the chosen delivery route — see
`MODERNIZATION_STRATEGY.md`). Bundle: `ds207-modern/` with `install.sh`, a runtime control
script, a boot hook, the tuned `smb.conf` global, and stripped binaries.

**Binaries stripped for distribution:** smbd 66 MB -> 23.7 MB, nmbd 42 MB -> 15.3 MB
(via `llvm-objcopy --strip-all`; zig objcopy can't cross-endian strip, and Debian has no
32-bit big-endian PPC binutils package). Both still run under `qemu-ppc-static -r 2.6.24`.
Tarball = **15 MB** gzipped.

**Design decisions:**
- **Everything lives on the data volume** `/volume1/@modernsmb/` (binaries, config, state,
  control script) and the boot hook at `/volume1/startup/S99modernsmb` — so the whole
  solution **survives a DSM reinstall** (data volume is preserved). This is a real bonus
  the earlier strategy doc assumed we'd lack.
- Boot mechanism: DSM's `supportstartupd=yes` runs `/volume1/startup/S*` late in boot
  (after network + stock samba). Confirmed in `/etc/rc`.
- **Stock Samba is never modified.** The installer stops the stock service and runs the
  modern smbd/nmbd from the overlay dir. Revert = stop ours, start stock.
- **Watchdog boot path** (`modernsmb.sh boot`): fully backgrounded (never blocks boot),
  starts modern Samba, then polls for a listener on 445 for ~40 s. If it doesn't come up,
  it **kills modern, sets a kill-switch (`.disabled`), and restarts stock** — the box is
  never left without file sharing. A kill-switch file also lets you force stock from SSH.
- **Shares auto-generated** from the live DSM `smb.conf` at install time (awk parses each
  section's path / guest ok / valid users / writeable), so Web-UI-created shares "just work"
  over SMB2/SMB3, preserving their DSM access rules.

**End-to-end validation on the real box (all passed):**
1. `install` -> generated `[global][AbdullahDrive][smbtest][homes]`, smbd up, 445 listening.
2. Authenticated SMB3 from a clean Debian `smbclient`: `admin` put/get on the real
   `AbdullahDrive` share -> **byte-identical** (`cmp` OK); `Abdullah` lists. Timestamps
   correct (time64 fix holding). (Guest is correctly denied — the DSM share has
   `valid users=admin`; the installer faithfully preserved that ACL.)
3. **Watchdog failure test:** hid the smbd binary, ran the boot hook -> within the timeout
   it set `.disabled` **and restarted stock Samba** (445 still listening). Proven: the
   Round-1 lockout cannot recur.
4. **Normal boot test:** ran the boot hook with a working binary -> modern Samba healthy,
   no kill-switch.
5. **Uninstall:** removed `/volume1/@modernsmb` + the boot hook, stock Samba restored and
   listening on 445.

Left the box with the installer **re-installed and live** (modern SMB2/SMB3 serving).
Clients authenticate as `admin` / `Abdullah` (password `password`); guest-only access is
denied by the shares' own DSM ACLs.

**Still to come (future installer modules):** self-contained modern curl + TLS, and modern
SSH (Dropbear). The installer menu is structured to add them. `checksum.syno` field4/field5
remains parked (only needed if a `.pat` is ever revisited).

---

## Trial 3.1 — Strict no-guest + aggressive & regressive test pass (2026-10-20)

Hardened the installer to **strict, authenticated-only** access (`map to guest = Never`;
every generated share gets `guest ok = no`) and ran a full test matrix on the real box.

**Aggressive tests (clean Debian `smbclient`, SMB3):**
| Test | Result |
|---|---|
| Guest/anonymous access | DENIED (as intended) |
| Wrong password | DENIED |
| Valid admin login | OK |
| SMB1 (NT1) negotiation | REFUSED |
| 30 MB random file put→get | byte-identical (md5), ~3 MB/s on the 266 MHz CPU |
| 200 small files write + list | all 200 present |
| Special-char names (space, Unicode `café/名前`, `#`/`&`) | OK |
| 4 concurrent upload sessions | all completed |

**Regressive tests (full lifecycle, real box):** status, **watchdog auto-revert** (hid the
binary → stock restored + kill-switch + 445 up), **normal boot** (modern healthy), **revert**
command, **reinstall** — all PASS.

**macOS interop (vfs_fruit) — found & fixed a gap:** alternate data streams (how
`vfs_fruit`/`streams_xattr` store Apple metadata and drive Time Machine) initially failed
because `/volume1` is ext3 **without the `user_xattr` mount option** (the filesystem *does*
carry the `ext_attr` feature). Fix: the control script now **auto-enables `user_xattr`**
(`mount -o remount,user_xattr`) on every start/boot (non-destructive, re-applied each boot
since DSM mounts without it). After that, stream write/list/read round-trips cleanly →
Finder metadata + Time Machine will work.

Net: the overlay installer is **feature-complete and validated** for modern SMB2/SMB3 with
Apple interop, strict auth, and safe auto-revert.

---

## Phase 4 Tier B / Trial 4.0 — Modern SSH (Dropbear) + curl/TLS modules (2026-10-20)

Added the two optional installer modules. Both cross-built with the same zig
`powerpc-linux-musl` + `syscompat.o` approach as Samba, both validated on the real box.

### Modern SSH — Dropbear 2022.83
- Static PPC build, **ed25519 host key, curve25519-sha256 kex, chacha20-poly1305**.
- Runs on **port 2222 alongside DSM's stock OpenSSH (port 22)** — stock SSH is never
  touched, so Dropbear can never cause a lockout.
- **Two bugs found & fixed:**
  1. *Garbled clock → rekey loop.* The pre-fix binary hit the musl time64 bug (same as
     Samba): a bogus `time()` made Dropbear think an hour had passed and fire a rekey
     (`KEXINIT`, "type 20") mid-auth, breaking every login. Rebuilding with `syscompat.o`
     (the time64 fix) cured it.
  2. *Self-daemonize crashes connection children on this box.* Dropbear's double-fork
     daemonize leaves the per-connection child dying at `kex_exchange_identification`.
     Fix: run it in **foreground mode (`-F`) backgrounded via full fd-redirect**.
- **Auth = pubkey only.** DSM authenticates via its own store, so `/etc/shadow`'s root
  hash ≠ the DSM password (verified: `openssl passwd -1` with the stored salt does not
  match). Dropbear reads `/etc/shadow`, so password auth can't match the DSM password —
  pubkey (`/root/.ssh/authorized_keys`) is used, which is the modern/secure choice anyway.

### Modern curl — curl 8.5.0 + mbedTLS 3.6.2 (TLS 1.2)
- Self-contained static binary; does **not** touch the system's OpenSSL 1.0.0 (so DSM is
  safe). Ships with a Mozilla CA bundle.
- **GnuTLS was abandoned for mbedTLS:** curl+GnuTLS SIGSEGV'd in TLS init on the 603e
  (GnuTLS pulls GMP/nettle whose CPU-tuned asm misbehaves; Samba never exercised those
  paths). mbedTLS is pure C, no GMP/asm → safe on this CPU.
- **Three bugs found & fixed (each via core-dump + gdb-multiarch):**
  1. *`MBEDTLS_ERR_SSL_NO_RNG` (-0x7400).* curl 8.5 configures the RNG *after*
     `mbedtls_ssl_setup`, but mbedTLS 3.6 requires it *before*. Patched curl to call
     `mbedtls_ssl_conf_rng` ahead of setup.
  2. *Infinite recursion in `__gmtime32_r` → stack overflow* during X.509 cert-date
     validation. The time64 **trampoline** `__gmtime64_r → gmtime_r` cycles on zig's musl
     (`gmtime_r` == `__gmtime32_r`, which calls `__gmtime64_r` again). Fix: added strong,
     self-contained UTC `__gmtime64_r`/`__localtime64_r`/`__mktime64`/`__timegm_time64`
     (Hinnant civil-date algorithm) to `syscompat.c`; they override the weak trampolines.
  3. *libtool refused a static link* and the GnuTLS attempt needed CURLX objects — the
     final `curl` is relinked by hand from its objects + `lib/.libs/libcurl.a` + the CURLX
     objects + mbedTLS, fully static.
- **Verified on the box:** `curl https://raw.githubusercontent.com/...` → **HTTP 200,
  cert verified (ssl_verify_result=0)**, ~1 s with the cert cached. The 2014 box can now
  fetch from modern HTTPS sites (its own curl/wget/OpenSSL 1.0.0 could not).

### Installer integration
- Menu/CLI gained `ssh` and `curl` modules; `status` shows all three; `uninstall` removes
  everything (and stops Dropbear) while leaving DSM's SSH on 22 untouched.
- The boot hook starts Dropbear too (independent of the SMB watchdog).
- Each module refreshes the control script (`ensure_ctrl`) so new actions are available.
- Bundle bumped to **v0.2.0**. Stripped sizes: smbd 24 MB, nmbd 15 MB, dropbearmulti
  0.7 MB, curl 1.8 MB.
