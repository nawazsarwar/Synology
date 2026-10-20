#!/bin/bash
# Cross-build Samba's mandatory dependency chain for DS207 (powerpc-linux-musl) with zig.
# Static libs installed into a sysroot so Samba's waf can find them.
# Runs inside the ds207-zig image.  Logs to /out/deps.log, marks /out/deps.OK on success.
set -e
exec > >(tee -a /out/deps.log) 2>&1
echo "===== deps build start $(date) ====="

SYS=/out/ppcsys                       # cross sysroot (install prefix)
mkdir -p "$SYS" /tmp/src
export PATH=/opt/zig:$PATH
TARGET=powerpc-linux-musl
export CC="zig cc -target $TARGET"
export CXX="zig c++ -target $TARGET"
export AR="zig ar"
export RANLIB="zig ranlib"
export CC_FOR_BUILD="zig cc"
export BUILD_CC="zig cc"
export CXX_FOR_BUILD="zig c++"
export GMP_CC_FOR_BUILD="zig cc"
export CFLAGS="-O2 -mcpu=603e -fPIC -ffunction-sections -fdata-sections"
export CPPFLAGS="-I$SYS/include"
export LDFLAGS="-L$SYS/lib"
export PKG_CONFIG_PATH="$SYS/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$SYS/lib/pkgconfig"
HOST=powerpc-linux
J=4

fetch(){ cd /tmp/src; [ -f "$2" ] || wget -q -O "$2" "$1"; }

# ---- 1. GMP ----
echo "### GMP ###"
fetch https://gmplib.org/download/gmp/gmp-6.3.0.tar.xz gmp-6.3.0.tar.xz
tar xf gmp-6.3.0.tar.xz; cd gmp-6.3.0
# GMP needs a known ABI; force 32-bit, disable assembly (zig/llvm ppc asm mismatch) for safety.
./configure --host=$HOST --prefix=$SYS --disable-shared --enable-static --with-pic \
   --disable-assembly ABI=32 >/tmp/gmp.cfg 2>&1 || { tail -30 /tmp/gmp.cfg; exit 11; }
make -j$J >/tmp/gmp.mk 2>&1 || { tail -30 /tmp/gmp.mk; exit 12; }
make install >/dev/null 2>&1
echo "GMP ok"

# ---- 2. libtasn1 ----
echo "### libtasn1 ###"
fetch https://ftp.gnu.org/gnu/libtasn1/libtasn1-4.19.0.tar.gz libtasn1-4.19.0.tar.gz
tar xf libtasn1-4.19.0.tar.gz; cd libtasn1-4.19.0
./configure --host=$HOST --prefix=$SYS --disable-shared --enable-static --with-pic --disable-doc >/tmp/ta.cfg 2>&1 || { tail -30 /tmp/ta.cfg; exit 21; }
make -j$J >/tmp/ta.mk 2>&1 || { tail -30 /tmp/ta.mk; exit 22; }
make install >/dev/null 2>&1
echo "libtasn1 ok"

# ---- 3. Nettle (needs GMP) ----
echo "### nettle ###"
fetch https://ftp.gnu.org/gnu/nettle/nettle-3.9.1.tar.gz nettle-3.9.1.tar.gz
tar xf nettle-3.9.1.tar.gz; cd nettle-3.9.1
./configure --host=$HOST --prefix=$SYS --disable-shared --enable-static --with-pic \
   --disable-assembler --disable-documentation \
   --with-include-path=$SYS/include --with-lib-path=$SYS/lib >/tmp/ne.cfg 2>&1 || { tail -40 /tmp/ne.cfg; exit 31; }
make -j$J >/tmp/ne.mk 2>&1 || { tail -40 /tmp/ne.mk; exit 32; }
make install >/dev/null 2>&1
echo "nettle ok"

# ---- 4. GnuTLS (needs nettle, gmp, libtasn1) ----
echo "### gnutls ###"
fetch https://www.gnupg.org/ftp/gcrypt/gnutls/v3.7/gnutls-3.7.11.tar.xz gnutls-3.7.11.tar.xz
tar xf gnutls-3.7.11.tar.xz; cd gnutls-3.7.11
GMP_CFLAGS="-I$SYS/include" GMP_LIBS="-L$SYS/lib -lgmp" \
NETTLE_CFLAGS="-I$SYS/include" NETTLE_LIBS="-L$SYS/lib -lnettle" \
HOGWEED_CFLAGS="-I$SYS/include" HOGWEED_LIBS="-L$SYS/lib -lhogweed -lnettle -lgmp" \
LIBTASN1_CFLAGS="-I$SYS/include" LIBTASN1_LIBS="-L$SYS/lib -ltasn1" \
./configure --host=$HOST --prefix=$SYS --disable-shared --enable-static --with-pic \
   --with-included-unistring --without-p11-kit --without-idn --without-tpm --without-tpm2 \
   --disable-doc --disable-tools --disable-tests --disable-cxx \
   --disable-libdane --without-zlib --without-brotli --without-zstd \
   --disable-nls >/tmp/gt.cfg 2>&1 || { tail -50 /tmp/gt.cfg; exit 41; }
make -j$J >/tmp/gt.mk 2>&1 || { tail -50 /tmp/gt.mk; exit 42; }
make install >/dev/null 2>&1
echo "gnutls ok"

echo "===== ALL DEPS OK $(date) ====="
touch /out/deps.OK
