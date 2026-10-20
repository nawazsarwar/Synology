#!/bin/bash
# Build a modernized DS207 hda1.tgz (stock rootfs + our static Samba4/Dropbear/avahi,
# stock smbd kept as .orig) and assemble DS207_Modernized_Custom.pat.
# Runs as root in the Linux container so perms / device nodes / symlinks are preserved.
set -e
exec > >(tee -a /out/pat.log) 2>&1
echo "===== package pat $(date) ====="
SRC=/w/ds207_extracted          # original .pat members (VERSION,hda1.tgz,zImage,updater,rd.bin,checksum.syno)
PAY=/w/build/ds207_patch/ds207_modern_payload.tgz
OUT=/out/pat; rm -rf $OUT; mkdir -p $OUT
NR=/tmp/newroot; rm -rf $NR; mkdir -p $NR

echo "### extract stock rootfs ###"
tar xzf $SRC/hda1.tgz -C $NR 2>/dev/null || true   # dev-node mknod may warn under some fs; fine here

echo "### overlay modern payload ###"
tar xzf $PAY -C $NR
mkdir -p $NR/usr/syno/samba/var/lock $NR/usr/syno/samba/var/locks $NR/usr/syno/samba/var/cache $NR/usr/syno/samba/var/run $NR/usr/syno/samba/private

echo "### swap smbd/nmbd (keep stock as .orig), use DSM user db ###"
for b in smbd nmbd; do
  if [ -e "$NR/usr/syno/sbin/$b" ] && [ ! -e "$NR/usr/syno/sbin/$b.orig" ]; then
    mv "$NR/usr/syno/sbin/$b" "$NR/usr/syno/sbin/$b.orig"
  fi
  ln -sf /usr/syno/samba/sbin/$b "$NR/usr/syno/sbin/$b"
done
ln -sf /usr/syno/etc/private/smbpasswd "$NR/usr/syno/samba/private/smbpasswd"

# install our smb.conf as the compiled-prefix default (our smbd reads this)
cp /w/build/configs/smb.conf.ds207 "$NR/usr/syno/samba/etc/smb.conf"

echo "### repack hda1.tgz ###"
( cd $NR && tar czf $OUT/hda1.tgz . )
ls -la $OUT/hda1.tgz

echo "### assemble checksum.syno ###"
cp $SRC/VERSION $SRC/zImage $SRC/updater $SRC/rd.bin $OUT/
python3 -I - "$SRC/checksum.syno" "$OUT" <<'PY'
import sys, zlib, os
orig_path, outdir = sys.argv[1], sys.argv[2]
# parse original field4/field5 for the UNCHANGED files (copied verbatim)
orig = {}
for line in open(orig_path):
    p = line.split()
    if len(p) == 5 and not line.startswith('#'):
        crc, size, name, f4, f5 = p
        orig[name] = (int(f4), int(f5))
members = ["VERSION", "hda1.tgz", "zImage", "updater", "rd.bin"]
lines = []
for m in members:
    data = open(os.path.join(outdir, m), "rb").read()
    crc = zlib.crc32(data) & 0xffffffff
    size = len(data)
    if m == "hda1.tgz":
        # CHANGED: crc32+size correct; field4/field5 algorithm not reverse-engineered.
        f4, f5 = 0, 0
    else:
        f4, f5 = orig.get(m, (0, 0))   # unchanged -> verbatim
    lines.append((crc, size, m, f4, f5))
Ssize = sum(l[1] for l in lines)
Sf4 = sum(l[3] for l in lines)
Sf5 = sum(l[4] for l in lines)
with open(os.path.join(outdir, "checksum.syno"), "w") as f:
    for crc, size, m, f4, f5 in lines:
        f.write(f"{crc} {size} {m} {f4} {f5}\n")
    f.write(f"#Synocksum {Ssize} {Sf4} {Sf5}\n")
print("checksum.syno written (hda1 field4/5 = 0 = UNVERIFIED)")
PY
cat $OUT/checksum.syno

echo "### build .pat (Synology member order) ###"
( cd $OUT && tar cf /out/DS207_Modernized_Custom.pat VERSION hda1.tgz zImage updater rd.bin checksum.syno )
ls -la /out/DS207_Modernized_Custom.pat
echo "===== PAT DONE $(date) ====="
touch /out/pat.OK
