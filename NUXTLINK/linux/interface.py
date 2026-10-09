"""Tk layout for the Linux client; no serial or filesystem operations."""
import tkinter as tk
from tkinter import ttk
from link import BAUDS, PEERS


class Tip:
    def __init__(self, widget, text):
        self.widget, self.text, self.timer, self.popup = widget, text, None, None
        widget.bind('<Enter>', self.schedule, add='+')
        for event in ('<Leave>', '<ButtonPress>', '<Destroy>'):
            widget.bind(event, self.hide, add='+')

    def schedule(self, event=None):
        self.hide()
        self.timer = self.widget.after(650, self.show)

    def show(self):
        self.timer = None
        self.popup = tk.Toplevel(self.widget)
        self.popup.wm_overrideredirect(True)
        label = tk.Label(self.popup, text=self.text, justify='left', wraplength=350,
                         background='#fffbdc', foreground='#202b34', relief='solid',
                         borderwidth=1, padx=9, pady=6)
        label.pack()
        self.popup.update_idletasks()
        x = min(self.widget.winfo_rootx(), self.widget.winfo_screenwidth()-self.popup.winfo_reqwidth()-8)
        y = min(self.widget.winfo_rooty()+self.widget.winfo_height()+5,
                self.widget.winfo_screenheight()-self.popup.winfo_reqheight()-8)
        self.popup.geometry(f'+{max(0,x)}+{max(0,y)}')

    def hide(self, event=None):
        if self.timer:
            self.widget.after_cancel(self.timer)
            self.timer = None
        if self.popup:
            self.popup.destroy()
            self.popup = None


