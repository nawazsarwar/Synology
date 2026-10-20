# Synology DS207 Modernization — Modern SMB2/SMB3, SSH & TLS for an End-of-Life NAS

> **In one sentence:** This is a free, open-source, reversible way to make an old,
> end-of-life Synology DS207 NAS work with modern Windows 11, macOS, and Linux again —
> by installing current Samba (SMB2/SMB3), Dropbear SSH, and TLS as a safe overlay on the
> data volume, **without reflashing firmware and without any brick risk.**

Bringing modern, secure file sharing to a **2007 Synology DS207** (PowerPC `mpc824x`,
266 MHz, 64 MB RAM, Linux 2.6.24, DSM 3.1) — so current **Windows 11, macOS, and Linux**
clients can mount network shares over **SMB2 / SMB3** (with Apple `vfs_fruit` and
Time Machine support), **without touching the device's flash (no brick risk).** If you have
a retired, unsupported Synology NAS that modern computers can no longer connect to because
it only speaks the old, insecure SMB1 protocol, this project is for you.

**Topics:** Synology · DS207 · NAS · end-of-life NAS · repurpose old NAS · SMB1 to SMB2/SMB3
· modern Samba on old hardware · DSM 3.1 · PowerPC · Windows 11 Synology SMB1 fix · macOS
Finder SMB · Time Machine over SMB · cross-compilation · musl · zig cc · LAN file server.

## Why this project exists

Synology declared the DS207 end-of-life years ago. No more firmware, no more feature
updates, and — critically — **no more security updates**. The official answer is "buy a new
one." For a huge number of these boxes, that answer no longer makes sense.

The hardware still works. A DS207 that has spent fifteen-plus years quietly serving files
has no moving part that has failed — the disks are replaceable, the board is fine. What
rotted was the *software*: an ancient Samba that modern Windows 11 and macOS refuse to talk
to, dead TLS, and SSH ciphers no current client will negotiate. That is a software problem,
and software is fixable.

And the timing has never been better for keeping old hardware alive:

- **Component prices are climbing, not falling.** The AI build-out is consuming enormous
  volumes of DRAM, NAND flash, and fab capacity. Memory and SSD prices have spiked, and new
  NAS units — along with the drives and PC parts that go in them — are caught in the same
  semiconductor squeeze. The cheap-upgrade era is, for now, over.
- **Replacing a working NAS is the expensive path.** A new unit plus new drives can cost
  more than it did a few years ago, to do a job your existing box already does well on a LAN.
- **There are hundreds of thousands of these units in the world**, sitting in closets and
  on shelves, retired only because their software went stale. That is a lot of capable
  hardware — and a lot of e-waste — waiting on a stack refresh it never got.

So the case is simple: **repurpose, don't replace.** If a reversible, LAN-safe software
refresh can let these machines mount shares securely from today's clients again, then a
retired NAS becomes a working one — no new silicon, no landfill, no waiting out a shortage.
This project exists to give those boxes another breath.

> **What this is, honestly.** We do **not** redistribute a modified Synology firmware image,
> and we do **not** repackage or re-sign a `.pat` file. We modernize the software stack *on
> top of* an already-installed DSM — dropping in current, self-compiled user-space services
> (Samba SMB2/3, Dropbear SSH, curl + TLS) that run *instead of* the stock obsolete ones.
> The stock binaries and Synology's original files are never modified; they stay in place as
> a fallback. This is a surgical, reversible overlay of the exposed network services — the
> realistic attack surface on a LAN — not a claim to have patched every CVE in a 2008 OS.
> The **kernel stays the old Linux 2.6.24** (it lives in flash and cannot be updated), which
> is exactly why the hard rule is **LAN-only, never exposed to the internet** (see below).

## Status

- ✅ **Samba 4.15.13 (SMB2/SMB3)** cross-compiled for PowerPC and **verified running**
  on the real DS207 at ~5 MB RAM. Byte-perfect round-trips confirmed from Linux
  `smbclient` and a real Ubuntu GNOME desktop.
