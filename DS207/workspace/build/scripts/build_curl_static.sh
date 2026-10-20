#!/bin/bash
# Static modern curl for DS207 (powerpc-linux-musl, zig) with GnuTLS backend
# (reusing the GnuTLS we already built for Samba -> TLS 1.2/1.3). Linked against
# syscompat.o for 2.6.24 syscall + time64 fallbacks. Produces a self-contained
# curl that does NOT touch the box's system OpenSSL.
set -e
exec > >(tee -a /out/curl.log) 2>&1
echo "===== static curl build $(date) ====="
SYS=/out/ppcsys
export PATH=/opt/zig:$PATH
T=powerpc-linux-musl

# qemu binfmt so autoconf's run-tests work for the cross target
if [ ! -f /proc/sys/fs/binfmt_misc/qemu-ppc ]; then
  mount binfmt_misc -t binfmt_misc /proc/sys/fs/binfmt_misc 2>/dev/null || true
  echo ':qemu-ppc:M::\x7fELF\x01\x02\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x14:\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff\xfe\xff\xff:/usr/local/bin/qemu-ppc-wrap:OC' \
    > /proc/sys/fs/binfmt_misc/register 2>/dev/null || echo "WARN: binfmt register failed (will try cached vars)"
fi
cat > /usr/local/bin/qemu-ppc-wrap <<'W'
#!/bin/sh
exec qemu-ppc-static -r 2.6.24 "$@"
W
chmod +x /usr/local/bin/qemu-ppc-wrap

# syscompat.o (same shim Samba uses): direct 2.6.24 syscalls for the 3 time
# functions that ENOSYS (clock_gettime/gettimeofday/time) + 2.6.27 syscall fallbacks.
zig cc -target $T -O2 -c /w/build/scripts/syscompat.c -o /opt/syscompat.o

# time64 trampolines (compat64.o): zig's powerpc musl headers __REDIR ~63 funcs to
# __*_time64 names its libc doesn't provide. Emit a weak tail-call trampoline from each
# __xxx_time64 to the plain musl xxx (32-bit time is fine for stat/gmtime/select/utimes
# on this box). syscompat.o's STRONG defs override the weak trampolines for the 3 that
# need real 2.6.24 syscalls.
INC=/opt/zig/lib/libc/musl/include
{ echo '.text'
  grep -rhoE "__REDIR\([a-z0-9_]+, *__[a-z0-9_]+\)" $INC 2>/dev/null \
    | sed -E 's/__REDIR\(([a-z0-9_]+), *(__[a-z0-9_]+)\)/\2 \1/' | sort -u \
    | while read tgt plain; do
        printf '.weak %s\n.type %s,@function\n%s:\n\tb %s\n' "$tgt" "$tgt" "$tgt" "$plain"
      done
} > /opt/time64.s
echo "time64 trampolines: $(grep -c '\.weak' /opt/time64.s)"
zig cc -target $T -c /opt/time64.s -o /opt/compat64.o

export CC="zig cc -target $T"
export CFLAGS="-O2 -mcpu=603e -I$SYS/include"
export CPPFLAGS="-I$SYS/include"
# static link: curl + gnutls crypto group + syscompat + no-pie static
export LDFLAGS="-L$SYS/lib -static -no-pie /opt/syscompat.o /opt/compat64.o -Wl,--start-group $SYS/lib/libgnutls.a $SYS/lib/libhogweed.a $SYS/lib/libnettle.a $SYS/lib/libtasn1.a $SYS/lib/libgmp.a $SYS/lib/libz.a -Wl,--end-group"
export LIBS="-lgnutls -lhogweed -lnettle -ltasn1 -lgmp -lz"
export PKG_CONFIG_PATH="$SYS/lib/pkgconfig"

SRC=/out/csrc; mkdir -p $SRC; cd $SRC
VER=8.5.0
[ -f curl-$VER.tar.gz ] || wget -q https://curl.se/download/curl-$VER.tar.gz
[ -d curl-$VER ] || tar xf curl-$VER.tar.gz
cd curl-$VER

# cached autoconf answers for the few run-tests (in case binfmt isn't available)
cat > config.cache <<'EOF'
ac_cv_func_setvbuf_reversed=no
ac_cv_sizeof_long=4
ac_cv_sizeof_size_t=4
ac_cv_sizeof_time_t=8
ac_cv_sizeof_off_t=8
EOF

./configure --host=powerpc-linux --build=aarch64-linux \
  --with-gnutls=$SYS \
  --disable-shared --enable-static \
  --disable-ldap --disable-ldaps --disable-rtsp --disable-dict --disable-telnet \
  --disable-tftp --disable-pop3 --disable-imap --disable-smtp --disable-gopher \
  --disable-mqtt --disable-smb --disable-manual --disable-ntlm \
  --without-libpsl --without-librtmp --without-libidn2 --without-zstd --without-brotli --without-nghttp2 \
  --with-ca-bundle=/volume1/@modernsmb/ssl/cacert.pem \
  --cache-file=config.cache \
  >/tmp/curl.cfg 2>&1 || { echo "CONFIGURE FAIL"; tail -40 /tmp/curl.cfg; exit 51; }
echo "configure OK"

# NOTE: LDFLAGS (with syscompat.o once) is already baked into the Makefile by
# configure; do NOT re-pass it to make or syscompat.o links twice (duplicate symbols).
make -j4 >/tmp/curl.mk 2>&1 || { echo "MAKE FAIL"; tail -50 /tmp/curl.mk; exit 52; }
echo "build OK"

OUT=/out/curl-static; rm -rf $OUT; mkdir -p $OUT
# libtool refuses a truly static link, so relink the tool ourselves from its objects.
# CURLX objects: lib sources compiled into the tool with the curlx_ macro context
CURLX="lib/base64.o lib/curl_multibyte.o lib/dynbuf.o lib/nonblock.o lib/strtoofft.o lib/timediff.o lib/version_win32.o lib/warnless.o"
zig cc -target $T -static -no-pie -o $OUT/curl \
  src/*.o $CURLX /opt/syscompat.o /opt/compat64.o \
  lib/.libs/libcurl.a \
  -L$SYS/lib -Wl,--start-group $SYS/lib/libgnutls.a $SYS/lib/libhogweed.a $SYS/lib/libnettle.a $SYS/lib/libtasn1.a $SYS/lib/libgmp.a $SYS/lib/libz.a -Wl,--end-group \
  >/tmp/curl.relink 2>&1 || { echo "RELINK FAIL"; tail -40 /tmp/curl.relink; exit 53; }
file $OUT/curl
echo "=== qemu test ==="
qemu-ppc-static -r 2.6.24 $OUT/curl --version 2>&1 | head -3
echo "===== curl DONE $(date) ====="
touch /out/curl.OK
