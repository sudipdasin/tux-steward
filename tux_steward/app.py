"""Small native desktop UI; workers never touch Tk widgets."""
import json
import os
import platform
import queue
import shlex
import threading
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
from .core import ACTIONS, execute, unavailable, matches
from . import __version__

BG='#101827'; PANEL='#192438'; TEXT='#e7eef9'; MUTED='#9aacc5'; ACCENT='#66dfbf'
GUIDES = {
'Storage':'Filesystem repair: back up first, identify the exact device, and use a live environment for offline checks. This version never repairs mounted filesystems or recursively changes ownership.',
'Maintenance':'Start with previews. Trash deletion, kernel purging, and broad cache deletion from the source scripts are not automated in this version.',
'Performance':'Measure before tuning. zRAM, swappiness and TLP package configuration need hardware-specific validation and are planned for a later version. Security update checks stay enabled.',
'Input':'Only device inspection is available initially. Temporary device toggles with timed recovery and permanent-rule backup/restore are planned.',
'Gaming':'This version checks the gaming environment. Managed per-game prefixes, import, launchers, and uninstall are planned. No Wine processes are killed and existing prefixes remain untouched.',
'Schedules':'Review existing timers first. SSD scheduling uses your distribution’s fstrim timer. Custom weekly cleanup plans and HDD profiles are planned.',
'Diagnostics':'Permission errors are shown directly. Reports may contain hostnames and device details; review them before sharing.'}

