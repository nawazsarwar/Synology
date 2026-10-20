#!/bin/bash
# Static modern Dropbear SSH (ed25519/curve25519/chacha20) for DS207
# (powerpc-linux-musl, zig). Linked against syscompat.o so the musl time64
# functions don't return garbage on kernel 2.6.24 -- otherwise Dropbear thinks
# an hour has passed and triggers a rekey (KEXINIT / "type 20") mid-auth, which
# breaks every login.
set -e
exec > >(tee -a /out/dropbear.log) 2>&1
echo "===== static dropbear build $(date) ====="
export PATH=/opt/zig:$PATH
T=powerpc-linux-musl

# syscompat.o (2.6.24 syscall + time64 fallbacks) -- the key fix
zig cc -target $T -O2 -c /w/build/scripts/syscompat.c -o /opt/syscompat.o

export CC="zig cc -target $T"
export CFLAGS="-O2 -mcpu=603e"
export LDFLAGS="-static -no-pie /opt/syscompat.o"

SRC=/out/dbsrc; mkdir -p $SRC; cd $SRC
VER=2022.83
[ -f dropbear-$VER.tar.bz2 ] || wget -q https://matt.ucc.asn.au/dropbear/releases/dropbear-$VER.tar.bz2
[ -d dropbear-$VER ] || tar xf dropbear-$VER.tar.bz2
cd dropbear-$VER

# force /dev/urandom (2.6.24 has no getrandom); disable utmp/wtmp/lastlog/pam/zlib
cat > localoptions.h <<'EOF'
#define DROPBEAR_SMALL_CODE 1
#define DO_MOTD 0
#define DROPBEAR_SVR_PASSWORD_AUTH 1
#define DROPBEAR_SVR_PUBKEY_AUTH 1
#define DROPBEAR_ED25519 1
#define DROPBEAR_CURVE25519 1
#define DROPBEAR_CHACHA20POLY1305 1
#define DSS_PRIV_FILENAME "/volume1/@modernsmb/ssh/hostkey_dss"
#define RSA_PRIV_FILENAME "/volume1/@modernsmb/ssh/hostkey_rsa"
#define ED25519_PRIV_FILENAME "/volume1/@modernsmb/ssh/hostkey_ed25519"
EOF

./configure --host=powerpc-linux --build=aarch64-linux \
  --disable-zlib --disable-lastlog --disable-utmp --disable-utmpx \
  --disable-wtmp --disable-wtmpx --disable-pututline --disable-pututxline \
  --disable-pam \
  >/tmp/db.cfg 2>&1 || { echo "CONFIGURE FAIL"; tail -40 /tmp/db.cfg; exit 71; }
echo "configure OK"

make -j4 PROGRAMS="dropbear dbclient dropbearkey scp" MULTI=1 SCPPROGRESS=1 \
  >/tmp/db.mk 2>&1 || { echo "MAKE FAIL"; tail -50 /tmp/db.mk; exit 72; }
echo "build OK"

OUT=/out/dropbear-static; rm -rf $OUT; mkdir -p $OUT
cp dropbearmulti $OUT/dropbearmulti
file $OUT/dropbearmulti
echo "=== qemu smoke test ==="
qemu-ppc-static -r 2.6.24 $OUT/dropbearmulti dropbear -V 2>&1 | head -2
echo "===== dropbear DONE $(date) ====="
touch /out/dropbear.OK
