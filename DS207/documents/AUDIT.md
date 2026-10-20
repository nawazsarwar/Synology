# DS207 Firmware Modernization — Forensic Audit (Phase 1 & 2)

**Date:** 2026-10-19
**Target unit:** Synology DS207 (PowerPC `ppc824x`) — confirmed by user.
**Goal:** Allow modern Windows 11 / macOS / Linux clients to mount shares over SMB2/SMB3 (+ Apple `vfs_fruit`) and native mDNS, by cross-compiling new binaries into `hda1.tgz` (never transplanting foreign-arch ELFs; never touching `zImage`/`rd.bin`).

## Input files

| File | `unique` | CPU | Format | Role |
|---|---|---|---|---|
| `DS207/DSM_DS207_1639.pat` | `synology_ppc824x_207` | PowerPC MPC8241 (603e), big-endian, hard-float, ELF32 SysV | plain GNU tar (extracted) | **Build target** |
| `DS207/DSM_DS207+_1636.pat` | `synology_88f5281_207+` | Marvell 88f5281 ARMv5te | plain GNU tar (extracted) | Sibling; not used |
| `DS216/DSM_DS216+II_90080.pat` | DS216+II | Intel x86_64 | **Encrypted** (magic `0x0cadbeef`, DSM 6.2.4+/7.x) | Modern config donor |

**Premise correction:** The two DS207 `.pat` files are both DSM 3.1 (2011/2014), same hardware generation — the `DS207+` is not a "modern" NAS, just the ARM sibling. The genuinely modern reference is the DS216+II pat, which is Intel x86_64 and runs modern DSM (Samba 4.x, `vfs_fruit`, Avahi Time Machine). Its binaries cannot be transplanted to PPC, but its configuration files are architecture-independent and reusable.

## Phase 1 — Architecture identification (DS207)

- `bin/busybox`: ELF 32-bit MSB (big-endian) PowerPC, statically linked, stripped.
- `usr/syno/sbin/smbd`: ELF 32-bit MSB PowerPC, dynamically linked, interpreter `/lib/ld.so.1`, stripped.
- **Arch:** PowerPC `ppc824x` (MPC8241, 603e core) — big-endian, hard-float (classic 603e FPU), ELF32 SysV ABI.
- **Kernel:** 2.6.24 (from `zImage` strings). Note: this is higher than the 2.6.15 assumed in CLAUDE.md — cross-toolchain should target the 2.6.24 syscall ABI.
- **libc:** glibc 2.3.3 (dynamic loader `/lib/ld.so.1`).
- `readelf -A` not yet run (binutils not installed); PPC ELF has no `.ARM.attributes` section regardless.

## Phase 2 — Capability audit (DS207 `hda1.tgz` rootfs)

| Component | Version | Modern-client blocker |
|---|---|---|
| Samba `smbd` | **3.2.8** (source tree `samba-3.2.2`) | No SMB2/SMB3 compiled in (zero `SMB2*` strings). SMB1/NT1 only — rejected by default on Win11 and macOS. |
| Netatalk `afpd` | 2.x (advertises AFPVersion 2.0 / 2.1) | DHX/DHX2 auth removed from macOS 11+. AFP effectively dead on modern macOS. |
| OpenSSH `sshd` | 5.2p1 | Legacy KEX/ciphers; no Ed25519/Curve25519. |
| OpenSSL | 1.0.0n (2014-08-06) | EOL; weak cipher set. |
| Avahi `avahi-daemon` | 0.6.28 | Present (`S99avahi.sh`), but no SMB / Time Machine `.service` files. |
| Dropbear | Not present | Synology ships full OpenSSH instead. |

**Implication for the two-tier plan:**
- **Tier A (unlock native SMB2 in stock binary) is impossible** — Samba 3.2.8 predates SMB2 entirely.
- All modernization must come from **Tier B** (cross-compile new Samba 4.x + Avahi service files + modern SSH config).

### smb.conf management & persistence
- Active config: `usr/syno/etc/smb.conf` (on-disk, inside `hda1.tgz`). Reset copy: `usr/syno/etc.defaults/smb.conf`.
- The DSM Web UI edits individual keys via `set_key_value`; it does **not** regenerate the file from scratch each boot.
- Startup: `usr/syno/etc/rc.d/S80samba.sh` launches `smbd -D` (line ~172).
- **Persistence hook:** `S80samba.sh` lives in `hda1.tgz` on the SATA root, so edits there survive reboot. Plan: point it at the custom `smbd` and a dedicated custom config so Web UI edits to stock keys do not clobber our settings.

### checksum.syno
Per-file line format: `<field1> <size> <name> <field4> <field5>`; trailer `#Synocksum <Σsize> <Σfield4> <Σfield5>`.
- **field1 = standard zlib CRC32** — verified exact against all five members.
- **field2 = byte size** — verified.
- **trailer = column sums** of size / field4 / field5 — verified.
- field4 (~28-bit, scales with size) and field5 (<1024) not yet identified; not CRC32/Adler32/Fletcher/CRC16/simple-sum variants tested.
- **Decision:** do not ship a guessed security checksum. Regenerate `checksum.syno` by running Synology's own generator embedded in the `updater` ELF under QEMU, rather than reverse-engineering field4/field5. CRC32 + size already cover real integrity.

## Environment gaps (block Phase 3+)
Missing on the host Mac: binutils (`readelf`), `binwalk`, `qemu-ppc`/`qemu-system-ppc`, Docker, cross-toolchain. Working: tar, `strings`, `file`, `python3`.