- ✅ **Overlay installer built & validated** — a safe, reversible installer that runs from
  the DSM SSH terminal, installs everything on the data volume, auto-generates shares from
  your DSM config (**authenticated users only, no guest**), and auto-reverts to stock if
  anything fails at boot. **Install guide: [`INSTALL.md`](INSTALL.md).**
- ✅ **Modern SSH** — Dropbear 2022.83 (ed25519 / curve25519 / chacha20) on port 2222,
  key-based login, running alongside DSM's own SSH (no lockout risk).
- ✅ **Modern curl + TLS** — curl 8.5 + mbedTLS 3.6 (TLS 1.2), self-contained; the 2014
  box can finally fetch from modern HTTPS sites (verified: HTTP 200 from GitHub, cert OK).
- 🔭 Under evaluation: refreshing the DSM web UI (new lightweight JS front-end over the
  existing C-CGI endpoints).

The "64 MB is too small" assumption turned out to be a myth — the real blockers were a
stack of kernel-ABI / codegen / crypto-version bugs, each diagnosed and fixed. See
[`DS207/documents/PROJECT_LOG.md`](DS207/documents/PROJECT_LOG.md) for the full trial log.

## Compatibility at a glance

| | Detail |
|---|---|
| **Target device** | Synology DS207 (PowerPC `mpc824x`, 266 MHz, 64 MB RAM) |
| **DSM version** | DSM 3.1 (Linux kernel 2.6.24, 2008) |
| **File-sharing protocols** | SMB2 and SMB3 (replaces stock SMB1 / CIFS) |
| **Apple support** | `vfs_fruit`, Finder, and Time Machine over SMB |
| **Clients that can connect** | Windows 11, Windows 10, macOS (current), Linux |
| **Secure shell** | Dropbear SSH — ed25519 / curve25519 / chacha20, key-only |
| **TLS client** | curl 8.5 + mbedTLS 3.6 (TLS 1.2) |
| **Install method** | Reversible overlay on the data volume — flash untouched |
| **Network scope** | **LAN only** — never expose to the public internet |
| **License / cost** | MIT, free and open source |

> Although this project was built and verified on the **DS207**, the same overlay approach
> applies to other end-of-life Synology models stuck on old DSM and SMB1 (e.g. DS209,
> DS210, and similar PowerPC / Marvell ARM units). Only the DS207 is tested today.

## Why not a custom `.pat` firmware?

Because the **kernel can't be modernized** (it lives in flash; updating it risks bricking
and there's no source), a full firmware rebuild is over-engineering. Instead everything is
installed onto the **data disk** as an overlay — proven, safe, fast, and reversible.
Full reasoning and comparison tables:
[`DS207/documents/MODERNIZATION_STRATEGY.md`](DS207/documents/MODERNIZATION_STRATEGY.md).

## Important constraint — LAN only

The kernel is Linux 2.6.24 (2008) and cannot be updated. **Do not expose this NAS to the
public internet** (no port-forwarding). On a trusted home/office LAN it is fine.

## What's in this repo

| Path | What |
|---|---|
| `INSTALL.md` | End-user install guide for the overlay installer |
| `DS207/workspace/build/installer/ds207-modern/` | **The installer** — `install.sh`, `modernsmb.sh`, boot hook, `smb.conf.global` |
| `DS207/documents/` | Project log, forensic audit, modernization strategy & decisions |
| `DS207/workspace/build/scripts/` | Cross-compilation build scripts + the kernel-ABI compatibility shim (`syscompat.c`) |
| `DS207/workspace/build/scripts/probes/` | Tiny throwaway C programs from the debugging (for the record) |
| `DS207/workspace/build/configs/` | Tuned `smb.conf` and Avahi service configs |
| `CLAUDE.md` | Engineering guardrails for the project |

**Not** included (and never will be): Synology's proprietary firmware (`.pat` files, kept
locally under `original_pat_files/`), extracted root filesystems, cross-toolchains, and the
compiled `smbd`/`nmbd` binaries (those ship via **GitHub Releases**). All large/proprietary
paths are git-ignored — see `.gitignore`.

