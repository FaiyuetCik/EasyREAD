"""EasyREAD Windows setup UI. Original code: MIT, copyright 2026 FaiyuetCik.

The separately packaged GPL esptool executable is invoked as a subprocess.
"""
import hashlib
import json
import os
from pathlib import Path
import queue
import re
import subprocess
import sys
import threading
import tkinter as tk
from tkinter import ttk, messagebox
from serial.tools import list_ports

ROOT = Path(sys.executable).resolve().parent if getattr(sys, 'frozen', False) else Path(__file__).resolve().parent
TEXT = {
 'zh': {'title':'EasyREAD 安装助手', 'subtitle':'reTerminal Sticky · 固件 2026-10-10.1 · Windows x64',
 'port':'连接设备', 'refresh':'刷新', 'mode':'安装方式', 'first':'首次安装（重置内部进度和设置）',
 'upgrade':'保留设置升级（先检查已有固件布局）', 'board':'我连接的是 Seeed Studio reTerminal Sticky',
 'install':'安装 EasyREAD', 'ready':'连接 USB 数据线，选择设备后点击安装。', 'none':'未发现串口：检查数据线，关闭占用串口的软件，再刷新。',
 'select':'请选择设备，并确认设备型号。', 'confirm':'首次安装将重置设备内部阅读进度和设置，SD 卡文件不受影响。继续？',
 'busy':'安装中，请勿断开 USB 或关闭窗口。', 'done':'安装成功。插入已准备的 SD 卡，开机开始阅读。',
 'failed':'安装未完成。检查日志；重新连接或按设备说明进入下载模式后重试。布局不匹配时，请备份设置并选择首次安装。',
 'close':'安装正在进行，请等待完成后关闭。', 'card':'准备 SD 卡：把本包 SD-Card 内的 Fonts、Books 复制到 FAT32 卡根目录。',
 'log':'打开日志', 'checking':'正在校验安装文件…', 'layout':'正在核对已有 bootloader 和分区…', 'writing':'正在写入并校验固件…',
 'broken':'安装文件缺失或损坏，请重新下载并完整解压入门包。'},
 'en': {'title':'EasyREAD Setup', 'subtitle':'reTerminal Sticky · Firmware 2026-10-10.1 · Windows x64',
 'port':'Connected device', 'refresh':'Refresh', 'mode':'Installation mode', 'first':'First install (resets internal settings and positions)',
 'upgrade':'Keep settings (checks existing firmware layout first)', 'board':'My device is a Seeed Studio reTerminal Sticky',
 'install':'Install EasyREAD', 'ready':'Connect a USB data cable, select the device, then install.',
 'none':'No serial ports found. Check the data cable, close serial apps, and refresh.', 'select':'Select a device and confirm its model.',
 'confirm':'First install resets internal reading positions and settings. SD card files are unchanged. Continue?',
 'busy':'Installing. Keep USB connected and leave this window open.', 'done':'Installation complete. Insert the prepared SD card and start reading.',
 'failed':'Installation failed. Check the log; reconnect or enter download mode as described by the device guide. If layout differs, back up settings and use First install.',
 'close':'Please wait for installation to finish before closing.', 'card':'SD card: copy Fonts and Books from SD-Card to the root of a FAT32 card.',
 'log':'Open log', 'checking':'Checking installation files…', 'layout':'Checking existing bootloader and partitions…',
 'writing':'Writing and verifying firmware…', 'broken':'Installation files are missing or damaged. Download and fully extract the starter ZIP again.'}
}

def check_payload():
    manifest = json.loads((ROOT / 'payload.json').read_text(encoding='utf-8'))
    for name, expected in manifest['files'].items():
        path = (ROOT / name).resolve()
        if not path.is_relative_to(ROOT) or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError('File verification failed: ' + name)
    return manifest

def command(port, action, args):
    return [str(ROOT / 'tools' / 'esptool' / 'esptool.exe'), '--chip', 'esp32s3',
            '--port', port, '--baud', '460800', action, *args]

