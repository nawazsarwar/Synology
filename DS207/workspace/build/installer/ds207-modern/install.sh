#!/bin/sh
# install.sh - DS207 modernization installer (overlay, no flash touch).
# Run as root ON the DS207, from the unpacked bundle directory:
#     sh install.sh
#
# What it does (SMB module):
#   - installs modern Samba (SMB2/SMB3 + Apple vfs_fruit) under /volume1/@modernsmb
#   - generates smb.conf from the live DSM shares (so your Web-UI shares just work)
#   - installs a watchdog-protected boot hook (/volume1/startup/S99modernsmb)
#   - stops the stock SMB1 Samba and starts the modern one now
#
# Safety:
#   - the stock Samba binaries are NEVER modified; revert = stop ours, start stock
#   - the boot hook auto-reverts to stock if modern Samba fails to come up
#   - nothing in flash (kernel/bootloader) is touched: the box cannot be bricked

set -u
HERE=$(cd "$(dirname "$0")" && pwd)
PAYLOAD="$HERE/payload"
ROOT=/volume1/@modernsmb
STARTUP=/volume1/startup
STOCK_CONF=/usr/syno/etc/smb.conf
CTRL="$ROOT/bin/modernsmb.sh"

say() { echo "$*"; }
die() { echo "ERROR: $*" >&2; exit 1; }

check_env() {
	[ "$(id -u)" = "0" ] || die "must run as root (you are $(id -un))."
	uname -m | grep -qi ppc || say "WARNING: arch is not ppc ($(uname -m)); continuing anyway."
	[ -d /volume1 ] || die "/volume1 not found - create a volume in DSM first."
	[ -f "$PAYLOAD/sbin/smbd" ] || die "payload not found next to install.sh (expected $PAYLOAD/sbin/smbd)."
}

# Generate modern share stanzas from the live DSM smb.conf.
# Keeps: path, guest ok, valid users, invalid users, writeable/read only.
gen_shares() {
	[ -f "$STOCK_CONF" ] || { echo "# (no stock smb.conf found; no shares generated)"; return; }
	awk '
	function emit() {
		if (name != "" && name != "global" && path != "") {
			printf "\n[%s]\n", name
			printf "   comment = %s (modern SMB)\n", name
			printf "   path = %s\n", path
			printf "   read only = %s\n", (ro=="yes"?"yes":"no")
			if (guest != "") printf "   guest ok = %s\n", guest
			if (valid != "") printf "   valid users = %s\n", valid
			if (invalid != "") printf "   invalid users = %s\n", invalid
		}
	}
	/^[[:space:]]*\[/ {
		emit()
		line=$0; sub(/^[[:space:]]*\[/,"",line); sub(/\].*$/,"",line)
		name=line; path=""; guest=""; valid=""; invalid=""; ro=""
		next
	}
	{
		k=$0; v=$0
		sub(/=.*/,"",k); gsub(/[[:space:]]/,"",k)
		sub(/^[^=]*=/,"",v); gsub(/^[[:space:]]+|[[:space:]]+$/,"",v)
		if (k=="path") path=v
		else if (k=="guestok") guest=v
		else if (k=="validusers") valid=v
		else if (k=="invalidusers") invalid=v
		else if (k=="writeable"||k=="writable") { if(v=="yes") ro="no"; else ro="yes" }
		else if (k=="readonly") ro=v
	}
	END { emit() }
	' "$STOCK_CONF"
}

build_conf() {
	mkdir -p "$ROOT/etc"
	cp "$PAYLOAD/smb.conf.global" "$ROOT/etc/smb.conf"
	gen_shares >> "$ROOT/etc/smb.conf"
	# always provide home dirs
	cat >> "$ROOT/etc/smb.conf" <<'EOF'

[homes]
   comment = Home Directories
   browseable = no
   read only = no
   create mask = 0700
   directory mask = 0700
   valid users = %S
EOF
}

install_smb() {
	say "Installing modern Samba to $ROOT ..."
	mkdir -p "$ROOT/sbin" "$ROOT/bin" "$ROOT/etc" "$ROOT/var/lock" "$ROOT/var/run" "$ROOT/private" "$STARTUP"
	cp "$PAYLOAD/sbin/smbd" "$ROOT/sbin/smbd"
	cp "$PAYLOAD/sbin/nmbd" "$ROOT/sbin/nmbd"
	cp "$PAYLOAD/modernsmb.sh" "$ROOT/bin/modernsmb.sh"
	chmod +x "$ROOT/sbin/smbd" "$ROOT/sbin/nmbd" "$ROOT/bin/modernsmb.sh"
	build_conf
	say "Generated config:"
	grep -E '^\[' "$ROOT/etc/smb.conf" | sed 's/^/   /'
	# validate config if testparm-equivalent exists (smbd -b won't validate; try a dry load)
	# install boot hook
	cp "$PAYLOAD/S99modernsmb" "$STARTUP/S99modernsmb"
	chmod +x "$STARTUP/S99modernsmb"
	rm -f "$ROOT/.disabled"
	say "Boot hook installed: $STARTUP/S99modernsmb"
	say ""
	say "Switching to modern Samba now ..."
	"$CTRL" start
	say ""
	"$CTRL" status
	say ""
	say "Done. Modern SMB2/SMB3 is live and will auto-start on boot."
	say "If modern Samba ever fails to start at boot, it auto-reverts to stock."
}

revert_stock() {
	[ -x "$CTRL" ] || die "not installed."
	"$CTRL" revert
	"$CTRL" disable
	say "Reverted to stock Samba for now. Boot will stay on stock until you run 'enable'."
}

uninstall() {
	say "Uninstalling modern Samba completely ..."
	[ -x "$CTRL" ] && "$CTRL" revert
	rm -f "$STARTUP/S99modernsmb"
	rm -rf "$ROOT"
	say "Removed $ROOT and the boot hook. Stock Samba restored."
}

status() { [ -x "$CTRL" ] && "$CTRL" status || say "not installed."; }

menu() {
	while true; do
		echo ""
		echo "==== DS207 Modernization Installer ===="
		echo "  1) Install / upgrade Modern SMB (SMB2/SMB3 + Apple vfs_fruit)"
		echo "  2) Status"
		echo "  3) Revert to stock Samba (keep installed, disable at boot)"
		echo "  4) Uninstall completely (remove everything, restore stock)"
		echo "  0) Exit"
		echo -n "choose: "
		read ans
		case "$ans" in
			1) install_smb ;;
			2) status ;;
			3) revert_stock ;;
			4) uninstall ;;
			0|q|Q) exit 0 ;;
			*) echo "invalid choice" ;;
		esac
	done
}

check_env
# non-interactive sub-commands: install.sh {install|status|revert|uninstall}
case "${1:-menu}" in
	install) install_smb ;;
	status)  status ;;
	revert)  revert_stock ;;
	uninstall) uninstall ;;
	menu)    menu ;;
	*) echo "usage: $0 {install|status|revert|uninstall}  (or no arg for menu)"; exit 1 ;;
esac