## How it was built (summary)

- Cross-toolchain: **zig `cc -target powerpc-linux-musl`** (static, musl), chosen because
  an emulated x86 GCC was unusable on Apple Silicon.
- A force-linked **`syscompat.c`** provides Linux-2.6.24-safe fallbacks for syscalls and
  time functions that modern musl assumes (signalfd4/epoll/eventfd/pipe2/accept4/dup3/inotify,
  and 32-bit `clock_gettime`/`gettimeofday`/`time` — the last fixed broken authentication).
- Two small Samba source patches (a PowerPC VLA codegen workaround, and tolerating
  `SO_REUSEPORT` on the old kernel).
- `smb.conf` tuned for the box: `use sendfile = no`, `aio read/write size = 0`, printing
  disabled, `vfs objects = catia fruit streams_xattr`.

## Frequently asked questions (FAQ)

**Can I keep using an end-of-life Synology NAS that no longer gets updates?**
Yes. The hardware still works; only the software went stale. This project replaces the
obsolete, exposed network services (Samba, SSH, TLS) with current ones as a reversible
overlay, so the box can serve files securely to modern clients on a LAN again.

**Why can't Windows 11 or macOS connect to my old Synology NAS anymore?**
Old DSM ships Samba that only speaks **SMB1 (CIFS)**, which modern Windows and macOS disable
by default for security. Installing modern Samba with **SMB2/SMB3** lets current clients
connect again without you having to re-enable the insecure SMB1 protocol on your computer.

**Does this modify or reflash the Synology firmware? Can it brick my NAS?**
No. Nothing in flash (kernel or bootloader) is touched, so the device **cannot be bricked**.
We do not rebuild, repackage, or re-sign the `.pat` firmware file. Everything installs onto
the data volume and runs *instead of* the stock services, which remain untouched as a
fallback.

**Is it reversible? What if it breaks?**
Fully reversible. A boot watchdog **auto-reverts to stock Samba** if the modern service
fails to start, and you can switch back or uninstall at any time. See [`INSTALL.md`](INSTALL.md).

**Is it safe to expose this NAS to the internet?**
No. The kernel is Linux 2.6.24 (2008) and cannot be updated, so kernel-level flaws remain.
Use it **on a trusted LAN only — never port-forward it to the public internet.**

**Is it free? What license?**
Yes, completely free and open source under the **MIT License**. See the sections below.

**Which Synology models does it support?**
Built and verified on the **DS207**. The overlay method should extend to other end-of-life
PowerPC / Marvell ARM Synology models on old DSM, but only the DS207 is tested today.

## License

This project is open source under the **MIT License** — see [`LICENSE`](LICENSE). The MIT
license is deliberately permissive: you may use, copy, modify, distribute, and build on this
work freely, for personal or commercial purposes, at no cost.

The license covers **only our own scripts, configs, patches, and docs**. Synology's
proprietary firmware (`.pat` files) and trademarks belong to Synology Inc. and are **not**
distributed here.

## No warranty, no liability — use at your own risk

**Read this before you use anything in this repository.**

This software is provided **free, as-is, and purely for educational use and voluntary
personal use by anyone who chooses to run it**. It is offered under the MIT License, and
being open source and free to use does **not** mean the authors take any ownership,
guarantee, or responsibility for what happens when you use it.

- We make **no warranty of any kind** — not of fitness, safety, correctness, or that it will
  work on your hardware at all.
- We accept **no liability and no responsibility whatsoever** for any issue, bug, data loss,
  downtime, hardware failure, security incident, or any other damage or loss — direct or
  indirect — arising in any way from the use of this software or these instructions.
- Nobody is **forcing this on anyone.** You use it entirely by your own choice, on your own
  terms, at your own will, and **at your own risk.** If anything goes wrong as a result of
  using it, that is solely your responsibility.

You modify firmware-adjacent systems and run unofficial software on hardware the manufacturer
no longer supports entirely at your own discretion. If you do not accept these terms, do not
use this project.
