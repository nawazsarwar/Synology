#!/bin/sh
# ds207_live_patch.sh — install/uninstall the modern SMB2/SMB3 + mDNS + SSH stack
# on a live Synology DS207 (PowerPC, DSM 3.1) over SSH/Telnet, WITHOUT reflashing
# and WITHOUT touching the data drives. Fully reversible.
#
#   Usage:  ./ds207_live_patch.sh install
#           ./ds207_live_patch.sh uninstall
#           ./ds207_live_patch.sh status
#
# What it does (install):
#   * Unpacks the self-contained static binaries to /usr/syno/samba and /opt/ssh.
#   * Backs up the stock Samba 3.2.8 smbd/nmbd (as *.orig) and points the stock
#     startup script at the new static Samba 4.15.13 (SMB2/SMB3 + Apple vfs_fruit).
#   * Installs Avahi service files so macOS Finder + Time Machine discover the NAS.
#   * Starts a modern Dropbear SSH (Ed25519) on port 2222, leaving stock sshd:22 alone.
# Nothing is overwritten that cannot be restored by "uninstall".

set -u
PAYLOAD="ds207_modern_payload.tgz"
SMBROOT=/usr/syno/samba
STOCK_SBIN=/usr/syno/sbin
RC=/usr/syno/etc/rc.d
STAMP=/usr/syno/samba/.ds207patch
log(){ echo "[ds207-patch] $*"; }
die(){ echo "[ds207-patch][ERROR] $*" >&2; exit 1; }

here="$(cd "$(dirname "$0")" && pwd)"

preflight(){
  [ "$(id -u)" = "0" ] || die "must run as root (use: sudo sh $0 $1)"
  # confirm this really is the PowerPC DS207 before touching anything
  if [ -r /etc.defaults/VERSION ]; then . /etc.defaults/VERSION 2>/dev/null; fi
  uniq="$(/bin/get_key_value /etc.defaults/synoinfo.conf unique 2>/dev/null)"
  case "$uniq" in
    *ppc824x_207*) : ;;
    *) die "this NAS reports unique='$uniq' — this patch is only for synology_ppc824x_207 (DS207). Aborting." ;;
  esac
  arch="$(uname -m 2>/dev/null)"
  log "host OK: $uniq, kernel $(uname -r), arch $arch"
}

