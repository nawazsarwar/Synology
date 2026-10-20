#!/bin/bash
# Static modern curl for DS207 with mbedTLS (pure-C, no GMP/asm -> safe on 603e).
# GnuTLS pulled GMP/nettle whose CPU-tuned asm SIGSEGVs on this PowerPC; mbedTLS
# avoids that entirely while still doing TLS 1.2/1.3.
set -e
exec > >(tee -a /out/curlmbed.log) 2>&1
echo "===== curl+mbedtls build $(date) ====="
export PATH=/opt/zig:$PATH
T=powerpc-linux-musl
CCZ="zig cc -target $T"

# ---- syscompat.o + time64 trampolines (same as Samba/curl-gnutls) ----
$CCZ -O2 -c /w/build/scripts/syscompat.c -o /opt/syscompat.o
INC=/opt/zig/lib/libc/musl/include
{ echo '.text'
  grep -rhoE "__REDIR\([a-z0-9_]+, *__[a-z0-9_]+\)" $INC 2>/dev/null \
    | sed -E 's/__REDIR\(([a-z0-9_]+), *(__[a-z0-9_]+)\)/\2 \1/' | sort -u \
    | while read tgt plain; do
        printf '.weak %s\n.type %s,@function\n%s:\n\tb %s\n' "$tgt" "$tgt" "$tgt" "$plain"
      done
} > /opt/time64.s
$CCZ -c /opt/time64.s -o /opt/compat64.o
echo "trampolines: $(grep -c '\.weak' /opt/time64.s)"

# ---- build mbedTLS static ----
MBED=/out/ppcmbed
SRC=/out/mbedsrc; mkdir -p $SRC; cd $SRC
MVER=3.6.2
if [ ! -d mbedtls-$MVER ]; then
  [ -f mbedtls-$MVER.tar.bz2 ] || wget -q https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-$MVER/mbedtls-$MVER.tar.bz2
  tar xf mbedtls-$MVER.tar.bz2
fi
cd mbedtls-$MVER
# TLS 1.2 only: curl 8.5 + mbedTLS 3.6 TLS 1.3 needs PSA crypto init that fails
# ssl_setup on this box. TLS 1.2 (ECDHE + AES-GCM) is still modern and GitHub speaks it.
sed -i 's|^#define MBEDTLS_SSL_PROTO_TLS1_3|//#define MBEDTLS_SSL_PROTO_TLS1_3|' include/mbedtls/mbedtls_config.h
grep -q '//#define MBEDTLS_SSL_PROTO_TLS1_3' include/mbedtls/mbedtls_config.h && echo "TLS1.3 disabled"
make clean >/dev/null 2>&1 || true
make -j4 lib CC="$CCZ" AR="zig ar" CFLAGS="-O2 -mcpu=603e -I$PWD/include -DMBEDTLS_HAVE_TIME_DATE" \
  >/tmp/mbed.mk 2>&1 || { echo "MBED MAKE FAIL"; tail -40 /tmp/mbed.mk; exit 41; }
rm -rf $MBED; mkdir -p $MBED/lib $MBED/include
cp library/libmbedtls.a library/libmbedx509.a library/libmbedcrypto.a $MBED/lib/
cp -r include/mbedtls include/psa $MBED/include/
echo "mbedTLS built: $(ls $MBED/lib)"

# ---- build curl with mbedTLS ----
export CC="$CCZ"
export CFLAGS="-O2 -mcpu=603e -I$MBED/include"
export CPPFLAGS="-I$MBED/include"
export LDFLAGS="-L$MBED/lib -static -no-pie /opt/syscompat.o /opt/compat64.o"
export LIBS="-lmbedtls -lmbedx509 -lmbedcrypto"

SRC=/out/csrc; mkdir -p $SRC; cd $SRC
VER=8.5.0
[ -f curl-$VER.tar.gz ] || wget -q https://curl.se/download/curl-$VER.tar.gz
[ -d curl-mbed-$VER ] || { tar xf curl-$VER.tar.gz; mv curl-$VER curl-mbed-$VER; }
cd curl-mbed-$VER
# PATCH: curl 8.5 configures the mbedTLS RNG *after* mbedtls_ssl_setup, but mbedTLS 3.6
# requires it *before* (setup returns MBEDTLS_ERR_SSL_NO_RNG = -0x7400). Insert the
# conf_rng call ahead of ssl_setup. Idempotent.
if ! grep -q "conf_rng-before-setup" lib/vtls/mbedtls.c; then
  sed -i 's|  mbedtls_ssl_init(&backend->ssl);|  /* conf_rng-before-setup */\n  mbedtls_ssl_conf_rng(\&backend->config, mbedtls_ctr_drbg_random, \&backend->ctr_drbg);\n  mbedtls_ssl_init(\&backend->ssl);|' lib/vtls/mbedtls.c
  echo "applied conf_rng-before-setup patch"
fi
cat > config.cache <<'EOF'
ac_cv_func_setvbuf_reversed=no
ac_cv_sizeof_long=4
ac_cv_sizeof_size_t=4
ac_cv_sizeof_time_t=8
ac_cv_sizeof_off_t=8
EOF
./configure --host=powerpc-linux --build=aarch64-linux \
  --with-mbedtls=$MBED \
  --disable-shared --enable-static \
  --disable-ldap --disable-ldaps --disable-rtsp --disable-dict --disable-telnet \
  --disable-tftp --disable-pop3 --disable-imap --disable-smtp --disable-gopher \
  --disable-mqtt --disable-smb --disable-manual \
  --without-libpsl --without-librtmp --without-libidn2 --without-zstd --without-brotli --without-nghttp2 \
  --with-ca-bundle=/volume1/@modernsmb/ssl/cacert.pem \
  --cache-file=config.cache \
  >/tmp/curl.cfg 2>&1 || { echo "CONFIGURE FAIL"; tail -40 /tmp/curl.cfg; exit 51; }
echo "configure OK"
# recompile from scratch so curl's objects match the current mbedTLS struct layout
# (a relink-only against a rebuilt mbedTLS gives ABI-mismatched structs -> ssl_setup fails)
make clean >/dev/null 2>&1 || true
make -j4 >/tmp/curl.mk 2>&1 || { echo "MAKE FAIL"; tail -40 /tmp/curl.mk; exit 52; }
echo "build OK"

OUT=/out/curl-static; rm -rf $OUT; mkdir -p $OUT
CURLX="lib/base64.o lib/curl_multibyte.o lib/dynbuf.o lib/nonblock.o lib/strtoofft.o lib/timediff.o lib/version_win32.o lib/warnless.o"
zig cc -target $T -static -no-pie -o $OUT/curl \
  src/*.o $CURLX /opt/syscompat.o /opt/compat64.o \
  lib/.libs/libcurl.a \
  -L$MBED/lib -lmbedtls -lmbedx509 -lmbedcrypto \
  >/tmp/curl.relink 2>&1 || { echo "RELINK FAIL"; tail -40 /tmp/curl.relink; exit 53; }
file $OUT/curl
echo "=== qemu test ==="
qemu-ppc-static -r 2.6.24 $OUT/curl --version 2>&1 | head -3; echo "rc=${PIPESTATUS[0]}"
echo "===== curl+mbedtls DONE $(date) ====="
touch /out/curlmbed.OK
