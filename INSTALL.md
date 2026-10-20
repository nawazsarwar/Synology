# Install Guide — DS207 Modern SMB

This installs **modern Samba (SMB2/SMB3, with Apple Time Machine / Finder support)** on a
Synology **DS207** running DSM 3.1, so current Windows, macOS, and Linux clients can mount
shares again.

> **Safety first.** Nothing in flash (kernel/bootloader) is touched — the box **cannot be
> bricked**. The stock Samba is never modified; it is only stopped while the modern one runs.
> If modern Samba ever fails to start at boot, a watchdog **automatically reverts to stock**.

> **LAN only.** The DS207 kernel is Linux 2.6.24 (2008) and cannot be updated. **Do not
> expose this NAS to the public internet** (no port-forwarding). On a trusted home/office
> LAN it is fine.

---

## Before you start

- A DS207 with DSM 3.1 installed, a volume created, and at least one shared folder.
- An **admin** account (you'll log into SMB with a real user — there is **no guest access**).
- About 40 MB of free space on the volume.

## Step 1 — Download the bundle

Download `ds207-modern-<version>.tar.gz` from the project's **GitHub Releases** page onto a
normal computer (not the NAS — the NAS's 2014 TLS can't reach GitHub).

## Step 2 — Copy it to the NAS

Put the file into one of your shared folders. Easiest options:
- **File Station** (DSM web UI) → upload the `.tar.gz` into a share, **or**
- drag it onto a mounted network share.

## Step 3 — Enable SSH

DSM → **Control Panel → Terminal** (or *Network Services*) → check **Enable SSH service** → Apply.

## Step 4 — Run the installer

From your computer's terminal, SSH into the NAS as an admin user and run:

```sh
ssh admin@<NAS-IP>
# become root:
sudo -i            # (enter the admin password)

# go to where you copied the file and unpack it:
cd /volume1/<your-share>
tar xzf ds207-modern-*.tar.gz
cd ds207-modern

# install:
sh install.sh
```

Choose **`1) Install / upgrade Modern SMB`**. The installer will:
- install everything under `/volume1/@modernsmb/` (on the data volume, so it survives a
  DSM reinstall),
- **generate shares automatically** from your existing DSM shared folders (keeping each
  folder's access rules — **authenticated users only, no guest**),
- install a watchdog-protected boot hook,
- stop stock SMB1 and start modern SMB2/SMB3 immediately.

You'll see `modern Samba up (listening on 445)` and a status summary.

## Step 5 — Connect from a client

- **macOS (Finder):** `Cmd-K` → `smb://<NAS-IP>/<ShareName>` → log in as your DSM user.
- **Windows:** `\\<NAS-IP>\<ShareName>` → log in as your DSM user.
- **Linux:** `smb://<NAS-IP>/<ShareName>` in Files, or
  `mount -t cifs //<NAS-IP>/<ShareName> /mnt -o user=<you>,vers=3.0`

> Log in with a **real DSM user** (e.g. your admin account). Guest / anonymous access is
> intentionally disabled.

---

## Optional modules (same installer)

From the menu (`sh install.sh`) you can also add:

**2) Modern SSH (Dropbear)** — runs on **port 2222**, **key-based login only** (password
logins are disabled), with modern crypto (ed25519 / curve25519 / chacha20). DSM's own SSH
on port 22 is left untouched, so you can never be locked out. During install you can paste a
public key; otherwise add keys to `/root/.ssh/authorized_keys`. Connect with:
```sh
ssh -p 2222 root@<NAS-IP>
```
> This is a **shell/command** channel only — it does not provide `scp`/`sftp` into the box.
> Use your SMB shares for file transfer. (To push a single file over SSH:
> `cat localfile | ssh -p 2222 root@<NAS-IP> 'cat > /path/on/nas'`.)

**3) Modern curl + TLS** — a self-contained `curl` (TLS 1.2) at
`/volume1/@modernsmb/bin/curl`, so the NAS can fetch from modern HTTPS sites (GitHub, etc.)
that its ancient system curl/wget cannot. It does not touch the system curl. Example:
```sh
/volume1/@modernsmb/bin/curl --cacert /volume1/@modernsmb/ssl/cacert.pem https://example.com
```

## Managing it later

Run these on the NAS (as root):

```sh
/volume1/@modernsmb/bin/modernsmb.sh status    # what's running + listening
/volume1/@modernsmb/bin/modernsmb.sh revert     # switch back to stock Samba now
/volume1/@modernsmb/bin/modernsmb.sh start      # switch to modern again
/volume1/@modernsmb/bin/modernsmb.sh disable    # keep stock at next boot (kill-switch)
/volume1/@modernsmb/bin/modernsmb.sh enable     # re-enable modern at boot
```

Or re-run `sh install.sh` for the menu (status / revert / uninstall).

## Uninstall

```sh
cd /volume1/<your-share>/ds207-modern
sh install.sh uninstall
```

This removes `/volume1/@modernsmb` and the boot hook and restores stock Samba.

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| Client says "connection refused/abort" right after copy | You're still on stock SMB1. Re-run the installer and pick option 1. |
| `NT_STATUS_ACCESS_DENIED` | Log in as a **real DSM user**; guest is disabled. Check the user is in the share's access list in DSM. |
| After a reboot it's on stock | The watchdog reverted because modern didn't come up. Check `/volume1/@modernsmb/var/modernsmb.log`, then run `modernsmb.sh enable` + `start`. |
| Want to force stock without uninstalling | `modernsmb.sh disable` then `modernsmb.sh revert`. |
| Locked out after a bad reboot | The watchdog should have restored stock automatically. Worst case: remove the drive, mount elsewhere, delete `/volume1/@modernsmb/` and `/volume1/startup/S99modernsmb`. |

Logs: `/volume1/@modernsmb/var/modernsmb.log`.
