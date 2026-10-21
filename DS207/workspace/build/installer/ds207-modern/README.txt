DS207 Modernization Bundle
==========================

Brings modern SMB2/SMB3 file sharing (with Apple Time Machine / Finder support)
to a Synology DS207 running DSM 3.1. Modern Windows, macOS, and Linux clients
can mount shares again.

SAFETY
  - Nothing in flash (kernel/bootloader) is touched: the box CANNOT be bricked.
  - The stock Samba is never modified; it is just stopped while the modern one runs.
  - If modern Samba fails to start at boot, it auto-reverts to stock.
  - Everything is installed on the data volume (/volume1/@modernsmb), so it even
    survives a DSM reinstall.

IMPORTANT
  - Keep this NAS on your LAN only. Its Linux kernel (2.6.24, 2008) cannot be
    updated and must NOT be exposed to the public internet (no port-forwarding).

MODULES (menu options)
  1) Modern SMB   - SMB2/SMB3 + Apple Time Machine/Finder (vfs_fruit)
  2) Modern SSH   - Dropbear (ed25519/curve25519/chacha20) on port 2222, KEY-BASED login.
                    DSM's own SSH on port 22 is left untouched, so you cannot be locked out.
  3) Modern curl  - self-contained curl 8.5 + mbedTLS (TLS 1.2); does not touch system curl.
                    Lets the NAS fetch from modern HTTPS sites (GitHub etc).
  4) Dashboard    - read-only web monitoring UI on port 8181 (install with: sh install.sh ui).
                    Shows live CPU/RAM/load, network + disk I/O, storage/RAID, SMART, sessions
                    and service versions. Runs ALONGSIDE the stock DSM UI (port 5000), which is
                    never touched. Open http://<nas-ip>:8181 and sign in (set a password at
                    install). LAN-only, plain HTTP; a UI crash never affects Samba.

HOW TO INSTALL
  1. Copy this whole folder onto the NAS (e.g. into a shared folder via
     File Station or by dragging it onto a mounted share).
  2. Enable SSH on the NAS: Control Panel -> Terminal -> Enable SSH service.
  3. SSH in as an admin user, become root, and run the installer:
         cd /volume1/<your-share>/ds207-modern
         sudo sh install.sh          # or: sh install.sh  (if already root)
  4. Choose "1) Install / upgrade Modern SMB".

That's it. Your existing DSM shared folders are served automatically over SMB2/SMB3.

MANAGE IT LATER
     /volume1/@modernsmb/bin/modernsmb.sh status     # what's running
     /volume1/@modernsmb/bin/modernsmb.sh revert     # back to stock now
     /volume1/@modernsmb/bin/modernsmb.sh start      # modern again
  Or re-run install.sh for the menu (status / revert / uninstall).

UNINSTALL
     sh install.sh uninstall

Kernel is frozen; this is a LAN file server, not an internet-facing box.