Install plan (user runs):
```
brew install qemu binutils lz4 xz
brew install --cask docker   # then launch Docker Desktop once
```
Encrypted DS216 pat requires the community `synology-decrypt` Python tool (or a Docker container); fetch and source-review before running (untrusted).

## Guardrails (from CLAUDE.md) — reaffirmed
1. Never transplant foreign-arch ELFs (x86_64/ARM) onto PPC.
2. Never modify `zImage` or `rd.bin` (brick risk). All changes stay in `hda1.tgz` / `/opt` overlay.
3. Target kernel ABI 2.6.24 (observed), glibc 2.3.3 / musl / uClibc-ng with 2.6.x-safe syscall fallbacks.
4. RAM ceiling 64–128 MB: strip binaries, disable AD DC / Python / CUPS / LDAP in Samba, tune `smb.conf` for low memory.
5. QEMU pre-flight (`qemu-ppc -r 2.6.24`) every new binary before packaging.
6. Regenerate `checksum.syno` so the DSM updater accepts the repacked pat.

## Phase 3 — Emulation harness (DONE, verified 2026-10-19)

Host Mac is Apple Silicon (arm64), macOS 27. Installed via brew: `qemu` (system only — **no `qemu-ppc` user-mode on macOS**, it is Linux-only), `binutils` (readelf 2.47), `colima` + `docker` CLI, `libsodium`. Container runtime = colima (aarch64 VM, virtiofs mounts). The project dir is mounted writable into the VM: `colima start --mount /Volumes/WDData2TB/Projects/Synology:w`.

QEMU PPC user-mode runs **inside a Debian container** (`qemu-user-static` 7.2), not on the host. Image `ds207-qemu` (arm64) runs `qemu-ppc-static` natively. Verified against the stock DS207 rootfs:
- `busybox` (static) under `qemu-ppc-static -r 2.6.24` → BusyBox v1.16.1 banner. ✅
- `busybox uname -a` → `... 2.6.24 ... ppc GNU/Linux` — kernel spoof + arch confirmed. ✅
- `lib/libc.so.6` executed → "GNU C Library stable release version 2.3.3". ✅
- Stock `smbd -V` **segfaults** (RC=139) under emulation — binary-specific (glibc-2.3.3 + krb5 TLS init in a 5 MB dynamic binary); not a blocker since stock Samba 3.2.8 is being replaced, not reused.

## Phase 4 — Cross-compile pipeline bootstrap (DONE, verified 2026-10-19)

Debian's `gcc-powerpc-linux-gnu` is rejected: it targets modern glibc (kernel ≥ 3.2) → "FATAL: kernel too old" on 2.6.24. Chosen toolchain: **Bootlin `powerpc-e300c3--musl--stable-2024.02-1`** (gcc 12.3, musl 1.2.x, big-endian; e300c3 is the 603e family, run with `-mcpu=603e`). Bootlin binaries are x86_64-hosted, so the build container is **amd64 (emulated on the arm64 VM)**.

Images / artifacts (persist in the colima VM across sessions):
- `ds207-ppc:amd64` — amd64 build image, toolchain at `/opt/tc/bin/powerpc-linux-gcc` (triple `powerpc-buildroot-linux-musl`).
- `ds207-qemu` — arm64 test image (`qemu-ppc-static`).
- docker volume `ds207vol` — mounted `/out`, used to hand built binaries from the amd64 build container to the arm64 test container (virtiofs bind mounts mangle cross-container file perms and cannot hardlink, so toolchain lives in-image and binaries move via this volume).

**End-to-end loop proven:** amd64 container compiled `powerpc-linux-gcc -static -mcpu=603e -O2 -s` → static ELF32 MSB PowerPC → arm64 container ran it under `qemu-ppc-static -r 2.6.24` → printed `kernel=2.6.24 machine=ppc`, RC=0. Note: running `qemu-ppc` *inside* the emulated amd64 container fails (nested-emulation VA reservation), hence the build/test split across the two images.

## Remaining work
5. Cross-compile the real daemons as static musl PPC, QEMU-test each on `-r 2.6.24`:
   - Dropbear SSH (smallest; prove a real daemon first).
   - Avahi / mDNS (macOS discovery + Time Machine advertisement).
   - Samba 4.x with SMB2/SMB3 + `vfs_fruit` (hardest: waf cross-build, bundled talloc/tdb/tevent/ldb, gnutls for SMB3 crypto; strip + trim per RAM guardrail).
6. Author modern configs for DS207: `smb.conf` (`min protocol = SMB2`, `max protocol = SMB3`, `vfs objects = catia fruit streams_xattr`, fruit settings), Avahi `_smb._tcp` + `_adisk._tcp` Time Machine `.service`. Reference base `smb.conf` extracted from DS216 (DSM 7.4.1) = `min SMB2 / max SMB3`.
7. Inject into `hda1.tgz` with stock binaries preserved as `.orig`; hook `S80samba.sh`. Regenerate `checksum.syno` (field1 CRC32 confirmed; field4/5 via vendor `updater` under QEMU). Emit `DS207_Modernized_Custom.pat` + `ds207_live_patch.sh`.

## DS216 modern reference (decrypted 2026-10-19)
DS216+II = DSM **7.4.1-90080**, Intel braswell x86_64. Decrypted with Synacktiv-derived `patology` (magic `0xadbeef`, embedded public/subkey, XChaCha20-Poly1305 → tar; source-reviewed, no network/eval). Samba is now the **SMBService 4.15.13** package (separate `.spk`). The `.spk` uses a **different signing keytype** than the `.pat`, so `patology` can't open it (would need `keys.json`); not needed — base `smb.conf` + standard upstream fruit/Time Machine settings suffice. The Time Machine Avahi `.service` files are generated at DSM runtime, not shipped static.
