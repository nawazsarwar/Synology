#!/bin/sh
# modernsmb.sh - runtime control for modern Samba (SMB2/SMB3) on DS207.
# Lives at /volume1/@modernsmb/bin/modernsmb.sh. POSIX sh (busybox ash).
#
# Design: we NEVER modify or replace the stock Samba binaries. We stop the
# stock Samba service and run our own smbd/nmbd from /volume1/@modernsmb.
# Reverting is therefore trivial: stop ours, start stock.
#
# The "boot" action is watchdog-protected: if modern smbd does not come up
# listening on port 445 within ~40s, it auto-reverts to stock AND sets a
# kill-switch so the next boot stays on stock (never leaves the box without
# file sharing, never loops a broken start). This is the hard-learned lesson
# from the first deployment attempt.

ROOT=/volume1/@modernsmb
SBIN=$ROOT/sbin
CONF=$ROOT/etc/smb.conf
VAR=$ROOT/var
LOG=$VAR/modernsmb.log
DISABLE=$ROOT/.disabled           # kill-switch: if present, boot stays on stock
STOCK_RC=/usr/syno/etc/rc.d/S80samba.sh

# Modern SSH (Dropbear) runs on a SEPARATE port alongside DSM's stock OpenSSH,
# so a Dropbear problem can never lock you out (stock SSH on 22 stays available).
SSHDIR=$ROOT/ssh
DBPORT=2222
DBKEY=$SSHDIR/hostkey_ed25519
DBPID=$VAR/dropbear.pid
DBBIN=$SSHDIR/dropbearmulti

mkdir -p "$VAR/lock" "$VAR/run" "$ROOT/private" 2>/dev/null

log() { echo "$(date '+%Y-%m-%d %H:%M:%S') $*" >>"$LOG" 2>/dev/null; }

# vfs_fruit / streams_xattr (macOS Finder metadata + Time Machine) need the data
# volume mounted with user_xattr. DSM mounts without it, so we enable it (the ext3
# filesystem already carries the ext_attr feature; this is non-destructive and only
# lasts until reboot, which is why start_modern re-applies it every time).
ensure_xattr() {
	for vol in /volume1 /volume2 /volume3 /volume4; do
		[ -d "$vol" ] || continue
		grep -q " $vol ext" /proc/mounts || continue
		grep -qE " $vol [^ ]+ [^ ]*user_xattr" /proc/mounts && continue
		mount -o remount,user_xattr "$vol" 2>>"$LOG" && log "enabled user_xattr on $vol"
	done
}

listening_445() { netstat -ln 2>/dev/null | grep -qE '[:.]445 '; }

stock_running() { ps w 2>/dev/null | grep -E '/usr/syno/sbin/smbd' | grep -qv grep; }
modern_running() { ps w 2>/dev/null | grep -E "$SBIN/smbd" | grep -qv grep; }

stop_stock() {
	[ -x "$STOCK_RC" ] && "$STOCK_RC" stop >>"$LOG" 2>&1
	# belt-and-suspenders: make sure stock smbd/nmbd are gone
	killall -q smbd nmbd 2>/dev/null
	sleep 2
}

start_stock() {
	log "reverting to stock Samba"
	killall -q smbd nmbd 2>/dev/null
	sleep 1
	[ -x "$STOCK_RC" ] && "$STOCK_RC" start >>"$LOG" 2>&1
}

stop_modern() {
	killall -q "$SBIN/smbd" "$SBIN/nmbd" 2>/dev/null
	# killall matches basename on busybox; also try basenames
	for p in $(ps w 2>/dev/null | grep "$SBIN/" | grep -v grep | awk '{print $1}'); do
		kill "$p" 2>/dev/null
	done
	sleep 2
}

start_modern() {
	ensure_xattr
	log "starting modern smbd/nmbd"
	"$SBIN/smbd" -D -s "$CONF" >>"$LOG" 2>&1
	"$SBIN/nmbd" -D -s "$CONF" >>"$LOG" 2>&1
}

