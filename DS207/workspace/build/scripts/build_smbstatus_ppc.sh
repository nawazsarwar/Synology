#!/bin/bash
# Static smbstatus only (derived from build_samba_static.sh). Reconfigures with smbstatus nonshared.
# Static => no musl loader needed (self-contained like Dropbear). vfs_fruit/catia/
# streams_xattr built in. Assumes /out/ppcsys deps (gmp/nettle/tasn1/gnutls/zlib, PIC).
set -e
exec > >(tee -a /out/smbstatus.log) 2>&1
echo "===== static samba build $(date) ====="
SYS=/out/ppcsys
export PATH=/opt/zig:$PATH
T=powerpc-linux-musl
export CC=/usr/local/bin/ppc-cc
export CC_FOR_BUILD="zig cc"; export BUILD_CC="zig cc"
export AR="zig ar"; export RANLIB="zig ranlib"
export CFLAGS="-O2 -mcpu=603e -fPIC -I$SYS/include"
export LDFLAGS="-L$SYS/lib"
export PKG_CONFIG_PATH="$SYS/lib/pkgconfig"; export PKG_CONFIG_LIBDIR="$SYS/lib/pkgconfig"

# qemu wrapper for waf cross-execute
cat > /usr/local/bin/ppc-run <<'W1'
#!/bin/sh
exec qemu-ppc-static -r 2.6.24 "$@"
W1
# zig's musl/ppc headers __REDIR ~63 funcs to __*_time64 but its libc lacks those
# symbols. Provide each as a weak PPC tail-call trampoline to the plain libc
# function (in asm there is no header redirect, so we branch straight to it).
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
# 2.6.24 syscall fallbacks (signalfd4->signalfd, eventfd2->eventfd, pipe2, accept4,
# epoll_create1, dup3, inotify_init1) — musl uses 2.6.27+ variants that ENOSYS on
# the DS207 kernel and make Samba's tevent panic. Force-linked into the binaries.
zig cc -target $T -O2 -c /w/build/scripts/syscompat.c -o /opt/syscompat.o

# ppc-cc has two modes via /opt/cc_mode: "configure" (no compat force-link, so
# configure's runtime feature tests behave normally) and "build" (force-link
# compat64.o on link steps to satisfy Samba's time64 symbol refs).
cat > /usr/local/bin/ppc-cc <<'W2'
#!/bin/bash
newargs=(); compiling=0; outf=""
prev=""
for a in "$@"; do
  [ "$a" = "-c" ] && compiling=1
  case "$a" in -o*) [ "$a" = "-o" ] || outf="${a#-o}";; esac
  [ "$prev" = "-o" ] && outf="$a"
  prev="$a"
done
base="__PENDING__"
# first pass only to know the output name (already have outf); classify
case "$(basename "$outf" 2>/dev/null)" in
  smbd|nmbd|smbpasswd|pdbedit|testparm|smbcontrol|smbclient|smbstatus) tgtbin=1;; *) tgtbin=0;;
esac
isbuild=0; [ "$(cat /opt/cc_mode 2>/dev/null)" = "build" ] && isbuild=1
while [ $# -gt 0 ]; do
  case "$1" in
    -Wl,-h,*) newargs+=("-Wl,-soname,${1#-Wl,-h,}");;
    -Wl,-h) shift; newargs+=("-Wl,-soname,$1");;
    -h) shift; newargs+=("-Wl,-soname,$1");;
    # for final static binaries, drop PIE / dynamic-linker flags that fight -static
    -pie|-fPIE|-fpie|-Wl,-z,now|-Wl,-pie)
       if [ $isbuild -eq 1 ] && [ $tgtbin -eq 1 ] && [ $compiling -eq 0 ]; then :; else newargs+=("$1"); fi;;
    -Wl,-dynamic-linker,*|-Wl,-Bdynamic)
       if [ $isbuild -eq 1 ] && [ $tgtbin -eq 1 ] && [ $compiling -eq 0 ]; then :; else newargs+=("$1"); fi;;
    *) newargs+=("$1");;
  esac; shift