class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title('Tux Steward · Linux System Tools')
        self.geometry('1120x800'); self.minsize(900,650); self.configure(bg=BG)
        self.results=[]; self.events=queue.Queue(); self.live=queue.Queue(maxsize=64); self.busy=False
        self.rows=[]
        self.protocol('WM_DELETE_WINDOW',self.close)
        style=ttk.Style(self); style.theme_use('clam')
        style.configure('.',background=PANEL,foreground=TEXT,font=('DejaVu Sans',10))
        style.configure('TNotebook',background=BG,borderwidth=0)
        style.configure('TNotebook.Tab',padding=(16,10),background=PANEL,foreground=MUTED)
        style.map('TNotebook.Tab',background=[('selected','#293c54')],foreground=[('selected',ACCENT)])
        style.configure('TButton',padding=(12,7),background='#2b4359',foreground=TEXT)
        menu=tk.Menu(self); self.config(menu=menu)
        file=tk.Menu(menu,tearoff=False); menu.add_cascade(label='File',menu=file)
        file.add_command(label='Export session report…',command=self.export); file.add_separator(); file.add_command(label='Quit',command=self.close)
        helpmenu=tk.Menu(menu,tearoff=False); menu.add_cascade(label='Help',menu=helpmenu)
        helpmenu.add_command(label='About & roadmap',command=lambda:messagebox.showinfo('Tux Steward',f'Version {__version__} · GPL-3.0-or-later\nPrivate development alpha.\nLinux Mint / XFCE first.\nRoadmap: managed Wine games, recovery-safe input controls, maintenance plans, packaging.'))
        top=tk.Frame(self,bg=BG); top.pack(fill='x',padx=26,pady=(22,10))
        tk.Label(top,text='◉  TUX STEWARD',bg=BG,fg=ACCENT,font=('DejaVu Sans',12,'bold')).pack(anchor='w')
        tk.Label(top,text='A calmer way to care for Linux.',bg=BG,fg=TEXT,font=('DejaVu Sans',24,'bold')).pack(anchor='w',pady=(8,3))
        tk.Label(top,text=f'{platform.system()} {platform.release()}   •   {platform.machine()}   •   v{__version__} alpha',bg=BG,fg=MUTED).pack(anchor='w')
        self.status=tk.StringVar(value='Ready · Choose a tool to review its command')
        tk.Label(self,textvariable=self.status,bg=BG,fg=ACCENT,anchor='w').pack(fill='x',padx=28,pady=8)
        searchbar=tk.Frame(self,bg=BG); searchbar.pack(fill='x',padx=26,pady=(0,8))
        tk.Label(searchbar,text='Find tools',bg=BG,fg=MUTED).pack(side='left',padx=(0,10))
        self.search=tk.StringVar()
        ttk.Entry(searchbar,textvariable=self.search).pack(side='left',fill='x',expand=True)
        ttk.Button(searchbar,text='Clear',command=lambda:self.search.set('')).pack(side='left',padx=8)
        self.search.trace_add('write',self.filter_tools)
        self.book=ttk.Notebook(self); self.book.pack(fill='both',expand=True,padx=24)
        for group in ['Diagnostics','Storage','Maintenance','Performance','Input','Gaming','Schedules']:
            page=ttk.Frame(self.book,padding=16); self.book.add(page,text=group)
            tk.Label(page,text=group,bg=PANEL,fg=TEXT,font=('DejaVu Sans',17,'bold')).pack(anchor='w')
            tk.Label(page,text=GUIDES[group],wraplength=980,justify='left',bg=PANEL,fg=MUTED).pack(anchor='w',pady=(8,14))
            for a in [a for a in ACTIONS if a.group==group]:
                row=tk.Frame(page,bg=PANEL); row.pack(fill='x',pady=5)
                ttk.Button(row,text=a.title,command=lambda a=a:self.review(a)).pack(side='left')
                problem=unavailable(a)
                label='Unavailable · '+problem if problem else ('Authentication required' if a.admin else 'Changes settings' if a.changes else 'Read only')
                tk.Label(row,text=label,bg=PANEL,fg=MUTED).pack(side='left',padx=14)
                self.rows.append((a,row))
        bottom=tk.Frame(self,bg=BG); bottom.pack(fill='both',padx=24,pady=14)
        tk.Label(bottom,text='LIVE OUTPUT · export retains the output tail; history keeps 20 tasks',bg=BG,fg=MUTED,font=('DejaVu Sans',9,'bold')).pack(anchor='w')
        box=tk.Frame(bottom,bg=BG); box.pack(fill='both',expand=True,pady=6)
        self.output=tk.Text(box,height=8,bg='#0b1220',fg=TEXT,insertbackground=TEXT,font=('DejaVu Sans Mono',10),relief='flat',wrap='word',state='disabled')
        scrollbar=ttk.Scrollbar(box,command=self.output.yview); scrollbar.pack(side='right',fill='y'); self.output.configure(yscrollcommand=scrollbar.set); self.output.pack(fill='both',expand=True)
        self.write('Welcome. No maintenance action runs at startup.\nPreview any tool, then run it when you are ready.\n')
        self.after(100,self.poll)
    def write(self,text):
        self.output.config(state='normal'); self.output.insert('end',text);
        size=int(self.output.count('1.0','end-1c','chars')[0])
        if size > 250000: self.output.delete('1.0',f'1.0+{size-250000}c')
        self.output.see('end'); self.output.config(state='disabled')
    def filter_tools(self,*_):
        for a,row in self.rows:
            row.pack_forget()
        for a,row in self.rows:
            if matches(a,self.search.get()): row.pack(fill='x',pady=5)
    def stream(self,chunk):
        try: self.live.put_nowait(chunk)
        except queue.Full: pass  # Report retains the output tail independently.
    def review(self,a):
        if self.busy: messagebox.showinfo('Task running','Wait for the current task to finish.'); return
        problem=unavailable(a)
        if problem: messagebox.showwarning(a.title,problem); return
        preview=shlex.join((('pkexec',) if a.admin else ())+a.command)
        if not messagebox.askokcancel(a.title,f'{a.description}\n\nCommand:\n{preview}\n\n'+('This action changes your system. Continue?' if a.changes else 'Run this diagnostic?')): return
        self.busy=True; self.status.set('Running · '+a.title); self.write('\n$ '+preview+'\n')
        def worker():
            try: result=execute(a.id,confirmed=True,on_output=self.stream)
            except Exception as e:
                self.stream(str(e)+'\n')
                result={'action':a.title,'command':list(a.command),'code':-1,'output':str(e)}
            self.events.put(result)
        threading.Thread(target=worker,daemon=True).start()
    def poll(self):
        try:
            # Limit work per tick so high-volume tools cannot starve Tk events.
            for _ in range(64): self.write(self.live.get_nowait())
        except queue.Empty: pass
        try:
            while True:
                result=self.events.get_nowait()
                # Worker finished; at most 64 queued chunks remain.
                while not self.live.empty(): self.write(self.live.get_nowait())
                self.results.append(result); self.results=self.results[-20:]; self.busy=False
                self.status.set(('Completed' if result['code']==0 else 'Needs attention')+' · '+result['action'])
                self.write('\nExit code: '+str(result['code'])+'\n')
                if result.get('truncated'): self.write('Report retains the latest 250,000 characters.\n')
        except queue.Empty: pass
        self.after(100,self.poll)
    def export(self):
        path=filedialog.asksaveasfilename(defaultextension='.json',initialfile='tux-steward-report.json',filetypes=[('JSON report','*.json')])
        if path:
            try:
                with open(path,'w') as f: json.dump({'version':__version__,'results':self.results},f,indent=2)
            except OSError as e: messagebox.showerror('Export failed',str(e))
    def close(self):
        if self.busy:
            messagebox.showwarning('Task running','Wait for the task to finish before closing.'); return
        self.destroy()

def main():
    if os.geteuid()==0:
        raise SystemExit('Run Tux Steward as your desktop user, without sudo. Individual admin tasks use PolicyKit.')
    try: App().mainloop()
    except tk.TclError as e: raise SystemExit(f'Desktop display unavailable: {e}')