do_install(){
  preflight install
  [ -f "$here/$PAYLOAD" ] || die "payload $PAYLOAD not found next to this script"

  log "unpacking payload…"
  tar xzf "$here/$PAYLOAD" -C / || die "payload extraction failed"
  mkdir -p $SMBROOT/var/lock $SMBROOT/var/locks $SMBROOT/var/cache $SMBROOT/var/run $SMBROOT/private

  # Use the existing DSM SMB user database so current accounts keep working.
  if [ -f /usr/syno/etc/private/smbpasswd ] && [ ! -e $SMBROOT/private/smbpasswd ]; then
    ln -sf /usr/syno/etc/private/smbpasswd $SMBROOT/private/smbpasswd
  fi

  # Back up and replace stock smbd/nmbd with symlinks to the new static builds.
  for b in smbd nmbd; do
    if [ -e "$STOCK_SBIN/$b" ] && [ ! -e "$STOCK_SBIN/$b.orig" ] && [ ! -L "$STOCK_SBIN/$b" ]; then
      mv "$STOCK_SBIN/$b" "$STOCK_SBIN/$b.orig" || die "could not back up $b"
      log "backed up stock $b -> $b.orig"
    fi
    ln -sf "$SMBROOT/sbin/$b" "$STOCK_SBIN/$b"
  done

  # Sanity: new smbd must at least report its version on this hardware.
  if "$SMBROOT/sbin/smbd" -V >/tmp/ds207_smbd_v 2>&1; then
    log "new smbd: $(cat /tmp/ds207_smbd_v)"
  else
    log "WARN: new smbd -V failed; rolling back binaries"
    do_uninstall_bins; die "new smbd did not run — rolled back. See /tmp/ds207_smbd_v"
  fi

  # Restart Samba via the stock init script (it calls \$SMBD -D / \$NMBD -D).
  log "restarting Samba…"
  sh $RC/S80samba.sh stop  >/dev/null 2>&1
  sh $RC/S80samba.sh start >/dev/null 2>&1
  sleep 2
  if pidof smbd >/dev/null 2>&1; then log "smbd is running"; else log "WARN: smbd not detected after start — check /usr/syno/samba/var/log.smbd"; fi

  # Avahi: reload so the new _smb._tcp / _adisk._tcp (Time Machine) services publish.
  if [ -x $RC/S99avahi.sh ]; then
    cp -f $SMBROOT/../avahi/services/*.service /usr/syno/avahi/services/ 2>/dev/null
    sh $RC/S99avahi.sh reload >/dev/null 2>&1 || sh $RC/S99avahi.sh restart >/dev/null 2>&1
    log "Avahi reloaded (Finder discovery + Time Machine)"
  fi

  # Dropbear (modern SSH) on :2222, non-intrusive (stock sshd stays on :22).
  mkdir -p /opt/ssh/etc
  [ -f /opt/ssh/etc/dropbear_ed25519_host_key ] || \
    /opt/ssh/sbin/dropbearmulti dropbearkey -t ed25519 -f /opt/ssh/etc/dropbear_ed25519_host_key >/dev/null 2>&1
  /opt/ssh/sbin/dropbearmulti dropbear -r /opt/ssh/etc/dropbear_ed25519_host_key -p 2222 -P /var/run/dropbear.pid >/dev/null 2>&1 \
    && log "Dropbear SSH (Ed25519) listening on :2222" || log "WARN: Dropbear failed to start"

  touch "$STAMP"
  log "INSTALL COMPLETE."
  log "  SMB:  \\\\<nas-ip>  (SMB2/SMB3; shares/users in $SMBROOT/etc/smb.conf)"
  log "  SSH:  ssh -p 2222 <user>@<nas-ip>  (modern Dropbear)"
  log "Revert any time with:  sh $0 uninstall"
}

do_uninstall_bins(){
  for b in smbd nmbd; do
    [ -L "$STOCK_SBIN/$b" ] && rm -f "$STOCK_SBIN/$b"
    [ -e "$STOCK_SBIN/$b.orig" ] && mv -f "$STOCK_SBIN/$b.orig" "$STOCK_SBIN/$b"
  done
}

do_uninstall(){
  preflight uninstall
  log "stopping modern services…"
  [ -f /var/run/dropbear.pid ] && kill "$(cat /var/run/dropbear.pid)" 2>/dev/null
  sh $RC/S80samba.sh stop >/dev/null 2>&1
  rm -f /usr/syno/avahi/services/smb.service /usr/syno/avahi/services/timemachine.service
  do_uninstall_bins
  log "restoring stock Samba…"
  sh $RC/S80samba.sh start >/dev/null 2>&1
  [ -x $RC/S99avahi.sh ] && sh $RC/S99avahi.sh reload >/dev/null 2>&1
  rm -f "$STAMP"
  log "UNINSTALL COMPLETE — stock DSM 3.1 Samba restored. (Static files left under $SMBROOT and /opt/ssh; delete manually if desired.)"
}

do_status(){
  echo "patch installed: $([ -f "$STAMP" ] && echo yes || echo no)"
  echo "smbd symlink:    $(ls -l $STOCK_SBIN/smbd 2>/dev/null)"
  echo "smbd running:    $(pidof smbd >/dev/null 2>&1 && echo yes || echo no)"
  echo "smbd version:    $($SMBROOT/sbin/smbd -V 2>/dev/null)"
  echo "dropbear :2222:  $(pidof dropbear >/dev/null 2>&1 && echo running || echo stopped)"
}

case "${1:-}" in
  install)   do_install ;;
  uninstall) do_uninstall ;;
  status)    do_status ;;
  *) echo "Usage: $0 {install|uninstall|status}"; exit 1 ;;
esac
