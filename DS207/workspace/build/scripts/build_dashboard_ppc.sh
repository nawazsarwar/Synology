#!/bin/bash
# Cross-build DS207 dashboard (collector + uiserver) for powerpc-linux-musl.
# Runs INSIDE ds207-zig container: /w = workspace, /out = volume.
set -e
export PATH=/opt/zig:$PATH
T=powerpc-linux-musl
S=/w/build/dashboard/src
O=/out/dashboard-ppc
mkdir -p $O /w/build/dashboard/ppc
zig cc -target $T -O2 -c /w/build/scripts/syscompat.c -o /opt/syscompat.o
zig cc -target $T -O2 -c /w/build/scripts/compat64.c  -o /opt/compat64.o
CF="-target $T -mcpu=603e -O2 -std=gnu99 -Wall -Wextra -I $S"
LF="-static -no-pie -Wl,--strip-all /opt/syscompat.o /opt/compat64.o"
PARSERS="$S/parse_*.c $S/json.c"
# (zig objcopy cannot strip big-endian ELF; strip at link time instead)
# collector: everything except the UI server
zig cc $CF $S/collector.c $PARSERS -o $O/collector $LF
# uiserver: UI sources only (collector.c's main excluded)
zig cc $CF -DUISERVER_MAIN -DCOLLECTOR_NO_MAIN $S/uiserver.c $S/http.c $S/auth.c $S/json.c -o $O/uiserver $LF
file $O/collector $O/uiserver 2>/dev/null || true
ls -l $O
echo "=== qemu smoke (kernel 2.6.24) ==="
qemu-ppc-static -r 2.6.24 $O/uiserver --help; echo "uiserver --help rc=$?"
rm -rf /tmp/st; mkdir -p /tmp/st
qemu-ppc-static -r 2.6.24 $O/collector --once /tmp/st; echo "collector --once rc=$?"
ls -l /tmp/st; head -c 300 /tmp/st/*.json | head -20
cp -f $O/collector $O/uiserver /w/build/dashboard/ppc/
echo "DASHBOARD BUILD OK"
