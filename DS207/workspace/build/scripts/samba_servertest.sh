set -e
S=/out/samba-static
R=/tmp/srv
rm -rf $R; mkdir -p $R/etc $R/private $R/var/{lock,locks,cache,run,log} $R/share
# config with test ports + paths
cat > $R/etc/smb.conf <<CONF
[global]
   workgroup = WORKGROUP
   netbios name = DS207
   server min protocol = SMB2_02
   server max protocol = SMB3
   security = user
   map to guest = Bad User
   smb ports = 4455
   private dir = $R/private
   lock directory = $R/var/lock
   state directory = $R/var/locks
   cache directory = $R/var/cache
   pid directory = $R/var/run
   passdb backend = smbpasswd:$R/private/smbpasswd
   log file = $R/var/log.%m
   log level = 1
   vfs objects = catia fruit streams_xattr
   fruit:metadata = stream
[public]
   path = $R/share
   read only = no
   guest ok = yes
CONF
echo "test test" > $R/share/hello.txt
Q="qemu-ppc-static -r 2.6.24"
echo "=== start smbd (foreground, bg) ==="
$Q $S/sbin/smbd -F --no-process-group -s $R/etc/smb.conf --debug-stdout > $R/smbd.log 2>&1 &
SMBPID=$!
# wait for port 4455 to listen
for i in $(seq 1 25); do
  if $Q $S/bin/smbclient -p 4455 -N -L 127.0.0.1 >/tmp/list.out 2>/tmp/list.err; then break; fi
  sleep 1
done
echo "=== smbclient -L (list shares, anonymous, SMB2) ==="
cat /tmp/list.out 2>/dev/null | head -25
echo "--- stderr ---"; head -5 /tmp/list.err 2>/dev/null
echo "=== smbd log tail ==="; tail -8 $R/smbd.log
echo "=== protocol check: force SMB3 ==="
$Q $S/bin/smbclient -p 4455 -N -m SMB3 -L 127.0.0.1 2>&1 | grep -iE "Sharename|public|SMB1|protocol negotiation" | head
kill $SMBPID 2>/dev/null || true
