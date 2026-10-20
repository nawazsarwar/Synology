#!/bin/sh
# HTTP-level regression gate for the DS207 dashboard (run from a LAN host).
# Covers: auth, path traversal, junk flood, concurrent clients, snapshot integrity.
# NOT covered here (done by hand, see PROJECT_LOG Trial 5): privilege/ps, resource/SMART
# cadence, kill-9-no-Samba-revert, uninstall/reinstall.
# usage: DASH_PW=... [NAS=192.168.1.11] [N=4] sh scripts/dashboard_battery.sh   (exit 0 = all PASS)
: "${DASH_PW:?set DASH_PW to the dashboard password}"
NAS=${NAS:-192.168.1.11}; N=${N:-4}; H="http://$NAS:8181"
CJ=$(mktemp); TRAV=$(mktemp); CONC=$(mktemp)
trap 'rm -f "$CJ" "$TRAV" "$CONC"' EXIT
fail=0
check() { desc=$1; shift; if "$@"; then echo "PASS: $desc"; else echo "FAIL: $desc"; fail=1; fi; }
code() { curl -s -m 8 -o /dev/null -w '%{http_code}' "$@"; }
eq() { [ "$1" = "$2" ]; }

check "unauth /api/system == 401" eq "$(code "$H/api/system")" 401
check "unauth /api/wan == 401" eq "$(code "$H/api/wan")" 401

for u in '/..%2f..%2fetc/passwd' '/../../etc/shadow' '/%2e%2e/%2e%2e/etc/passwd' '/assets/../../../etc/passwd'; do
  st=$(curl -s --path-as-is -m 8 -o "$TRAV" -w '%{http_code}' "$H$u")
  [ "$st" != 200 ] && ! grep -q 'root:' "$TRAV"; check "traversal rejected ($u -> $st)" [ $? -eq 0 ]
done

# Login body is JSON built by interpolation: a password containing a double quote or backslash
# would break it (set one without those characters for this script).
curl -s -m 5 -c "$CJ" -H 'Content-Type: application/json' -d "{\"password\":\"$DASH_PW\"}" "$H/api/login" >/dev/null
check "login yields authed /api/system == 200" eq "$(code -b "$CJ" "$H/api/system")" 200

for i in 1 2 3 4 5 6; do head -c 4096 /dev/urandom | nc -w 2 "$NAS" 8181 >/dev/null 2>&1; done
printf 'GET %s HTTP/1.1\r\n\r\n' "$(head -c 20000 /dev/zero | tr '\0' A)" | nc -w 2 "$NAS" 8181 >/dev/null 2>&1
check "/ == 200 after junk flood" eq "$(code "$H/")" 200
check "authed /api/system == 200 after flood" eq "$(code -b "$CJ" "$H/api/system")" 200

: > "$CONC"; i=0
while [ $i -lt "$N" ]; do ( j=0; while [ $j -lt 5 ]; do printf "%s\n" "$(code -b "$CJ" "$H/api/system")" >> "$CONC"; j=$((j+1)); done ) & i=$((i+1)); done
wait
total=$(grep -c . "$CONC"); ok=$(grep -c '^200$' "$CONC")
check "all $((N*5)) concurrent authed requests 200 (got $ok/$total)" [ "$total" -eq $((N*5)) ] && [ "$ok" -eq "$total" ]

bad=0; i=0
while [ $i -lt 60 ]; do
  b=$(curl -s -m 5 -b "$CJ" "$H/api/system")
  case "$b" in '{'*'}') ;; *) bad=$((bad+1));; esac
  sleep 0.2; i=$((i+1))
done
check "snapshot integrity: 60 fetches, bad=$bad" eq "$bad" 0

[ $fail -eq 0 ] && echo "ALL PASS" || echo "FAILURES"
exit $fail
