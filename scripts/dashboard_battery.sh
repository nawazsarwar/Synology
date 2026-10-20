#!/bin/bash
# Aggressive + snapshot battery for the DS207 dashboard (run from a LAN host).
# usage: NAS=192.168.1.11 DASH_PW=... scripts/dashboard_battery.sh
NAS=${NAS:-192.168.1.11}; PW=${DASH_PW:-password}; H=http://$NAS:8181
CJ=$(mktemp); trap 'rm -f $CJ' EXIT
code() { curl -s -m 8 -o /dev/null -w '%{http_code}' "$@"; }
echo "== unauth"; for p in /api/system /api/wan; do echo "$p -> $(code $H$p)"; done
echo "== traversal"
for u in '/..%2f..%2fetc/passwd' '/../../etc/shadow' '/%2e%2e/%2e%2e/etc/passwd' '/assets/../../../etc/passwd'; do
  echo "$u -> $(curl -s --path-as-is -m 8 -o /tmp/trav.out -w '%{http_code}' "$H$u") body_has_root=$(grep -c '^root:' /tmp/trav.out)"; done
curl -s -m5 -c $CJ -H 'Content-Type: application/json' -d "{\"password\":\"$PW\"}" $H/api/login >/dev/null
echo "== flood"
for i in 1 2 3 4 5 6; do head -c 4096 /dev/urandom | nc -w 2 $NAS 8181 >/dev/null 2>&1; done
printf 'GET %s HTTP/1.1\r\n\r\n' "$(head -c 20000 /dev/zero | tr '\0' A)" | nc -w 2 $NAS 8181 >/dev/null 2>&1
echo "/ -> $(code $H/)  authed /api/system -> $(code -b $CJ $H/api/system)"
echo "== 4 concurrent"
for i in 1 2 3 4; do ( for j in 1 2 3 4 5; do code -b $CJ $H/api/system; echo -n ' '; done; echo ) & done; wait
echo "== snapshot integrity (60 fetches)"
bad=0; for i in $(seq 60); do b=$(curl -s -m5 -b $CJ $H/api/system); case "$b" in '{'*'}') ;; *) bad=$((bad+1)); echo "BAD: $b";; esac; sleep 0.2; done
echo "bad=$bad"
echo "== bogus endpoint: $(code -b $CJ $H/api/doesnotexist)"
