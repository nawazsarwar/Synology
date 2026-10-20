# DS207 Modernization — Strategy, Component Assessment & Delivery Decision

_Last updated: 2026-10-20_

This document captures the architectural decision for how the DS207 modernization
is shipped, a component-by-component risk assessment, and the reasoning behind
choosing a **script/overlay installer** over a rebuilt `.pat` firmware file.

---

## 1. The hard constraint: the kernel is frozen

| | |
|---|---|
| **Kernel version** | Linux **2.6.24** (released Jan 2008; Synology build #1639, 2014) |
| **Can it be updated?** | **No, practically.** The kernel lives in flash (`zImage`), which our safety rules forbid touching (brick risk). There is no vendor kernel source for this exact SoC, and building a modern kernel would be a months-long board-support-package (BSP) effort. |
| **Risk if the NAS is exposed to the internet** | **High** — 16+ years of unpatched kernel CVEs. |
| **Risk on a trusted LAN (behind a router, no port-forwarding)** | **Low–moderate** — nothing off-network can reach it. |
| **Mitigation / usage rule** | **LAN-only.** Do not port-forward or expose to the public internet. Under that condition the box is fine for home/small-office use. |

**Consequence:** everything we modernize lives on the **data disk** (the root filesystem
installed to the SATA drive), never in flash. This is both the safety guarantee
(*can't brick what you don't touch*) and the ceiling (*can't fix the kernel*).

---

## 2. Component-by-component assessment

| Component | Current | Risk if left as-is | Modernize? | Difficulty | Time | Success odds |
|---|---|---|---|---|---|---|
| **Samba (file sharing)** | 3.x, SMB1 only | High — SMB1 insecure, blocked by modern Windows/macOS | ✅ **Done** — Samba 4.15.13 (SMB2/SMB3 + vfs_fruit) built & verified running | — | — | **Done (100%)** |
| **SSH** | OpenSSH 5.2p1 (2009) | Low–moderate — works, deprecated host-key signatures (ssh-rsa/SHA-1, DSA) | ✅ Yes — ship modern Dropbear (ed25519/curve25519), self-contained | Low–Medium | ~1 day | **High (~90%)** |
| **TLS client (curl/wget)** | OpenSSL 1.0.0n — cannot do TLS 1.2 | Moderate — box cannot fetch from modern HTTPS sites | ✅ Yes — ship a **self-contained** modern curl + CA bundle (static; does not touch system libs) | Medium | 1–2 days | **High (~85%)** |
| **DSM's own web crypto** | Same old OpenSSL, linked into DSM UI binaries | Moderate | ⚠️ Risky — DSM binaries link the old `libssl.so.1.0.0`; replacing it can break the UI | High | — | **Low — not recommended** |
| **Linux kernel** | 2.6.24 (2008) | High if exposed; low on LAN | ❌ No (flash / brick / no source) | — | — | **Not feasible** |

### Notes on the modern curl + TLS (self-contained)
- Supports **TLS 1.2 and TLS 1.3**, modern cipher suites (ECDHE, AES-GCM, ChaCha20-Poly1305),
  SNI, and full certificate validation against a shipped Mozilla CA bundle.
- Built **statically** — its own TLS library is baked in; it does **not** replace the
  system OpenSSL, so DSM cannot break. Only our curl uses the new crypto.
- **Caveat — speed:** crypto runs in software on a 266 MHz CPU with no hardware
  acceleration, so handshakes/downloads are **slow** (fine for update bundles, not bulk traffic).
- **Caveat — CA freshness:** the CA bundle is a file that should be refreshed occasionally.
- Built with the same zig `powerpc-linux-musl` toolchain that already produced working
  Samba 4, so the approach is proven.

### Why SSH works but HTTPS doesn't (same old OpenSSL)
SSH does **not** use TLS. It negotiates its own ciphers, and OpenSSH 5.2 supports ones
modern clients still accept (`aes128-ctr` + `diffie-hellman-group-exchange-sha256` were
negotiated in testing). HTTPS to GitHub **requires TLS 1.2**, which OpenSSL 1.0.0 cannot
speak — hence `curl https://github.com` fails the handshake and old `wget` segfaults.

---

## 3. Delivery route: `.pat` file vs script/overlay installer

| Aspect | Full `.pat` firmware | **Script + overlay (chosen)** |
|---|---|---|
| How the user installs | One-click in Synology Assistant / DSM UI | Copy a zip to the NAS → paste one command in SSH |
| Touches flash (kernel/bootloader)? | **Yes** (re-flashes `zImage`/`rd.bin`, even if unchanged) | **No** — data disk only |
| Brick risk | **Real** (any flash write can fail) | **None** |
| Blockers remaining | Crack `checksum.syno` field4/field5 **and** confirm the installer's signature check | **None — already proven working** |
| Reversible / uninstall | Hard (must reinstall stock `.pat`) | Easy (script backs up & restores stock) |
| Survives a DSM factory reset? | Yes (it *is* the firmware) | No — re-run the script |
| Difficulty | **High** | **Low** |
| Time to ship | Several days–weeks | **~1 day** |
| Success odds | **Medium–low (~50%)** | **High (~95%)** |
| Violates safety rule #2 (never touch flash)? | **Yes** | No |

### `checksum.syno` — what we learned (for the record)
Format decoded: `field1 field2 filename field4 field5` per file, with a footer
`#Synocksum <Σsize> <Σfield4> <Σfield5>`.
- **field1 = zlib CRC32** of the file (confirmed on all 5 members).
- **field2 = file size in bytes** (confirmed on all 5).
- **field4 / field5** = per-file additive quantities (footer totals verified exactly);
  the precise formula lives in the `updater` binary and was **not** fully reversed.
- The `updater` strings also reference `publickey` / `UPDTVerifyFile`, so a signature
  check may exist — another reason the `.pat` route is uncertain.

This work is parked, not lost — if a `.pat` is ever needed, field4/field5 can be
recovered by disassembling `updater`.

---

## 4. Decision & recommendation

**Chosen route: script/overlay installer.** Rationale:
- The kernel can't be modernized anyway, so a full firmware rebuild is over-engineering.
- The overlay approach is **proven, safe (no flash), fast, reversible, and shareable**.
- Its one downside (doesn't survive a factory reset) is handled by simply re-running the script.

**First installer scope (agreed):** a single installer with a menu —
1. **Modern Samba** (SMB2/SMB3 + Apple vfs_fruit) — ready.
2. **Modern curl + TLS** (self-contained) — enables secure downloads / future HTTPS updates.
3. **Modern SSH** (Dropbear, ed25519).
4. **Uninstall / revert to stock.**

**Built-in safety (the Round-1 lockout lesson):** the installer backs up stock binaries,
installs the new ones, wires up a late-boot auto-start **with a watchdog / auto-revert**
(if the modern daemon fails to come up, stock service is restored so the box is never
left without file sharing), and provides a kill-switch file to force stock from SSH.

**Distribution model:** download a release zip on a modern machine → copy to the NAS
(over the now-working SMB share, or File Station) → paste one command to run the installer.
The NAS does **not** need internet for the first install (its old TLS can't reach GitHub);
once the bundled modern curl is installed, future updates can use real HTTPS.

**GitHub note:** only our own source (scripts, configs, docs, installer) is published.
Synology's proprietary firmware (`.pat`, extracted rootfs, toolchains) is **never** committed.
