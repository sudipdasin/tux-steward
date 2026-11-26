#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
import argparse,gzip,lzma,os,shutil,stat
parser=argparse.ArgumentParser(description='Build a small real-VM validation initramfs from extracted Debian packages.')
parser.add_argument('--lab',type=Path,required=True,help='External lab containing extracted packages under root/')
args=parser.parse_args()
base=args.lab.resolve()
kernel=next((base/'root/usr/lib/modules').iterdir())
image=base/'initrd-tree'
image.mkdir(exist_ok=True)
for p in ['proc','sys','dev','var/lib/dpkg','usr','usr-ro','usr-upper','usr-work','usr-overlay','tmp','run','home/tester','etc','etc/fonts','src','out','opt/builddeps','modules']:
 (image/p).mkdir(parents=True,exist_ok=True)
for name,target in [('bin','usr/bin'),('sbin','usr/sbin'),('lib','usr/lib'),('lib64','usr/lib64')]:
 p=image/name
 if not p.is_symlink():p.symlink_to(target)
shutil.copy2(base/'root/usr/bin/busybox',image/'busybox')
deps={}
for line in (kernel/'modules.dep').read_text().splitlines():
 name,ds=line.split(':',1);deps[name]=ds.split()
ordered=[]
def add(name):
 for dep in deps.get(name,[]): add(dep)
 if name not in ordered:ordered.append(name)
for ending in ['virtio_pci.ko.xz','9pnet_virtio.ko.xz','9p.ko.xz','virtio_blk.ko.xz','virtio_net.ko.xz','overlay.ko.xz']:
 matches=[n for n in deps if n.endswith('/'+ending)]
 if matches:add(matches[0])
for name in ordered:
 (image/'modules'/Path(name).name[:-3]).write_bytes(lzma.decompress((kernel/name).read_bytes()))
(image/'module-list').write_text('\n'.join('/modules/'+Path(n).name[:-3] for n in ordered)+'\n')
(image/'etc/passwd').write_text('root:x:0:0:root:/root:/bin/sh\ntester:x:1000:1000:VM Tester:/home/tester:/bin/sh\n')
(image/'etc/group').write_text('root:x:0:\ntester:x:1000:\n')
(image/'etc/os-release').write_text('PRETTY_NAME="SysPulse isolated Debian userspace VM"\nID=debian\nVERSION_ID="13"\n')
(image/'etc/nsswitch.conf').write_text('passwd: files\ngroup: files\nhosts: files\n')
(image/'etc/hosts').write_text('127.0.0.1 localhost\n')
(image/'etc/fonts/fonts.conf').write_text('''<?xml version="1.0"?><!DOCTYPE fontconfig SYSTEM "fonts.dtd"><fontconfig><dir>/usr/share/fonts</dir><cachedir>/tmp/fontconfig</cachedir><alias><family>sans-serif</family><prefer><family>DejaVu Sans</family></prefer></alias><alias><family>monospace</family><prefer><family>DejaVu Sans Mono</family></prefer></alias></fontconfig>''')
(image/'init').write_text('''#!/busybox sh
export PATH=/usr/bin:/usr/sbin:/bin:/sbin
/busybox chmod 755 /
/busybox mount -t proc proc /proc
/busybox mount -t sysfs sysfs /sys
/busybox mount -t devtmpfs devtmpfs /dev
exec </dev/console >/dev/console 2>&1
while read module; do /busybox insmod "$module" || exit 1; done </module-list
/busybox mount -t 9p -o trans=virtio,version=9p2000.L,ro,cache=loose,msize=262144 usr /usr-ro || exit 2
/busybox mount -t tmpfs tmpfs /usr-overlay || exit 7
/busybox mkdir -p /usr-overlay/upper /usr-overlay/work
/busybox mount -t overlay overlay -o lowerdir=/usr-ro,upperdir=/usr-overlay/upper,workdir=/usr-overlay/work,metacopy=on,index=off,redirect_dir=on /usr || { /busybox dmesg | /busybox tail -40; exit 7; }
# The sandbox's packaged /usr has anonymous ownership. Normalize ONLY guest
# overlay metadata for protected system bins; the host lower remains read-only.
/busybox chown 0:0 /usr /usr/bin /usr/sbin
/busybox chmod go-w /usr /usr/bin /usr/sbin
for tool in sh dash bash pkexec systemctl apt-get apt journalctl fstrim sysctl tlp zramctl; do
 for directory in /usr/bin /usr/sbin; do
  if [ -e "$directory/$tool" ]; then
   /busybox chown 0:0 "$directory/$tool"
   /busybox chmod go-w "$directory/$tool"
  fi
 done
done
/busybox mount -t 9p -o trans=virtio,version=9p2000.L,ro,cache=loose,msize=262144 deps /opt/builddeps || exit 3
/busybox mount -t 9p -o trans=virtio,version=9p2000.L,ro,cache=loose,msize=262144 source /src || exit 4
/busybox mount -t 9p -o trans=virtio,version=9p2000.L,msize=262144 output /out || exit 5
/busybox chmod 1777 /tmp
/busybox mkdir -p /run/user/1000
/busybox chown -R 1000:1000 /home/tester /run/user/1000
/busybox chmod 700 /run/user/1000
export HOME=/home/tester USER=tester LOGNAME=tester XDG_RUNTIME_DIR=/run/user/1000
export LD_LIBRARY_PATH=/opt/builddeps/usr/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu
export PATH=/opt/builddeps/usr/bin:/opt/builddeps/usr/lib/qt5/bin:/usr/bin:/usr/sbin
export CMAKE_PREFIX_PATH=/opt/builddeps/usr
export QT_QPA_PLATFORM=offscreen
export QT_PLUGIN_PATH=/usr/lib/x86_64-linux-gnu/qt5/plugins
export LANG=C.UTF-8
printf '\\nSYSPULSE_VM_READY\\n'
uname -a
/usr/bin/setpriv --reuid 1000 --regid 1000 --clear-groups /bin/sh /out/job.sh
rc=$?
printf '%s\\n' "$rc" >/out/job.exit
printf '\\nSYSPULSE_VM_JOB_EXIT=%s\\n' "$rc"
/busybox sync
/busybox poweroff -f
''')
(image/'init').chmod(0o755)
# Build a portable newc archive without requiring host mknod privileges.
entries=[]
for p in sorted(image.rglob('*')):
 name=str(p.relative_to(image));st=p.lstat();mode=st.st_mode
 data=os.readlink(p).encode() if p.is_symlink() else p.read_bytes() if p.is_file() else b''
 entries.append((name,mode,data,0,0))
entries.extend([('dev/console',stat.S_IFCHR|0o600,b'',5,1),('dev/null',stat.S_IFCHR|0o666,b'',1,3)])
with gzip.open(base/'initramfs.cpio.gz','wb',compresslevel=1) as f:
 offset=0
 for ino,(name,mode,data,major,minor) in enumerate(entries+[('TRAILER!!!',0,b'',0,0)],1):
  nb=name.encode()+b'\0';fields=[ino,mode,0,0,1,0,len(data),0,0,major,minor,len(nb),0]
  h=b'070701'+b''.join(f'{v:08x}'.encode() for v in fields)
  block=h+nb;block+=b'\0'*((-len(block))%4);block+=data;block+=b'\0'*((-len(data))%4);f.write(block)
print('Prepared',base/'initramfs.cpio.gz','with',len(ordered),'kernel modules')
