#!/bin/bash
# Cross-build Samba 4.15.13 for DS207 (powerpc-linux-musl) with zig + qemu cross-execute.
# Minimal SMB2/SMB3 file server with vfs_fruit. Bundled talloc/tdb/tevent/ldb/popt.
# Logs to /out/samba.log, marks /out/samba.OK on success.
set -e
exec > >(tee -a /out/samba.log) 2>&1
echo "===== samba build start $(date) ====="

SYS=/out/ppcsys
export PATH=/opt/zig:$PATH
TARGET=powerpc-linux-musl
export CC="/usr/local/bin/ppc-cc"
export CC_FOR_BUILD="zig cc"
export BUILD_CC="zig cc"
export AR="zig ar"; export RANLIB="zig ranlib"
export CFLAGS="-O2 -mcpu=603e -fPIC -I$SYS/include"
export LDFLAGS="-L$SYS/lib"
export PKG_CONFIG_PATH="$SYS/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$SYS/lib/pkgconfig"
SRCROOT=/out/bsrc; mkdir -p "$SRCROOT"; cd "$SRCROOT"
fetch(){ [ -f "$2" ] || wget -q -O "$2" "$1"; }

# waf mis-parses spaces in --cross-execute; wrap qemu in a no-arg script.
cat > /usr/local/bin/ppc-run <<'WRAP'
#!/bin/sh
exec qemu-ppc-static -r 2.6.24 "$@"
WRAP
chmod +x /usr/local/bin/ppc-run

# zig's linker rejects GNU soname shorthand "-h"; rewrite it to -soname.
cat > /usr/local/bin/ppc-cc <<'WRAP'
#!/bin/bash
newargs=()
while [ $# -gt 0 ]; do
  case "$1" in
    -Wl,-h,*) newargs+=("-Wl,-soname,${1#-Wl,-h,}");;
    -Wl,-h)   shift; newargs+=("-Wl,-soname,$1");;
    -h)       shift; newargs+=("-Wl,-soname,$1");;
    *)        newargs+=("$1");;
  esac
  shift
done
exec zig cc -target powerpc-linux-musl \
  "-Dinnetgr(a,b,c,d)=0" \
  "-Dsetnetgrent(a)=((void)0)" "-Dendnetgrent()=((void)0)" \
  "-Dgetnetgrent(a,b,c)=0" \
  -DNETDB_INTERNAL=-1 -DNETDB_SUCCESS=0 \
  "${newargs[@]}"
WRAP
chmod +x /usr/local/bin/ppc-cc

# ---- zlib (Samba needs system zlib) ----
if [ ! -f "$SYS/lib/libz.a" ]; then
  echo "### zlib ###"
  fetch https://zlib.net/fossils/zlib-1.3.1.tar.gz zlib-1.3.1.tar.gz
  rm -rf zlib-1.3.1; tar xf zlib-1.3.1.tar.gz; cd zlib-1.3.1
  CHOST=$TARGET ./configure --prefix=$SYS --static >/tmp/z.cfg 2>&1 || { tail -20 /tmp/z.cfg; exit 51; }
  make -j4 >/tmp/z.mk 2>&1 || { tail -20 /tmp/z.mk; exit 52; }
  make install >/dev/null 2>&1
  cd "$SRCROOT"; echo "zlib ok"
fi

# ---- linker-script .so: static-embed external deps into Samba's shared libs ----
L=$SYS/lib
printf "GROUP ( %s/libgnutls.a %s/libhogweed.a %s/libnettle.a %s/libtasn1.a %s/libgmp.a )\n" "$L" "$L" "$L" "$L" "$L" > "$L/libgnutls.so"
printf "GROUP ( %s/libz.a )\n" "$L" > "$L/libz.so"
echo "linker scripts written"

# ---- Samba ----
echo "### samba 4.15.13 ###"
fetch https://download.samba.org/pub/samba/stable/samba-4.15.13.tar.gz samba-4.15.13.tar.gz
[ -d samba-4.15.13 ] || { tar xf samba-4.15.13.tar.gz; }
cd samba-4.15.13

GNUTLS_CFLAGS="-I$SYS/include" GNUTLS_LIBS="-L$SYS/lib -lgnutls -lhogweed -lnettle -ltasn1 -lgmp" \
ZLIB_CFLAGS="-I$SYS/include" ZLIB_LIBS="-L$SYS/lib -lz" \
./configure \
  --cross-compile --cross-execute=/usr/local/bin/ppc-run \
  --prefix=/usr/syno/samba \
  --without-ad-dc --without-systemd --without-json \
  --without-libarchive --without-acl-support --without-ldap --without-ads --without-pam \
  --disable-python --disable-rpath \
  --bundled-libraries=ALL \
  --with-static-modules=vfs_fruit,vfs_catia,vfs_streams_xattr,vfs_default \
  --nonshared-binary=smbd/smbd,nmbd,smbpasswd,pdbedit,testparm,smbcontrol \
  >/tmp/smb.cfg 2>&1 ; CFGRC=$?; if [ $CFGRC -ne 0 ] && [ ! -f bin/c4che/default_cache.py ]; then echo "CONFIGURE FAIL"; tail -70 /tmp/smb.cfg; exit 61; fi
echo "configure OK (rc=$CFGRC)"

make -j4 >/tmp/smb.mk 2>&1 || { echo "MAKE FAIL"; tail -70 /tmp/smb.mk; exit 62; }
echo "make OK"

make install >/tmp/smb.inst 2>&1 || { echo "INSTALL FAIL"; tail -40 /tmp/smb.inst; exit 63; }
# stage into volume
rm -rf /out/samba-install
cp -a /usr/syno/samba /out/samba-install
echo "===== SAMBA OK $(date) ====="
ls -la /out/samba-install/sbin 2>/dev/null
touch /out/samba.OK