done
extra=()
# NOTE: zig's musl libc already provides the __*_time64 symbols; no trampolines
# needed. The earlier "undefined" errors were link-order/.so artifacts.
if [ $isbuild -eq 1 ] && [ $compiling -eq 0 ]; then
  case "$(basename "$outf" 2>/dev/null)" in
    smbd|nmbd|smbpasswd|pdbedit|testparm|smbcontrol|smbclient|smbstatus)
      # static mode: lld ignores our linker-script .so, so pass the crypto chain
      # explicitly in a group (circular deps gnutls<->nettle<->hogweed<->tasn1<->gmp)
      extra+=(/opt/syscompat.o -static -no-pie -Wl,--strip-all
        -Wl,--start-group
        /out/ppcsys/lib/libgnutls.a /out/ppcsys/lib/libhogweed.a /out/ppcsys/lib/libnettle.a
        /out/ppcsys/lib/libtasn1.a /out/ppcsys/lib/libgmp.a /out/ppcsys/lib/libz.a
        -Wl,--end-group);;
  esac
fi
exec zig cc -target powerpc-linux-musl \
  "-Dinnetgr(a,b,c,d)=0" "-Dsetnetgrent(a)=((void)0)" "-Dendnetgrent()=((void)0)" "-Dgetnetgrent(a,b,c)=0" \
  -DNETDB_INTERNAL=-1 -DNETDB_SUCCESS=0 \
  "${newargs[@]}" "${extra[@]}"
W2
chmod +x /usr/local/bin/ppc-run /usr/local/bin/ppc-cc
echo configure > /opt/cc_mode

# linker-script .so to static-embed external deps
L=$SYS/lib
printf "GROUP ( %s/libgnutls.a %s/libhogweed.a %s/libnettle.a %s/libtasn1.a %s/libgmp.a )\n" "$L" "$L" "$L" "$L" "$L" > "$L/libgnutls.so"
printf "GROUP ( %s/libz.a )\n" "$L" > "$L/libz.so"

SRC=/out/bsrc-static; mkdir -p $SRC; cd $SRC
[ -f samba-4.15.13.tar.gz ] || wget -q https://download.samba.org/pub/samba/stable/samba-4.15.13.tar.gz
[ -d samba-4.15.13 ] || tar xf samba-4.15.13.tar.gz
cd samba-4.15.13

if ! grep -q smbstatus bin/c4che/default_cache.py 2>/dev/null; then
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
  --with-shared-modules='!vfs_snapper' \
  --nonshared-binary=smbd/smbd,nmbd,smbpasswd,pdbedit,testparm,smbcontrol,smbclient,smbstatus \
  >/tmp/smb.cfg 2>&1 || { echo "CONFIGURE FAIL"; tail -70 /tmp/smb.cfg; exit 61; }
fi
echo "configure OK"
# static (nonshared) merge puts smbd/notify_msg.c in both smbstatus and smbd_base -> waf error.
# smbstatus already deps on smbd_base, so drop the duplicate source (idempotent).
sed -i "s# '../smbd/notify_msg.c'#'#; s#'status.c ../smbd/notify_msg.c'#'status.c'#" source3/utils/wscript_build
echo build > /opt/cc_mode   # from here, force-link compat64.o on link steps

TGT="smbstatus"
# force relink of final binaries (wrapper flag changes aren't tracked by waf)
B0=bin/default
for p in source3/smbd/smbd source3/nmbd/nmbd source3/utils/smbpasswd source3/utils/pdbedit source3/utils/smbcontrol source3/utils/testparm source3/smbclient source3/utils/smbstatus; do
  [ -f "$B0/$p" ] && mv -f "$B0/$p" "$B0/$p.relink.bak" 2>/dev/null || true
done
./buildtools/bin/waf build --targets="$TGT" -j4 >/tmp/smb.mk 2>&1 || { echo "MAKE FAIL"; tail -60 /tmp/smb.mk; exit 62; }
echo "build OK"

B=bin/default
f=$B/source3/utils/smbstatus
file $f
mkdir -p /out/smbstatus-static; cp $f /out/smbstatus-static/smbstatus
# stripped at link time (-Wl,--strip-all in ppc-cc); no PPC strip tool in the container
cp /out/smbstatus-static/smbstatus /out/smbstatus
file /out/smbstatus; ls -la /out/smbstatus
qemu-ppc-static -r 2.6.24 /out/smbstatus --version; echo rc=$?
touch /out/smbstatus.OK
