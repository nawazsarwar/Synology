# Synology DS207 Modernization

Bringing modern, secure file sharing to a **2007 Synology DS207** (PowerPC `mpc824x`,
266 MHz, 64 MB RAM, Linux 2.6.24, DSM 3.1) — so current Windows 11, macOS, and Linux
clients can mount shares over **SMB2/SMB3** (with Apple `vfs_fruit` / Time Machine),
without touching the device's flash (no brick risk).

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

## License

Our own scripts, configs, and docs only. Synology firmware and trademarks belong to
Synology Inc. and are not distributed here.