def flash_steps(port, upgrade):
    binary = ROOT / 'firmware'
    if upgrade:
        yield 'layout', command(port, 'verify-flash', ['0x0', str(binary/'bootloader.bin'),
            '0x8000', str(binary/'partitions.bin'), '0xe000', str(binary/'boot_app0.bin')])
    yield 'writing', command(port, 'write-flash',
        ['0x10000', str(binary/'firmware.bin')] if upgrade else ['0x0', str(binary/'full.bin')])

class Setup:
    def __init__(self):
        self.ui = tk.Tk()
        self.ui.geometry('820x680')
        self.ui.minsize(760, 620)
        self.lang = tk.StringVar(value='zh')
        self.mode = tk.StringVar(value='first')
        self.confirmed = tk.BooleanVar()
        self.events = queue.Queue()
        self.busy = False
        self.ports = []
        self.widgets = []
        logdir = Path(os.environ.get('LOCALAPPDATA', str(Path.home()))) / 'EasyREAD' / 'logs'
        logdir.mkdir(parents=True, exist_ok=True)
        from datetime import datetime
        self.logfile = logdir / (datetime.now().strftime('setup-%Y%m%d-%H%M%S') + '.log')
        frame = ttk.Frame(self.ui, padding=20)
        frame.pack(fill='both', expand=True)
        header = ttk.Frame(frame); header.pack(fill='x')
        self.label(header, 'title', font=('Segoe UI',20,'bold')).pack(side='left')
        languages = ttk.Combobox(header, values=['简体中文', 'English'], state='readonly', width=12)
        languages.current(0); languages.pack(side='right')
        languages.bind('<<ComboboxSelected>>', lambda e: self.change_language(languages.current()))
        self.label(frame, 'subtitle').pack(anchor='w', pady=(8,16))
        self.label(frame, 'card', wraplength=760).pack(anchor='w', pady=(0,16))
        row = ttk.LabelFrame(frame); row.pack(fill='x', pady=4); self.widgets.append((row,'port'))
        self.portbox = ttk.Combobox(row, state='readonly', width=74)
        self.portbox.pack(side='left', fill='x', expand=True, padx=8, pady=10)
        self.refresh_button = ttk.Button(row, command=self.refresh)
        self.widgets.append((self.refresh_button,'refresh')); self.refresh_button.pack(side='right', padx=8)
        modes = ttk.LabelFrame(frame); modes.pack(fill='x', pady=10); self.widgets.append((modes,'mode'))
        for value, key in [('first','first'),('upgrade','upgrade')]:
            button = ttk.Radiobutton(modes, variable=self.mode, value=value)
            button.pack(anchor='w', padx=10, pady=7); self.widgets.append((button,key))
        self.board = ttk.Checkbutton(frame, variable=self.confirmed)
        self.board.pack(anchor='w', pady=8); self.widgets.append((self.board,'board'))
        self.install = ttk.Button(frame, command=self.start)
        self.install.pack(fill='x', pady=8); self.widgets.append((self.install,'install'))
        self.progress = ttk.Progressbar(frame, mode='determinate', maximum=100)
        self.progress.pack(fill='x', pady=6)
        self.status = ttk.Label(frame, wraplength=760); self.status.pack(anchor='w', pady=8)
        logs = ttk.Frame(frame); logs.pack(fill='both', expand=True)
        self.output = tk.Text(logs, height=9, state='disabled', wrap='word', font=('Consolas',9))
        scroll = ttk.Scrollbar(logs, command=self.output.yview)
        self.output.configure(yscrollcommand=scroll.set)
        scroll.pack(side='right',fill='y'); self.output.pack(fill='both',expand=True)
        openlog = ttk.Button(frame, command=lambda: os.startfile(str(self.logfile)))
        openlog.pack(anchor='e', pady=6); self.widgets.append((openlog,'log'))
        self.logfile.touch()
        self.change_language(0)
        self.refresh()
        self.ui.protocol('WM_DELETE_WINDOW', self.close)
        self.ui.after(100, self.poll)

    def label(self, parent, key, **kwargs):
        widget = ttk.Label(parent, **kwargs); self.widgets.append((widget,key)); return widget

    def t(self, key): return TEXT[self.lang.get()][key]

    def set_status(self, key):
        self.status_key = key; self.status.configure(text=self.t(key))

    def change_language(self, index):
        self.lang.set('en' if index else 'zh')
        self.ui.title(self.t('title'))
        for widget, key in self.widgets: widget.configure(text=self.t(key))
        self.set_status(getattr(self,'status_key','ready'))

    def refresh(self):
        if self.busy: return
        self.ports = sorted(list_ports.comports(), key=lambda p: (p.vid != 0x303a,p.device))
        self.portbox['values'] = [f'{p.device} — {p.description}' for p in self.ports]
        self.portbox.set('')
        candidates = [i for i,p in enumerate(self.ports) if p.vid == 0x303a]
        if len(candidates) == 1: self.portbox.current(candidates[0])
        elif len(self.ports) == 1: self.portbox.current(0)
        self.set_status('ready' if self.ports else 'none')

    def start(self):
        if self.busy: return
        index = self.portbox.current()
        if index < 0 or not self.confirmed.get():
            messagebox.showinfo(self.t('title'), self.t('select')); return
        upgrade = self.mode.get() == 'upgrade'
        if not upgrade and not messagebox.askokcancel(self.t('title'), self.t('confirm')): return
        port = self.ports[index].device
        self.busy = True
        self.install.configure(state='disabled'); self.refresh_button.configure(state='disabled')
        self.portbox.configure(state='disabled')
        self.progress['value'] = 0
        self.set_status('checking')
        threading.Thread(target=self.worker, args=(port,upgrade), daemon=True).start()

    def worker(self, port, upgrade):
        try:
            check_payload()
            for key, args in flash_steps(port, upgrade):
                self.events.put(('status', key))
                self.events.put(('line', f'\n{key}: {port}\n'))
                env = os.environ.copy(); env['PYTHONIOENCODING']='utf-8'; env['NO_COLOR']='1'; env['TERM']='dumb'
                with subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                        stdin=subprocess.DEVNULL, encoding='utf-8', errors='replace',
                        creationflags=subprocess.CREATE_NO_WINDOW, env=env, cwd=ROOT) as child:
                    for line in child.stdout:
                        self.events.put(('line', line))
                    if child.wait() != 0: raise RuntimeError(f'{key} failed (exit {child.returncode})')
            self.events.put(('finish', 'done'))
        except Exception as exc:
            self.events.put(('line', '\n' + str(exc) + '\n'))
            self.events.put(('finish', 'failed'))

    def poll(self):
        while not self.events.empty():
            kind, value = self.events.get()
            if kind == 'line':
                value = re.sub(r'\x1b\[[0-9;]*[A-Za-z]', '', value)
                with self.logfile.open('a', encoding='utf-8') as stream: stream.write(value)
                self.output.configure(state='normal'); self.output.insert('end',value)
                self.output.see('end'); self.output.configure(state='disabled')
                match = re.search(r'\((\d+(?:\.\d+)?)\s*%\)', value)
                if match: self.progress['value']=float(match[1])
            elif kind == 'status': self.set_status(value)
            elif kind == 'finish':
                self.busy = False; self.set_status(value)
                self.install.configure(state='normal'); self.refresh_button.configure(state='normal')
                self.portbox.configure(state='readonly')
                if value == 'done': self.progress['value']=100
                (messagebox.showinfo if value == 'done' else messagebox.showerror)(self.t('title'),self.t(value))
        self.ui.after(100,self.poll)

    def close(self):
        if self.busy: messagebox.showinfo(self.t('title'),self.t('close'))
        else: self.ui.destroy()

if __name__ == '__main__':
    if '--self-check' in sys.argv:
        result = {'payload':check_payload(), 'ports':[p.device for p in list_ports.comports()],
                  'writes_device':False}
        (ROOT/'self-check.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    else:
        Setup().ui.mainloop()