def build_interface(app, settings):
    a, root = app, app.root
    root.title('NuXT Link 3.1 — serial file manager')
    root.geometry(f'{min(1180, root.winfo_screenwidth()-40)}x{min(820, root.winfo_screenheight()-80)}')
    root.minsize(960, 700)
    root.protocol('WM_DELETE_WINDOW', a.close_window)
    style = ttk.Style(root)
    if 'clam' in style.theme_names(): style.theme_use('clam')
    style.configure('.', font=('sans-serif', 9), background='#f2f4f6', foreground='#243340')
    style.configure('TButton', padding=(6, 2), width=0)
    style.configure('TMenubutton', padding=(6, 2))
    style.configure('TEntry', padding=2)
    style.configure('TCombobox', padding=2)
    style.configure('Title.TLabel', font=('sans-serif', 16, 'bold'))
    style.configure('Hint.TLabel', foreground='#526474')
    style.configure('Good.TLabel', foreground='#216046')
    style.configure('Warning.TLabel', foreground='#955300')
    style.configure('Treeview', rowheight=23, background='#ffffff', fieldbackground='#ffffff')
    style.configure('Treeview.Heading', padding=(5, 2))
    style.map('Treeview', background=[('selected', '#235d86')], foreground=[('selected', '#ffffff')])
    style.configure('Accent.TButton', font=('sans-serif', 9, 'bold'))
    style.configure('Horizontal.TProgressbar', background='#347ca7', troughcolor='#e2e7eb')
    outer = ttk.Frame(root, padding=10)
    outer.pack(fill='both', expand=True)
    header = ttk.Frame(outer)
    header.pack(fill='x', pady=(0, 4))
    ttk.Label(header, text='NuXT Link', style='Title.TLabel').pack(side='left')
    ttk.Label(header, text='3.1  /  Serial file manager', style='Hint.TLabel').pack(side='left', padx=12)
    profiles = ttk.Frame(header)
    profiles.pack(side='right')
    ttk.Label(profiles, text='Profile').pack(side='left', padx=5)
    a.profile_name = tk.StringVar(value='NuXT')
    a.profile_box = ttk.Combobox(profiles, textvariable=a.profile_name, values=sorted(a.profiles.items), width=14)
    a.profile_box.pack(side='left')
    a.profile_box.bind('<<ComboboxSelected>>', lambda e: a.load_profile())
    for attr, text, command in [('load_button','Load',a.load_profile), ('save_button','Save',a.save_profile),
                                ('remove_profile_button','Remove',a.remove_profile)]:
        button = ttk.Button(profiles, text=text, width=6, command=command)
        button.pack(side='left', padx=(4,0))
        setattr(a, attr, button)
    Tip(a.save_button, 'Save the current connection settings and both folder paths in this profile.')

    conn = ttk.LabelFrame(outer, text='Connection', padding=6)
    conn.pack(fill='x')
    conn.columnconfigure(1, weight=1)
    for attr, key in [('device','device'), ('speed','baud'), ('flow','flow'), ('peer','peer'),
                      ('block_size','block'), ('latency','latency'), ('doscom','com')]:
        setattr(a, attr, tk.StringVar(value=str(settings[key])))
    ttk.Label(conn, text='Device').grid(row=0, column=0, padx=(0,6))
    a.port_box = ttk.Combobox(conn, textvariable=a.device, width=24)
    a.port_box.grid(row=0, column=1, sticky='ew')
    ttk.Label(conn, text='Baud').grid(row=0, column=2, padx=(10,4))
    a.speed_box = ttk.Combobox(conn, textvariable=a.speed, values=BAUDS, width=8, state='readonly')
    a.speed_box.grid(row=0, column=3)
    ttk.Label(conn, text='Flow').grid(row=0, column=4, padx=(10,4))
    a.flow_box = ttk.Combobox(conn, textvariable=a.flow, values=('none','rtscts'), width=7, state='readonly')
    a.flow_box.grid(row=0, column=5)
    Tip(a.flow_box, 'none: basic 3-wire cable.\nrtscts: hardware flow control with RTS/CTS wires connected.\nApply changes updates both ends while idle.')
    a.scan_button = ttk.Button(conn, text='Rescan', command=a.scan)
    a.scan_button.grid(row=0, column=6, padx=6)
    a.connect_button = ttk.Button(conn, text='Connect', style='Accent.TButton', command=a.connect)
    a.connect_button.grid(row=0, column=7)
    options = ttk.Frame(conn)
    options.grid(row=1, column=0, columnspan=8, sticky='ew', pady=(6,0))
    a.advanced_open = tk.BooleanVar(value=False)
    ttk.Checkbutton(options, text='Connection options', variable=a.advanced_open,
                    command=a.toggle_advanced).pack(side='left')
    a.connection_hint = tk.StringVar(value='Disconnected · Choose a device or saved profile')
    a.connection_label = ttk.Label(options, textvariable=a.connection_hint, style='Hint.TLabel', width=1)
    a.connection_label.pack(side='left', padx=10, fill='x', expand=True)
    a.diag_button = ttk.Button(options, text='Diagnostics', command=a.diagnostics)
    a.diag_button.pack(side='right')
    a.apply_button = ttk.Button(options, text='Apply changes', command=a.apply_connection)
    a.apply_button.pack(side='right', padx=5)
    a.advanced = ttk.Frame(conn)
    a.advanced.grid(row=2, column=0, columnspan=8, sticky='ew', pady=(8,0))
    for text, attr, var, values, width in [
            ('DOS port','com_box',a.doscom,('COM1','COM2','COM3','COM4'),6),
            ('Require peer','peer_box',a.peer,tuple(PEERS),6),
            ('Block bytes','block_box',a.block_size,(128,256,512),5),
            ('FTDI ms','latency_box',a.latency,(0,2,4,8,16),4)]:
        ttk.Label(a.advanced, text=text).pack(side='left', padx=(0,4))
        box = ttk.Combobox(a.advanced, textvariable=var, values=values, width=width,
                           state='normal' if attr=='latency_box' else 'readonly')
        box.pack(side='left', padx=(0,14))
        setattr(a, attr, box)
    a.ftdi_button = ttk.Button(a.advanced, text='Apply FTDI', command=a.apply_ftdi)
    a.ftdi_button.pack(side='left')
    Tip(a.peer_box, 'Optional readiness checks: DSR, DCD, or both.\nUse none for a basic cable. These checks are separate from RTS/CTS flow control.')
    Tip(a.com_box, 'The DOS port is a profile reminder. Change the physical COM port at the DOS server with its C key or command line.')
    Tip(a.block_box, 'Payload bytes per packet. Larger blocks reduce overhead. CRC and file verification remain enabled at every size.')
    Tip(a.latency_box, 'FTDI USB latency in milliseconds. 0 leaves it unchanged. Applied on connect, or with Apply FTDI. Previous value is restored on disconnect.')
    a.advanced.grid_remove()

    options = ttk.Frame(outer)
    options.pack(fill='x', pady=5)
    ttk.Label(options, text='Existing files').pack(side='left')
    a.policy = tk.StringVar(value='Skip')
    a.policy_box = ttk.Combobox(options, textvariable=a.policy, values=('Skip','Overwrite','Rename'), state='readonly', width=9)
    a.policy_box.pack(side='left', padx=6)
    a.incremental = tk.BooleanVar(value=True)
    a.incremental_check = ttk.Checkbutton(options, text='Verify & skip unchanged', variable=a.incremental)
    a.incremental_check.pack(side='left')
    Tip(a.incremental_check, 'Skip identical contents after verification. Turning this off does not disable packet CRC or verification of transferred files.')
    a.backup_button = ttk.Menubutton(options, text='Backups')
    a.backup_button.pack(side='right')
    menu = tk.Menu(a.backup_button, tearoff=False)
    menu.add_command(label='Back up entire DOS drive…', command=a.backup)
    menu.add_command(label='Restore backup into DOS folder…', command=a.restore)
    a.backup_button.configure(menu=menu)
    a.partial_button = ttk.Button(options, text='DOS partials…', command=a.inspect_partials)
    a.partial_button.pack(side='right', padx=5)
    a.report_button = ttk.Button(options, text='Open report', command=a.open_report)
    a.report_button.pack(side='right')

    # Pack the footer first so it stays visible when the window is made smaller.
    footer = ttk.Frame(outer)
    footer.pack(side='bottom', fill='x', pady=(8,0))
    footer.columnconfigure(1, weight=1)
    a.status = tk.StringVar(value='Choose a profile or device, then connect to the DOS server.')
    a.detail = tk.StringVar(value='CRC and whole-file verification are always enabled.')
    a.overall = tk.StringVar(value='Queue empty')
    a.status_label = ttk.Label(footer, textvariable=a.status)
    a.status_label.grid(row=0, column=0, columnspan=3, sticky='w')
    a.detail_label = ttk.Label(footer, textvariable=a.detail, style='Hint.TLabel')
    a.detail_label.grid(row=1, column=0, columnspan=3, sticky='w', pady=(2,4))
    ttk.Label(footer, text='File', style='Hint.TLabel').grid(row=2, column=0, sticky='w', padx=(0,8))
    a.progress = ttk.Progressbar(footer, maximum=100)
    a.progress.grid(row=2, column=1, sticky='ew', pady=2)
    a.file_percent = tk.StringVar(value='—')
    ttk.Label(footer, textvariable=a.file_percent, width=5, anchor='e').grid(row=2, column=2)
    ttk.Label(footer, text='Queue', style='Hint.TLabel').grid(row=3, column=0, sticky='w', padx=(0,8))
    a.queue_progress = ttk.Progressbar(footer, maximum=100)
    a.queue_progress.grid(row=3, column=1, sticky='ew', pady=2)
    a.queue_percent = tk.StringVar(value='—')
    ttk.Label(footer, textvariable=a.queue_percent, width=5, anchor='e').grid(row=3, column=2)
    ttk.Label(footer, textvariable=a.overall, style='Hint.TLabel').grid(row=4, column=0, columnspan=2, sticky='w', pady=(4,0))
    a.show_log = tk.BooleanVar(value=False)
    ttk.Checkbutton(footer, text='Activity log', variable=a.show_log, command=a.toggle_log).grid(row=4, column=2, sticky='e')
    footer.bind('<Configure>', lambda e: (a.status_label.configure(wraplength=max(300,e.width-5)),
                                         a.detail_label.configure(wraplength=max(300,e.width-5))))

    a.panes = ttk.Panedwindow(outer, orient='vertical')
    a.panes.pack(fill='both', expand=True)
    a.browsers = ttk.Panedwindow(a.panes, orient='horizontal')
    a.panes.add(a.browsers, weight=3)
    local = ttk.LabelFrame(a.browsers, text='Local files', padding=6)
    remote = ttk.LabelFrame(a.browsers, text='DOS files', padding=6)
    a.browsers.add(local, weight=1)
    a.browsers.add(remote, weight=1)
    a.remote_frame = remote
    a.disk_space = tk.StringVar(value='')
    for side, frame in [('local',local), ('remote',remote)]:
        is_local = side=='local'
        nav = ttk.Frame(frame)
        nav.pack(fill='x', pady=(0,3))
        up = ttk.Button(nav, text='↑', width=2,
                        command=(lambda: a.local_navigate(a.local.parent)) if is_local else a.up)
        up.pack(side='left')
        location = tk.StringVar(value=settings['local'] if is_local else a.remote)
        box = ttk.Entry(nav, textvariable=location)
        box.pack(side='left', fill='x', expand=True, padx=4)
        box.bind('<Return>', lambda e, local=is_local: a.local_navigate() if local else a.navigate())
        Tip(box, 'Type a folder path and press Enter. Ctrl+L focuses this field from the file list.')
        go = ttk.Button(nav, text='Filter', width=5, command=lambda side=side: a.toggle_filter(side))
        go.pack(side='left')
        refresh = ttk.Button(nav, text='…' if is_local else '↻', width=2,
                             command=a.choose_local if is_local else a.refresh)
        refresh.pack(side='left', padx=(4,0))
        Tip(refresh, 'Choose a local folder' if is_local else 'Refresh DOS folder (F5)')
        if is_local: a.local_location, a.local_box = location, box
        else:
            a.location, a.path_box = location, box
            a.up_button, a.go_button, a.refresh_button = up, go, refresh
        search = ttk.Frame(frame)
        ttk.Label(search, text='Filter', style='Hint.TLabel').pack(side='left')
        query = tk.StringVar()
        entry = ttk.Entry(search, textvariable=query)
        entry.pack(side='left', fill='x', expand=True, padx=5)
        ttk.Button(search, text='×', width=2, command=lambda v=query: v.set('')).pack(side='left')
        Tip(entry, 'Filter names in this folder (not subfolders). Hidden/system files remain included. Clear the filter with × or Escape.')
        entry.bind('<Escape>', lambda e,v=query: v.set(''))
        setattr(a, side+'_filter', query)
        setattr(a, side+'_search', (search,nav,entry,go))
        buttons = ttk.Frame(frame)
        buttons.pack(side='bottom', fill='x', pady=(3,0))
        definitions = ([('send_button','Upload →',a.upload_selection), ('folder_button','Choose files…',a.upload_files),
                        ('verify_button','Verify backup…',a.verify)] if is_local else
                       [('get_button','← Download',a.download), ('mkdir_button','New folder',a.mkdir_remote),
                        ('rename_button','Rename',a.rename_remote), ('delete_button','Delete…',a.delete_remote)])
        for attr, label, command in definitions:
            button = ttk.Button(buttons, text=label, command=command)
            button.pack(side='left', padx=(0,4))
            setattr(a, attr, button)
        summary = tk.StringVar(value='')
        ttk.Label(frame, textvariable=summary, style='Hint.TLabel').pack(side='bottom', anchor='w', pady=(2,0))
        setattr(a, side+'_summary', summary)
    a.local_tree = a.table(local, ('name','size','date'), ('Name','Bytes','Modified'), (175,90,133))
    a.tree = a.table(remote, ('name','size','date','attr'), ('Name','Bytes','DOS modified','Attr'), (145,90,160,48))
    for tree, box, callback, up, refresh in [
            (a.local_tree,a.local_box,a.local_open,lambda: a.local_navigate(a.local.parent),a.local_navigate),
            (a.tree,a.path_box,a.open_selected,a.up,a.refresh)]:
        tree.bind('<Double-1>', callback)
        tree.bind('<Return>', callback)
        tree.bind('<<TreeviewSelect>>', lambda e: a.selection_changed())
        tree.bind('<Control-a>', lambda e,t=tree: (t.selection_set(t.get_children()),'break')[-1])
        tree.bind('<Control-l>', lambda e,b=box: (b.focus_set(),b.selection_range(0,'end'),'break')[-1])
        tree.bind('<Alt-Up>', lambda e,c=up: (c(),'break')[-1])
        tree.bind('<F5>', lambda e,c=refresh: (c(),'break')[-1])
    a.local_filter.trace_add('write', lambda *args: a.render_local())
    a.remote_filter.trace_add('write', lambda *args: a.render_remote())
    a.tree.bind('<Delete>', lambda e: a.delete_remote())
    a.tree.bind('<F2>', lambda e: a.rename_remote())
    a.remote_menu = tk.Menu(root, tearoff=False)
    for label, command in [('Download',a.download), ('New folder',a.mkdir_remote), ('Rename / move',a.rename_remote),
                            ('Delete…',a.delete_remote), ('Refresh',a.refresh)]:
        a.remote_menu.add_command(label=label, command=command)
    a.tree.bind('<Button-3>', a.remote_context)
    a.local_menu = tk.Menu(root, tearoff=False)
    a.local_menu.add_command(label='Upload selection', command=a.upload_selection)
    a.local_menu.add_command(label='Refresh', command=a.local_navigate)
    a.local_tree.bind('<Button-3>', a.local_context)

    queued = ttk.LabelFrame(a.panes, text='Transfer queue — preview before starting', padding=6)
    a.panes.add(queued, weight=2)
    actions = ttk.Frame(queued)
    actions.pack(fill='x', pady=(0,5))
    for attr, label, command in [('start_button','Start / resume',a.start_queue), ('retry_button','Retry failed',a.retry_queue),
                                ('cancel_button','Cancel queue',a.cancel), ('clear_button','Clear queue',a.clear_queue)]:
        button = ttk.Button(actions, text=label, command=command, style='Accent.TButton' if attr=='start_button' else 'TButton')
        button.pack(side='left', padx=(0,5))
        setattr(a, attr, button)
    Tip(a.clear_button, 'Remove the queue preview. Completed files and resumable partial files are kept.')
    Tip(a.cancel_button, 'Stop after the current verified operation. Keep completed work and partial files for resume.')
    a.queue_filter = tk.StringVar(value='All')
    filters = ttk.Combobox(actions, textvariable=a.queue_filter, values=('All','Pending','Failed','Completed'), state='readonly', width=10)
    filters.pack(side='right')
    filters.bind('<<ComboboxSelected>>', lambda e: a.fill_plan())
    ttk.Label(actions, text='Show', style='Hint.TLabel').pack(side='right', padx=5)
    a.summary = tk.StringVar(value='Queue — select files, then Upload or Download to preview')
    queue_title = ttk.Label(queued, textvariable=a.summary)
    queued.configure(labelwidget=queue_title)
    Tip(queue_title, 'Preview the queue before Start / resume. Double-click a job to see full paths, DOS metadata and any error or skip reason.')
    a.queue_tree = a.table(queued, ('status','action','source','path','size'),
                           ('Status','Action','Source','Destination / issue','Bytes'), (100,85,330,330,90))
    a.queue_tree.bind('<Double-1>', a.show_job)
    a.queue_tree.bind('<Return>', a.show_job)
    for tag, color in [('complete','#236146'), ('error','#a32833'), ('running','#155a89'), ('muted','#64717b')]:
        a.queue_tree.tag_configure(tag, foreground=color)
    a.log_frame = ttk.LabelFrame(a.panes, text='Activity log', padding=4)
    a.log = tk.Text(a.log_frame, height=5, wrap='word', font=('monospace',9), state='disabled')
    scroll = ttk.Scrollbar(a.log_frame, command=a.log.yview)
    scroll.pack(side='right', fill='y')
    a.log.configure(yscrollcommand=scroll.set)
    a.log.pack(fill='both', expand=True)
    for var in (a.speed,a.flow,a.peer,a.block_size):
        var.trace_add('write', lambda *args: a.connection_state())