# ---- Modern SSH (Dropbear) : separate port, pubkey auth, never replaces DSM SSH ----
ssh_running() { ps w 2>/dev/null | grep "$DBBIN dropbear" | grep -qv grep; }
ssh_start() {
	[ -x "$DBBIN" ] || return 0                      # SSH module not installed
	[ -f "$DBKEY" ] || "$DBBIN" dropbearkey -t ed25519 -f "$DBKEY" >>"$LOG" 2>&1
	ssh_running && { log "dropbear already running"; return 0; }
	# Foreground mode, backgrounded via full fd-redirect. Dropbear's own daemonize
	# double-fork crashes the connection children on this box; -F avoids that.
	"$DBBIN" dropbear -F -E -r "$DBKEY" -p "$DBPORT" >>"$VAR/dropbear.log" 2>&1 </dev/null &
	sleep 2
	ssh_running && log "dropbear up on port $DBPORT" || log "dropbear failed to start"
}
ssh_stop() { killall dropbearmulti 2>/dev/null; }

case "$1" in
	start)
		stop_stock
		start_modern
		sleep 3
		if listening_445; then echo "modern Samba up (listening on 445)";
		else echo "WARNING: 445 not listening yet; check $LOG"; fi
		;;
	stop)
		stop_modern
		echo "modern Samba stopped (stock NOT started; use 'revert' for that)"
		;;
	revert)
		stop_modern
		start_stock
		sleep 2
		stock_running && echo "stock Samba restored" || echo "WARNING: stock not confirmed; check $LOG"
		;;
	boot)
		# watchdog-protected start, fully backgrounded so it can never block boot
		[ -f "$DISABLE" ] && { log "boot: kill-switch present, staying on stock"; exit 0; }
		(
			sleep 15                       # let network + DSM services settle
			log "boot: switching to modern Samba"
			stop_stock
			start_modern
			ok=0
			i=0
			while [ $i -lt 8 ]; do
				sleep 5
				if listening_445 && modern_running; then ok=1; break; fi
				i=$((i+1))
			done
			if [ "$ok" = 1 ]; then
				log "boot: modern Samba healthy (445 listening)"
			else
				log "boot: WATCHDOG tripped - modern Samba did not come up; reverting + disabling"
				stop_modern
				touch "$DISABLE"
				start_stock
			fi
			# Modern SSH is independent (separate port, DSM SSH untouched) -> just start it.
			ssh_start
		) >>"$LOG" 2>&1 &
		exit 0
		;;
	ssh-start) ssh_start; ssh_running && echo "dropbear up on port $DBPORT (pubkey auth)" || echo "dropbear not running; see $VAR/dropbear.log";;
	ssh-stop)  ssh_stop; echo "dropbear stopped (DSM SSH on 22 unaffected)";;
	status)
		echo "== modern Samba status =="
		if modern_running; then echo "modern smbd: RUNNING"; else echo "modern smbd: not running"; fi
		if stock_running; then echo "stock  smbd: RUNNING"; else echo "stock  smbd: not running"; fi
		if listening_445; then echo "port 445 : LISTENING"; else echo "port 445 : not listening"; fi
		[ -f "$DISABLE" ] && echo "kill-switch: PRESENT (boot stays on stock)"
		echo "version  : $($SBIN/smbd -V 2>/dev/null | head -1)"
		echo "-- modern SSH --"
		if [ -x "$DBBIN" ]; then
			if ssh_running; then echo "dropbear : RUNNING on port $DBPORT (pubkey auth)"; else echo "dropbear : installed, not running"; fi
		else echo "dropbear : not installed"; fi
		echo "-- modern curl --"
		if [ -x "$ROOT/bin/curl" ]; then echo "curl     : $($ROOT/bin/curl --version 2>/dev/null | head -1 | cut -c1-60)"; else echo "curl     : not installed"; fi
		;;
	enable)
		rm -f "$DISABLE" && echo "kill-switch cleared; modern Samba will start at next boot"
		;;
	disable)
		touch "$DISABLE" && echo "kill-switch set; boot will stay on stock Samba"
		;;
	*)
		echo "usage: $0 {start|stop|revert|boot|status|enable|disable|ssh-start|ssh-stop}"
		exit 1
		;;
esac
