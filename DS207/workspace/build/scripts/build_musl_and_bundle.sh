#!/bin/bash
# Build musl shared (libc.so + ld-musl loader) for powerpc, then assemble a
# self-contained Samba bundle and test smbd -V under qemu.
set -e
exec > >(tee -a /out/bundle.log) 2>&1
echo "===== musl+bundle $(date) ====="
export PATH=/opt/zig:$PATH
T=powerpc-linux-musl
MUSL=/out/muslsys
B=/out/bsrc/samba-4.15.13/bin/default
BUNDLE=/out/bundle          # staging = DS207 /usr/syno/samba
mkdir -p /tmp/src; cd /tmp/src

# ---- musl shared ----
if [ ! -f "$MUSL/lib/libc.so" ]; then
  echo "### musl ###"
  [ -f musl-1.2.5.tar.gz ] || wget -q https://musl.libc.org/releases/musl-1.2.5.tar.gz
  rm -rf musl-1.2.5; tar xf musl-1.2.5.tar.gz; cd musl-1.2.5
  ./configure --target=$T CC="zig cc -target $T" --prefix=$MUSL --disable-static >/tmp/musl.cfg 2>&1 \
     || { tail -25 /tmp/musl.cfg; exit 71; }
  make -j4 >/tmp/musl.mk 2>&1 || { tail -30 /tmp/musl.mk; exit 72; }
  make install >/dev/null 2>&1
  cd /tmp/src; echo "musl ok"
fi
echo "musl libc.so:"; ls -la $MUSL/lib/libc.so

# ---- assemble bundle ----
echo "### bundle ###"
rm -rf $BUNDLE; mkdir -p $BUNDLE/sbin $BUNDLE/bin $BUNDLE/lib $BUNDLE/lib/vfs
# binaries
cp $B/source3/smbd/smbd            $BUNDLE/sbin/
cp $B/source3/nmbd/nmbd            $BUNDLE/sbin/
cp $B/source3/smbpasswd            $BUNDLE/bin/ 2>/dev/null || true
cp $B/source3/pdbedit              $BUNDLE/bin/ 2>/dev/null || true
cp $B/source3/utils/smbcontrol     $BUNDLE/bin/ 2>/dev/null || true
cp $B/source3/utils/testparm       $BUNDLE/bin/ 2>/dev/null || true
cp $B/source3/smbclient            $BUNDLE/bin/ 2>/dev/null || true
# vfs modules
for m in fruit catia streams_xattr; do
  f=$(find $B -name "libvfs_module_${m}.so" | head -1)
  [ -n "$f" ] && cp "$f" "$BUNDLE/lib/vfs/${m}.so"
done
# musl loader + libc
cp $MUSL/lib/libc.so $BUNDLE/lib/libc.so
ln -sf libc.so $BUNDLE/lib/ld-musl-powerpc.so.1

# collect the full .so dependency closure for all binaries
collect() {
  local changed=1
  while [ $changed -eq 1 ]; do
    changed=0
    for bin in $BUNDLE/sbin/* $BUNDLE/bin/* $BUNDLE/lib/*.so $BUNDLE/lib/vfs/*.so; do
      [ -f "$bin" ] || continue
      for need in $(readelf -d "$bin" 2>/dev/null | awk '/NEEDED/{gsub(/[\[\]]/,"",$5);print $5}'); do
        case "$need" in libc.so|ld-musl*) continue;; esac
        if [ ! -f "$BUNDLE/lib/$need" ]; then
          src=$(find $B -name "$need" -type f | head -1)
          if [ -n "$src" ]; then cp "$src" "$BUNDLE/lib/$need"; changed=1; fi
        fi
      done
    done
  done
}
collect
echo "libs collected: $(ls $BUNDLE/lib/*.so* | wc -l)"

# patch interpreter + rpath so the bundle is self-contained on DS207
PREFIX=/usr/syno/samba       # final install path on the NAS
for bin in $BUNDLE/sbin/* $BUNDLE/bin/*; do
  [ -f "$bin" ] || continue
  patchelf --set-interpreter $PREFIX/lib/ld-musl-powerpc.so.1 "$bin"
  patchelf --set-rpath "$PREFIX/lib:$PREFIX/lib/vfs" "$bin"
  powerpc-linux-strip "$bin" 2>/dev/null || llvm-strip "$bin" 2>/dev/null || true
done
for so in $BUNDLE/lib/*.so* $BUNDLE/lib/vfs/*.so; do
  [ -f "$so" ] || continue
  patchelf --set-rpath "$PREFIX/lib:$PREFIX/lib/vfs" "$so" 2>/dev/null || true
done

echo "=== bundle size ==="; du -sh $BUNDLE
echo "=== smbd deps now ==="; readelf -d $BUNDLE/sbin/smbd 2>/dev/null | grep -E "NEEDED|RPATH|RUNPATH" | head

# ---- TEST under qemu (map DS207 prefix to the bundle via -L sysroot) ----
echo "### qemu test ###"
ROOT=/tmp/sysroot; rm -rf $ROOT; mkdir -p $ROOT/usr/syno
cp -a $BUNDLE $ROOT/usr/syno/samba
echo "--- smbd -V ---"
qemu-ppc-static -r 2.6.24 -L $ROOT $ROOT/usr/syno/samba/sbin/smbd -V 2>&1 | head -5; echo "smbd-rc=${PIPESTATUS[0]}"
echo "--- testparm --version ---"
qemu-ppc-static -r 2.6.24 -L $ROOT $ROOT/usr/syno/samba/bin/testparm --version 2>&1 | head -3; echo "tp-rc=${PIPESTATUS[0]}"

# stage bundle out
rm -rf /out/samba-bundle; cp -a $BUNDLE /out/samba-bundle
echo "===== BUNDLE DONE $(date) ====="
touch /out/bundle.OK
