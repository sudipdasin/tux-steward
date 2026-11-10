"""Explicit action catalog. Never executes shell text or source scripts."""
from dataclasses import dataclass
import os
import shutil
import subprocess
import time

@dataclass(frozen=True)
class Action:
    id: str
    group: str
    title: str
    description: str
    command: tuple[str, ...]
    changes: bool = False
    admin: bool = False
    x11: bool = False

ACTIONS = [
    Action('storage','Storage','Storage usage','Review mounted filesystems and free space.',('df','-hT')),
    Action('devices','Storage','Disks & filesystems','Inspect devices, rotation type, mount points and filesystem types.',('lsblk','-o','NAME,SIZE,ROTA,FSTYPE,MOUNTPOINTS')),
    Action('mounts','Storage','Mount map','Inspect loop devices and filesystem mounts before planning repairs.',('findmnt',)),
    Action('trim-check','Storage','SSD trim schedule','Inspect the system SSD trim timer.',('systemctl','status','fstrim.timer','--no-pager')),
    Action('trim','Storage','Trim SSDs','Reclaim unused blocks on supported mounted SSD filesystems. Requires administrator authentication.',('fstrim','--all','--verbose'),True,True),
    Action('failed','Diagnostics','Failed services','Inspect failed system services.',('systemctl','--failed','--no-pager')),
    Action('kernel','Diagnostics','Kernel warnings','Read the latest 150 kernel warnings from this boot. Access depends on local journal permissions.',('journalctl','-k','-b','-p','warning','-n','150','--no-pager')),
    Action('journal','Diagnostics','System warnings','Read the latest 200 warnings and errors.',('journalctl','-b','-p','warning','-n','200','--no-pager')),
    Action('journal-size','Maintenance','Journal disk usage','Measure journal storage before deciding whether to prune.',('journalctl','--disk-usage')),
    Action('apt-preview','Maintenance','Preview unused packages','Simulate APT autoremove. No packages are removed.',('apt-get','--simulate','autoremove')),
    Action('apt-clean','Maintenance','Clean downloaded packages','Delete cached APT package downloads. Installed applications remain available.',('apt-get','clean'),True,True),
    Action('journal-prune','Maintenance','Retain seven days of logs','Permanently remove archived journal files older than seven days. Export needed diagnostics first.',('journalctl','--vacuum-time=7d'),True,True),
    Action('timers','Schedules','Scheduled system tasks','Review timers including trim and package maintenance.',('systemctl','list-timers','--all','--no-pager')),
    Action('user-timers','Schedules','Your scheduled tasks','Inspect timers in the current user session.',('systemctl','--user','list-timers','--all','--no-pager')),
    Action('trim-enable','Schedules','Enable weekly SSD trim','Enable and start the distribution-provided fstrim timer. Undo: sudo systemctl disable --now fstrim.timer',('systemctl','enable','--now','fstrim.timer'),True,True),
    Action('memory','Performance','Memory & swap','Inspect RAM and swap usage before tuning.',('free','-h')),
    Action('zram','Performance','Compressed swap status','Inspect active zRAM devices.',('zramctl',)),
    Action('power','Performance','Power management status','Inspect TLP if installed. No packages are installed automatically.',('tlp-stat','-s')),
    Action('compositor-on','Performance','Enable XFCE compositor','Enable desktop compositing. Reversible using Disable compositor.',('xfconf-query','-c','xfwm4','-p','/general/use_compositing','-s','true'),True),
    Action('compositor-off','Performance','Disable XFCE compositor','May reduce rendering overhead; can cause tearing. Reversible using Enable compositor.',('xfconf-query','-c','xfwm4','-p','/general/use_compositing','-s','false'),True),
    Action('input','Input','List X11 input devices','Inspect exact device IDs. X11 only; permanent keyboard disabling is intentionally deferred.',('xinput','list'),x11=True),
    Action('wine','Gaming','Wine version','Check whether Wine is installed.',('wine','--version')),
    Action('lutris','Gaming','Lutris version','Check whether Lutris is installed.',('lutris','--version')),
    Action('vulkan','Gaming','Vulkan capabilities','Inspect graphics driver support.',('vulkaninfo','--summary')),
    Action('arch','Gaming','32-bit architectures','Inspect enabled foreign package architectures.',('dpkg','--print-foreign-architectures')),
]
CATALOG = {a.id: a for a in ACTIONS}

def unavailable(a):
    if a.x11 and os.environ.get('XDG_SESSION_TYPE') == 'wayland':
        return 'Requires an X11 session; XWayland does not control native Wayland devices.'
    if not shutil.which(a.command[0]):
        return f'Missing dependency: {a.command[0]}'
    if a.admin and not shutil.which('pkexec'):
        return 'Missing dependency: pkexec (PolicyKit authentication agent required)'
    return ''

def execute(action_id, confirmed=False, timeout=120):
    a = CATALOG[action_id]
    if a.changes and not confirmed:
        raise PermissionError('Review and confirm the action first.')
    problem = unavailable(a)
    if problem:
        raise RuntimeError(problem)
    command = (('pkexec',) if a.admin else ()) + a.command
    started = time.monotonic()
    try:
        p = subprocess.run(command, capture_output=True, text=True, errors='replace', timeout=timeout, shell=False)
        return {'action':a.title,'command':list(command),'code':p.returncode,'output':(p.stdout+p.stderr)[-250000:], 'seconds':round(time.monotonic()-started,2)}
    except subprocess.TimeoutExpired:
        return {'action':a.title,'command':list(command),'code':124,'output':'Timed out. Check system state before retrying; changes may have partially completed.', 'seconds':round(time.monotonic()-started,2)}
