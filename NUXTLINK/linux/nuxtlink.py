#!/usr/bin/env python3
"""NuXT Link 3: full-drive browser and reviewable serial transfer queues."""
import os
import stat
import argparse
import queue
import subprocess
import threading
import time
from pathlib import Path
import tkinter as tk
from tkinter import ttk, filedialog, messagebox, simpledialog
from link import BAUDS, PEERS, SerialPort, Client, Cancelled, LinkError, VersionError, ports, dos_path, join_path
from transfers import plan_download, plan_upload, execute, verify_backup
from operations import plan_delete, execute_delete
from profiles import Profiles, DEFAULTS, validate
from ftdi import LatencySession, read_latency
from interface import build_interface


class App:
    def __init__(self, root, settings=None, profile_path=None):
        self.root, self.client, self.plan = root, None, None
        self.busy = self.closing = False
        self.can_cancel = True
        self.events = queue.Queue()
        self.cancel_event = threading.Event()
        self.entries, self.local_entries, self.rows = {}, {}, {}
        self.remote = '\\'
        self.started, self.progress_base, self.progress_file = 0, 0, None
        self.active_progress=None
        self.queue_started, self.queue_base = time.monotonic(), 0
        self.report, self.latency_session = None, None
        self.info = {}
        self.profiles = Profiles(profile_path)
        self.state_dir = self.profiles.path.parent
        settings = {**DEFAULTS, **(settings or {})}
        self.remote=dos_path(settings['remote'])
        self.local_items, self.remote_items = [], []
        build_interface(self, settings)
        self.scan();self.local=Path(settings['local']).expanduser().absolute();self.local_navigate(self.local)
        self.update_controls();root.after(100,self.arrange_panes);root.after(60,self.poll)

    def arrange_panes(self):
        self.panes.update_idletasks()
        height=self.panes.winfo_height()
        if self.show_log.get():
            self.panes.sashpos(0,int(height*.52))
            self.panes.sashpos(1,int(height*.80))
        else:
            self.panes.sashpos(0,int(height*.56))

    def toggle_advanced(self):
        if self.advanced_open.get():self.advanced.grid()
        else:self.advanced.grid_remove()
        self.root.after_idle(self.arrange_panes)

    def toggle_filter(self,side):
        frame,nav,entry,button=getattr(self,side+'_search')
        if frame.winfo_manager():
            getattr(self,side+'_filter').set('');frame.pack_forget();button.configure(text='Filter')
        else:
            frame.pack(fill='x',pady=(0,3),after=nav);entry.focus_set();button.configure(text='Hide')

    def table(self,parent,columns,headings,widths):
        frame=ttk.Frame(parent)
        frame.pack(fill='both',expand=True)
        tree=ttk.Treeview(frame,columns=columns,show='headings',selectmode='extended',height=5)
        tree._labels=dict(zip(columns,headings));tree._directories=set()
        for col,label,width in zip(columns,headings,widths):
            tree.heading(col,text=label,command=lambda c=col,t=tree:self.sort_table(t,c))
            tree.column(col,width=width,minwidth=75 if col in ('name','source','path') else width,
                        stretch=col in ('name','source','path'),anchor='e' if col=='size' else 'w')
        vertical=ttk.Scrollbar(frame,orient='vertical',command=tree.yview)
        horizontal=ttk.Scrollbar(frame,orient='horizontal',command=tree.xview)
        tree.configure(yscrollcommand=vertical.set,xscrollcommand=horizontal.set)
        tree.grid(row=0,column=0,sticky='nsew')
        vertical.grid(row=0,column=1,sticky='ns')
        horizontal.grid(row=1,column=0,sticky='ew')
        frame.rowconfigure(0,weight=1);frame.columnconfigure(0,weight=1)
        return tree

    def scan(self):
        found=ports();self.port_box.configure(values=found)
        if not self.device.get() and found:self.device.set(found[0])

    def sort_table(self,tree,column,reverse=None):
        rows=list(tree.get_children())
        def key(row):
            value=tree.set(row,column)
            if column=='size':
                try:return int(value.replace(',',''))
                except ValueError:return -1
            return value.casefold()
        if reverse is None:reverse=getattr(tree,'_sort',None)==(column,False)
        ordered=sorted(rows,key=key,reverse=reverse)
        ordered.sort(key=lambda row:row not in tree._directories)
        for i,row in enumerate(ordered):tree.move(row,'',i)
        tree._sort=(column,reverse)
        for col,label in tree._labels.items():
            tree.heading(col,text=label+(' ▼' if reverse else ' ▲') if col==column else label)

    def selection_changed(self):
        for tree,total,var in ((self.local_tree,len(self.local_items),self.local_summary),
                               (self.tree,len(self.remote_items),self.remote_summary)):
            selected=tree.selection();size=0;folders=0
            for row in selected:
                value=tree.set(row,'size')
                if value=='<DIR>':folders+=1
                else:size+=int(value.replace(',',''))
            text=f'{len(tree.get_children())} / {total} entries'
            if selected:
                text+=f' · {len(selected)} selected · {size:,} file bytes'
                if folders:text+=f' + {folders} folders'
            var.set(text)
        self.update_controls()

    def connection_state(self):
        if not self.client:
            self.connection_hint.set('Disconnected · Choose a device or saved profile')
            self.connection_label.configure(style='Hint.TLabel')
            self.apply_button.configure(state='disabled')
            return
        d=self.info
        if not d:return
        pending=(str(d['baud']),d['flow'],d['peer'],str(d['block_size'])) != (
            self.speed.get(),self.flow.get(),self.peer.get(),self.block_size.get())
        errors=sum(d[key] for key in ('crc_errors','overruns','parity_errors','framing_errors','breaks','flow_timeouts'))
        if pending:
            text='Settings edited · Apply changes to update both ends'
        else:
            flow='RTS/CTS' if d['flow']=='rtscts' else '3-wire'
            text=f"Connected · COM{d['com']} · {d['baud']} · {flow} · {d['block_size']} bytes"
            if errors:text+=f' · {errors} errors / timeouts'
        self.connection_hint.set(text)
        self.connection_label.configure(style='Warning.TLabel' if pending or errors else 'Good.TLabel')
        self.apply_button.configure(state='normal' if pending and not self.busy else 'disabled')

    def update_controls(self):
        connected=self.client is not None
        for w in (self.send_button,self.folder_button,self.get_button,self.backup_button,
                  self.up_button,self.go_button,self.refresh_button,self.path_box,self.partial_button,
                  self.mkdir_button,self.rename_button,self.delete_button,self.apply_button,self.diag_button):
            w.configure(state='normal' if connected and not self.busy else 'disabled')
        for w in (self.verify_button,self.save_button,self.remove_profile_button,self.ftdi_button):w.configure(state='disabled' if self.busy else 'normal')
        self.connect_button.configure(text='Disconnect' if connected else 'Connect',state='disabled' if self.busy else 'normal')
        self.port_box.configure(state='disabled' if connected or self.busy else 'normal')
        self.profile_box.configure(state='disabled' if connected or self.busy else 'normal')
        self.load_button.configure(state='disabled' if connected or self.busy else 'normal')
        self.com_box.configure(state='disabled' if connected or self.busy else 'readonly')
        for w in (self.speed_box,self.flow_box,self.peer_box,self.block_box):w.configure(state='disabled' if self.busy else 'readonly')
        self.latency_box.configure(state='disabled' if self.busy else 'normal')
        self.scan_button.configure(state='disabled' if connected or self.busy else 'normal')
        self.cancel_button.configure(state='normal' if self.busy and self.can_cancel else 'disabled')
        self.clear_button.configure(state='normal' if self.plan and not self.busy else 'disabled')
        self.start_button.configure(state='normal' if connected and self.plan and not self.busy and self.plan.status!='Complete' else 'disabled')
        self.retry_button.configure(state='normal' if connected and self.plan and not self.busy and any(j.status in ('Failed','Cancelled') for j in self.plan.jobs) else 'disabled')
        self.report_button.configure(state='normal' if self.report else 'disabled')
        self.policy_box.configure(state='disabled' if self.busy else 'readonly')
        self.incremental_check.configure(state='disabled' if self.busy else 'normal')
        ready=connected and not self.busy
        selected=[self.entries[i] for i in self.tree.selection() if i in self.entries]
        usable=bool(selected) and not any(e.internal for e in selected)
        for widget,enabled in ((self.send_button,bool(self.local_tree.selection())),
                               (self.get_button,usable),(self.delete_button,usable),
                               (self.rename_button,usable and len(selected)==1)):
            widget.configure(state='normal' if ready and enabled else 'disabled')
        for index,enabled in ((0,usable),(1,True),(2,usable and len(selected)==1),(3,usable),(4,True)):
            self.remote_menu.entryconfigure(index,state='normal' if ready and enabled else 'disabled')
        self.local_menu.entryconfigure(0,state='normal' if ready and self.local_tree.selection() else 'disabled')
        self.connection_state()

    def work(self,title,action,cancellable=True):
        if self.busy:return
        self.busy=True;self.can_cancel=cancellable;self.started=time.monotonic();self.progress_file=None
        self.progress.configure(mode='indeterminate',value=0);self.progress.start(20)
        self.file_percent.set('…');self.detail.set('Waiting for the operation to finish…')
        self.status.set(title);self.log_message(title)
        self.cancel_event.clear()
        if self.client:self.client.cancelled.clear()
        self.update_controls()
        def worker():
            try:self.events.put(('done',action()))
            except Exception as exc:self.events.put(('error',exc))
        threading.Thread(target=worker,daemon=True).start()

    def notify(self,message):
        if self.cancel_event.is_set():raise Cancelled('Operation cancelled; completed work and resumable data retained.')
        if self.events.qsize()<20:self.events.put(('note',message))

    def connect(self):
        if self.busy:return
        if self.client:self.disconnect();return
        try:v=self.profile_values()
        except ValueError as exc:messagebox.showerror('Settings',str(exc));return
        if not v['device']:messagebox.showinfo('Serial device','Choose a serial device.');return
        remote=self.remote
        def action():
            # Connect at the selected speed first, then the DOS bootstrap default.
            # Begin without local gating so we can negotiate RTS/CTS on both ends.
            port=SerialPort(v['device'],v['baud']);client=Client(port,v['baud']);client.cancelled=self.cancel_event
            try:
                client.attempts=1
                try:hello=client.connect()
                except VersionError:
                    raise
                except LinkError:
                    if self.cancel_event.is_set():raise Cancelled('Connection cancelled')
                    if v['baud']==38400:raise
                    port.configure(38400);client.baud=38400;client.parser.buffer.clear();hello=client.connect()
                client.attempts=5
                info=client.apply_settings(v['baud'],v['flow'],v['peer'],v['block'])
                latency=self.latency_session
                if latency and latency.device!=v['device']:latency.restore(authorize=True);latency=None
                if latency is None:latency=LatencySession(v['device'])
                self.latency_session=latency
                if v['latency']:
                    try:
                        if latency.apply(v['latency']):self.notify(f"FTDI latency {v['latency']} ms")
                    except OSError:self.notify('Use Apply FTDI latency to authorize the adapter setting.')
                entries=client.listing(remote)
                return ('connected',client,hello,entries,info,remote)
            except BaseException:
                port.close()
                if self.latency_session:
                    try:self.latency_session.restore()
                    except OSError:self.notify('FTDI latency still changed; close the client to restore it with authorization.')
                raise
        self.work('Connecting and applying the selected DOS/Linux settings…',action)

    def navigate(self,path=None):
        if self.busy or not self.client:return
        try:
            path=path or self.location.get()
            if path[:2].upper()==self.client.drive[:2]:path=path[2:]
            path=dos_path(path)
        except ValueError as exc:messagebox.showerror('DOS path',str(exc));return
        self.work('Reading '+path,lambda:('listing',path,self.client.listing(path),self.client.diagnostics()))

    def refresh(self):self.navigate(self.remote)
    def up(self):self.navigate(self.remote.rsplit('\\',1)[0] or '\\')
    def open_selected(self,event=None):
        selected=self.tree.selection()
        if self.busy or len(selected)!=1:return
        entry=self.entries[selected[0]]
        if entry.internal:return
        if entry.meta.is_dir:self.navigate(self.remote.rstrip('\\')+'\\'+entry.name)
        else:self.download()

    def options(self):return dict(policy=self.policy.get().lower(),incremental=self.incremental.get(),notify=self.notify)
    def prepare_upload(self,sources,**extra):
        if self.busy or not self.client:return
        root,options=self.remote,self.options()
        self.work('Building transfer preview…',lambda:('prepared',plan_upload(self.client,sources,root,**options,**extra)))
    def upload_files(self):
        sources=filedialog.askopenfilenames(title='Files to send to the DOS folder')
        if sources:self.prepare_upload(sources)
    def upload_folder(self):
        source=filedialog.askdirectory(title='Folder to send, including all subfolders')
        if source:self.prepare_upload([source])
    def download(self):
        if self.busy or not self.client:return
        names=[self.entries[i].name for i in self.tree.selection() if not self.entries[i].internal]
        if not names:messagebox.showinfo('Select entries','Select files or folders from the DOS browser.');return
        root,options,dest=self.remote,self.options(),str(self.local)
        self.work('Scanning selection…',lambda:('prepared',plan_download(self.client,root,dest,names=names,**options)))
    def backup(self):
        dest=filedialog.askdirectory(title='Choose a dedicated folder for this DOS drive backup')
        if not dest:return
        incremental=self.incremental.get()
        self.work('Scanning the entire DOS drive, including hidden/system files…',lambda:('prepared',
            plan_download(self.client,'\\',dest,policy='overwrite',incremental=incremental,backup=True,notify=self.notify)))
    def restore(self):
        source=filedialog.askdirectory(title='NuXT Link backup to restore into the current DOS folder')
        if source:self.prepare_upload([source],contents=True,restore=True)
    def verify(self):
        source=filedialog.askdirectory(title='NuXT Link backup to verify against its manifest')
        if source:self.work('Verifying local backup…',lambda:('verified',*verify_backup(source,self.notify)))

    def inspect_partials(self):
        self.work('Inspecting DOS resume cache…',lambda:('partials',self.client.partials()))

    def start_queue(self):
        if not self.plan or not self.client or self.busy:return
        if self.plan.drive!=self.client.drive:
            messagebox.showerror('Different DOS drive','This queue was prepared for another drive. Build a new preview.');return
        plan=self.plan;self.queue_started=time.monotonic()
        self.queue_base=sum(j.meta.size for j in plan.jobs if j.action in ('send','receive') and j.status in ('Complete','Verified'))
        if plan.direction=='delete':
            pending=[j for j in plan.jobs if j.status!='Complete'];protected=sum(bool(j.meta.attr&7) for j in pending)
            folders=sum(j.meta.is_dir for j in pending)
            text=f'Permanently delete {len(pending)-folders} files and {folders} folders from {plan.drive}?\n\n'
            if protected:text+=f'This includes {protected} read-only, hidden or system entries.\n\n'
            text+='Review the queue first. Completed deletions cannot be undone.'
            if not messagebox.askyesno('Confirm DOS deletion',text):return
            self.work('Running deletion queue…',lambda:('executed',execute_delete(self.client,plan,self.progress_event,protected=True)))
        else:self.work('Running transfer queue…',lambda:('executed',execute(self.client,plan,self.progress_event)))
    def progress_event(self,index,job,done,total):
        if done==total or self.events.qsize()<20:
            self.events.put(('progress',(index,job.status,done,total)))
    def cancel(self):
        self.cancel_event.set()
        if self.client:self.client.cancelled.set()
        self.status.set('Cancelling after the current verified operation; subsequent jobs will stop…')
    def open_report(self):
        if self.report:subprocess.Popen(['xdg-open',self.report],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    def show_job(self,event=None):
        selected=self.queue_tree.selection()
        if not selected:return
        row=selected[0]
        if row.startswith('job:') and self.plan:
            job=self.plan.jobs[int(row.split(':')[1])]
            status,action,source,target,size=self.job_values(job)
            text=f'{action} · {status}\n\nSource: {source}\nDestination: {target}\nBytes: {size}\n'
            text+=f'DOS modified: {job.meta.display_time}\nAttributes: {job.meta.attributes}'
            if job.reason:text+='\n\n'+job.reason
        else:text='\n'.join(str(v) for v in self.queue_tree.item(row,'values') if v)
        messagebox.showinfo('Queue entry',text,parent=self.root)

    def fill(self,path,entries):
        changed=path!=self.remote
        if changed:self.tree.selection_remove(self.tree.selection())
        self.remote=path;self.location.set((self.client.drive[:2] if self.client else '')+path)
        self.remote_items=list(entries)
        if changed:self.remote_filter.set('')
        self.render_remote()

    def render_remote(self):
        selected={self.entries[i].name for i in self.tree.selection() if i in self.entries}
        self.tree.delete(*self.tree.get_children());self.entries={}
        self.tree._directories=set();query=self.remote_filter.get().casefold()
        for entry in self.remote_items:
            if query not in entry.name.casefold():continue
            key=self.tree.insert('','end',values=(entry.name+(' [resume cache]' if entry.internal else ''),
                '<DIR>' if entry.meta.is_dir else f'{entry.meta.size:,}',entry.meta.display_time,entry.meta.attributes))
            self.entries[key]=entry
            if entry.meta.is_dir:self.tree._directories.add(key)
            if entry.name in selected:self.tree.selection_add(key)
        self.sort_table(self.tree,*getattr(self.tree,'_sort',('name',False)))
        self.selection_changed()

    def job_values(self,job,status=None,compact=False):
        actions={'send':'Upload →','receive':'← Download','directory':'Folder','verified':'Verify',
                 'skip':'Skip','delete':'Delete','rmdir':'Delete folder'}
        source,target=str(job.source),str(job.target)
        drive=self.plan.drive[:2]
        if self.plan.direction in ('download','delete'):source=drive+source
        if self.plan.direction=='upload':target=drive+target
        if self.plan.direction=='delete':target='—'
        if compact:
            local=source if self.plan.direction=='upload' else target
            if self.plan.direction!='delete':
                try:local='Local: '+str(Path(local).relative_to(self.plan.local_root))
                except ValueError:pass
                if self.plan.direction=='upload':source=local
                else:target=local
        return (status or job.status,actions.get(job.action,job.action),source,target,
                '<DIR>' if job.meta.is_dir else f'{job.meta.size:,}')

    def job_visible(self,status):
        mode=self.queue_filter.get();complete=status in ('Complete','Verified','Skipped')
        return (mode=='All' or mode=='Completed' and complete or mode=='Failed' and status=='Failed'
                or mode=='Pending' and not complete and status!='Failed')

    def update_job_row(self,index,status=None):
        job=self.plan.jobs[index];status=status or job.status
        previous=self.queue_tree.set(self.rows[index],'status') if index in self.rows else None
        if not self.job_visible(status):
            if index in self.rows:self.queue_tree.delete(self.rows.pop(index))
            return
        tag=('complete' if status in ('Complete','Verified') else 'error' if status=='Failed'
             else 'running' if status=='Running' else 'muted' if status in ('Skipped','Cancelled') else '')
        values=self.job_values(job,status,compact=True)
        if index in self.rows:self.queue_tree.item(self.rows[index],values=values,tags=(tag,))
        else:self.rows[index]=self.queue_tree.insert('','end',iid=f'job:{index}',values=values,tags=(tag,))
        if previous!=status and hasattr(self.queue_tree,'_sort'):
            self.sort_table(self.queue_tree,*self.queue_tree._sort)

    def fill_plan(self):
        selected=self.queue_tree.selection();position=self.queue_tree.yview()
        self.queue_tree.delete(*self.queue_tree.get_children());self.rows={}
        if not self.plan:return
        mode=self.queue_filter.get()
        for i in range(len(self.plan.jobs)):self.update_job_row(i)
        if mode in ('All','Failed'):
            for e in self.plan.errors:self.queue_tree.insert('','end',values=('ERROR','',e['path'],e['error'],''),tags=('error',))
        if mode=='All':
            for e in self.plan.excluded:self.queue_tree.insert('','end',values=('Excluded','',e['path'],e['reason'],''),tags=('muted',))
            for p in self.plan.missing:self.queue_tree.insert('','end',values=('Missing on DOS','',p,'Previous local copy retained',''),tags=('muted',))
        for row in selected:
            if self.queue_tree.exists(row):self.queue_tree.selection_add(row)
        if hasattr(self.queue_tree,'_sort'):self.sort_table(self.queue_tree,*self.queue_tree._sort)
        if position:self.queue_tree.yview_moveto(position[0])
        self.queue_summary();self.queue_totals()

    def queue_summary(self):
        self.summary.set(f'Queue: {self.plan.status} · {len(self.rows)} / {len(self.plan.jobs)} jobs shown · '
            f'{self.plan.bytes:,} transfer bytes · {len(self.plan.errors)} issues · {len(self.plan.missing)} missing on DOS')

    def finish_progress(self):
        amount=self.progress['value']
        self.progress.stop()
        if str(self.progress['mode'])=='indeterminate':
            self.progress.configure(mode='determinate',value=0);self.file_percent.set('—')
        else:self.progress['value']=amount

    def refresh_after_queue(self):
        # Capture the completion summary before work() changes the status. A normal
        # refresh used to erase it immediately after every completed queue.
        summary,detail,amount,percent=self.status.get(),self.detail.get(),self.progress['value'],self.file_percent.get()
        path=self.remote
        self.work('Refreshing DOS folder…',lambda:('queue_listing',path,self.client.listing(path),
            self.client.diagnostics(),summary,detail,amount,percent))

    def poll(self):
        try:
            while True:
                kind,value=self.events.get_nowait()
                if kind=='note':self.status.set(value);self.log_message(value)
                elif kind=='progress':
                    index,status,done,total=value
                    if not self.plan:continue
                    self.active_progress=(index,done)
                    job=self.plan.jobs[index]
                    self.update_job_row(index,status);self.queue_summary()
                    if self.progress_file!=index:
                        self.progress_file=index;self.progress_base=done;self.started=time.monotonic()
                    elapsed=max(.01,time.monotonic()-self.started)
                    rate=max(0,done-self.progress_base)/elapsed
                    eta=(total-done)/rate if rate>0 else None
                    self.progress.stop();self.progress.configure(mode='determinate',value=100 if not total else 100*done/total)
                    self.file_percent.set(f"{min(100,int(self.progress['value']))}%")
                    self.status.set(f'{index+1}/{len(self.plan.jobs)} — {job.target} — {status}')
                    self.detail.set(f'{done:,} / {total:,} bytes • {rate/1024:.1f} KiB/s • '
                        f'ETA {int(eta)//60}m {int(eta)%60}s' if eta is not None else f'{done:,} / {total:,} bytes • preparing / verifying')
                    if job.action not in ('send','receive'):
                        self.detail.set(f'{self.job_values(job,status)[1]} · {status}')
                    self.detail.set(self.detail.get()+f' • {self.client.retries if self.client else 0} link retries');self.queue_totals(index,done)
                elif kind=='done':
                    self.busy=False;self.finish_progress()
                    if value[0]=='connected':
                        _,self.client,hello,entries,info,path=value;self.fill(path,entries);self.display_diagnostics(info);self.status.set(hello)
                    elif value[0] in ('listing','queue_listing'):
                        self.fill(value[1],value[2]);self.display_diagnostics(value[3]);self.status.set(f'{len(value[2])} entries; hidden/system entries included.')
                        if value[0]=='queue_listing':
                            self.status.set(value[4]);self.detail.set(value[5]);self.progress['value']=value[6];self.file_percent.set(value[7])
                    elif value[0]=='prepared':
                        self.plan=value[1];self.active_progress=None;self.queue_filter.set('All');self.fill_plan()
                        self.status.set('Preview ready. Review source and destination, then Start / resume.')
                    elif value[0]=='executed':
                        self.report=value[1];self.fill_plan();self.status.set(self.plan.status+' · Use Open report for full details.')
                        self.detail.set('Double-click a queue entry for full paths, DOS metadata and any issue details.')
                        self.local_navigate(self.local)
                    elif value[0] in ('diagnostics','settings'):
                        self.display_diagnostics(value[1]);self.status.set('Connection checked.' if value[0]=='diagnostics' else 'Settings confirmed on DOS and Linux.')
                        if value[0]=='diagnostics':
                            d=value[1];lat=read_latency(self.device.get())
                            messagebox.showinfo('Connection diagnostics',f"DOS COM{d['com']} · {d['baud']} baud · {d['flow']}\nPeer checks: {d['peer']} · block {d['block_size']} · FIFO {bool(d['fifo'])}\n\nCTS {bool(d['msr']&16)} · DSR {bool(d['msr']&32)} · DCD {bool(d['msr']&128)}\nCRC errors: {d['crc_errors']}\nUART overruns: {d['overruns']}\nParity errors: {d['parity_errors']}\nFraming errors: {d['framing_errors']}\nBreaks: {d['breaks']}\nTransmit/handshake timeouts: {d['flow_timeouts']}\nClient retries: {self.client.retries}\nFTDI latency: {str(lat)+' ms' if lat is not None else 'unavailable'}\nFree DOS space: {d['free_bytes']:,} bytes")
                    elif value[0]=='latency':self.status.set(f'FTDI latency set to {value[1]} ms; previous setting retained for restoration.')
                    elif value[0]=='disconnected':
                        self.client=None;self.latency_session=None;self.info={};self.disk_space.set('')
                        self.remote_items=[];self.render_remote();self.remote_frame.configure(text='DOS files')
                        self.status.set('Disconnected.'+value[1])
                    elif value[0]=='partials':
                        count,protected=value[1]
                        self.status.set(f'{count} resumable uploads; {protected} records protected for inspection.')
                        if count and messagebox.askyesno('Discard partial uploads?',f'Discard {count} incomplete DOS uploads to free their disk space? Completed destination files are preserved.\n\n{protected} recovery records will be kept.'):
                            self.work('Discarding incomplete uploads…',lambda:('cleared',self.client.partials(True)))
                    elif value[0]=='cleared':
                        self.status.set(f'{value[1][0]} partial uploads discarded; {value[1][1]} recovery records retained.')
                    elif value[0]=='verified':
                        result,self.report=value[1:];self.status.set(f'{result["checked"]} entries verified; {len(result["errors"])} issues. Report: {self.report}')
                    self.log_message(self.status.get());self.update_controls()
                    if value[0] not in ('executed','queue_listing'):
                        self.detail.set('CRC and whole-file verification are always enabled. DOS metadata is preserved.')
                    if value[0]=='executed' and not self.closing:self.refresh_after_queue()
                elif kind=='error':
                    self.busy=False;self.finish_progress();self.status.set(str(value));self.log_message(str(value))
                    self.detail.set('Completed work and resumable files are retained. Review the queue or activity log.')
                    if self.plan and self.plan.status in ('Paused','Cancelled','Interrupted'):
                        self.fill_plan()
                        folder=Path(self.plan.local_root)/'.nuxtlink'
                        if (folder/'last-report.txt').exists():self.report=str(folder/'last-report.txt')
                    if not isinstance(value,Cancelled) and not self.closing:messagebox.showerror('NuXT Link',str(value))
                    self.update_controls()
        except queue.Empty:pass
        if self.closing and not self.busy:
            if self.client or self.latency_session:self.disconnect()
            else:self.root.destroy();return
        self.root.after(60,self.poll)

    def close_window(self):
        self.closing=True
        if self.busy:self.cancel()
        elif self.client or self.latency_session:self.disconnect()
        else:self.root.destroy()

    def log_message(self, text):
        self.log.configure(state='normal');self.log.insert('end',time.strftime('%H:%M:%S ')+text+'\n')
        if int(self.log.index('end-1c').split('.')[0])>1000:self.log.delete('1.0','101.0')
        self.log.see('end');self.log.configure(state='disabled')

    def toggle_log(self):
        if self.show_log.get():self.panes.add(self.log_frame,weight=1)
        else:self.panes.forget(self.log_frame)
        self.arrange_panes()

    def profile_values(self):
        return validate(dict(device=self.device.get().strip(),baud=int(self.speed.get()),flow=self.flow.get(),peer=self.peer.get(),block=int(self.block_size.get()),latency=int(self.latency.get()),com=self.doscom.get(),local=str(self.local),remote=self.remote))

    def save_profile(self):
        try:self.profiles.save(self.profile_name.get(),self.profile_values())
        except (ValueError,OSError) as exc:messagebox.showerror('Profile',str(exc));return
        self.profile_box.configure(values=sorted(self.profiles.items));self.log_message('Saved profile '+self.profile_name.get())

    def load_profile(self):
        if self.busy or self.client:return
        values=self.profiles.items.get(self.profile_name.get())
        if not values:return
        for key,var in [('device',self.device),('baud',self.speed),('flow',self.flow),('peer',self.peer),('block',self.block_size),('latency',self.latency),('com',self.doscom)]:var.set(str(values[key]))
        self.remote=values['remote'];self.location.set(self.remote);self.local_navigate(values['local'])

    def remove_profile(self):
        if self.busy:return
        name=self.profile_name.get()
        if name in self.profiles.items and messagebox.askyesno('Remove profile','Remove saved profile '+name+'?'):
            self.profiles.delete(name);self.profile_box.configure(values=sorted(self.profiles.items))

    def choose_local(self):
        path=filedialog.askdirectory(initialdir=str(self.local))
        if path:self.local_navigate(path)

    def local_navigate(self, path=None):
        try:
            directory=Path(path or self.local_location.get()).expanduser().absolute()
            entries=sorted(directory.iterdir(),key=lambda p:(not p.is_dir(),p.name.casefold()))
            rows=[]
            for item in entries:
                try:
                    info=item.stat();rows.append((item,info))
                except OSError:continue
            changed=directory!=self.local
            self.local=directory;self.local_location.set(str(directory));self.local_items=rows
            if changed:self.local_filter.set('')
            self.render_local()
        except OSError as exc:messagebox.showerror('Local folder',str(exc))

    def render_local(self):
        selected={self.local_entries[i] for i in self.local_tree.selection() if i in self.local_entries}
        self.local_tree.delete(*self.local_tree.get_children());self.local_entries={}
        self.local_tree._directories=set();query=self.local_filter.get().casefold()
        for item,info in self.local_items:
            if query not in item.name.casefold():continue
            is_dir=stat.S_ISDIR(info.st_mode)
            key=self.local_tree.insert('','end',values=(item.name,'<DIR>' if is_dir else f'{info.st_size:,}',
                time.strftime('%Y-%m-%d %H:%M',time.localtime(info.st_mtime))))
            self.local_entries[key]=item
            if is_dir:self.local_tree._directories.add(key)
            if item in selected:self.local_tree.selection_add(key)
        self.sort_table(self.local_tree,*getattr(self.local_tree,'_sort',('name',False)))
        self.selection_changed()

    def local_open(self,event=None):
        selected=self.local_tree.selection()
        if len(selected)!=1:return
        path=self.local_entries[selected[0]]
        if path.is_dir():self.local_navigate(path)
        else:self.upload_selection()

    def local_context(self,event):
        item=self.local_tree.identify_row(event.y)
        if item and item not in self.local_tree.selection():self.local_tree.selection_set(item)
        self.selection_changed()
        if not self.busy:self.local_menu.tk_popup(event.x_root,event.y_root)

    def remote_context(self,event):
        item=self.tree.identify_row(event.y)
        if item and item not in self.tree.selection():self.tree.selection_set(item)
        self.selection_changed()
        if self.client and not self.busy:self.remote_menu.tk_popup(event.x_root,event.y_root)

    def upload_selection(self):
        if not self.client or self.busy:return
        sources=[self.local_entries[i] for i in self.local_tree.selection()]
        if sources:self.prepare_upload(sources)

    def clear_queue(self):
        if self.busy:return
        self.plan=None;self.rows={};self.active_progress=None;self.queue_tree.delete(*self.queue_tree.get_children())
        self.summary.set('No queued operations.');self.overall.set('Queue empty');self.progress['value']=0
        self.queue_progress['value']=0;self.file_percent.set('—');self.queue_percent.set('—')
        self.detail.set('CRC and whole-file verification are always enabled.')
        self.status.set('Queue cleared. Transferred files and resumable data retained.');self.update_controls()

    def retry_queue(self):
        if self.busy or not self.plan:return
        for job in self.plan.jobs:
            if job.status in ('Failed','Cancelled','Running'):job.status='Queued'
        self.start_queue()

    def mkdir_remote(self):
        if self.busy or not self.client:return
        name=simpledialog.askstring('New DOS folder','Folder name (DOS 8.3):',parent=self.root)
        if not name:return
        try:path=join_path(self.remote,name)
        except ValueError as exc:messagebox.showerror('Folder name',str(exc));return
        root=self.remote
        def action():
            self.client.mkdir(path)
            return ('listing',root,self.client.listing(root),self.client.diagnostics())
        self.work('Creating '+path,action)

    def rename_remote(self):
        if self.busy or not self.client:return
        selected=self.tree.selection()
        if len(selected)!=1:return
        entry=self.entries[selected[0]]
        if entry.internal:return
        name=simpledialog.askstring('Rename / move DOS entry','New 8.3 name, or root-relative destination path:',initialvalue=entry.name,parent=self.root)
        if not name:return
        try:destination=dos_path(name) if name.startswith(('\\','/')) else join_path(self.remote,name)
        except ValueError as exc:messagebox.showerror('DOS path',str(exc));return
        source=join_path(self.remote,entry.name);root=self.remote
        def action():
            self.client.rename(source,destination,entry.meta)
            return ('listing',root,self.client.listing(root),self.client.diagnostics())
        self.work('Renaming '+source+' → '+destination,action)

    def delete_remote(self):
        if self.busy or not self.client:return
        selected=[self.entries[i] for i in self.tree.selection()]
        if any(e.internal for e in selected):return
        names=[e.name for e in selected]
        if not names:return
        root=self.remote;report_root=self.state_dir/'deletion-reports'
        report_root.mkdir(parents=True,exist_ok=True)
        self.work('Building recursive deletion preview…',lambda:('prepared',plan_delete(self.client,root,names,report_root,self.notify)))

    def display_diagnostics(self,info):
        self.info=info
        self.disk_space.set(f"Free {info['free_bytes']/1048576:.1f} / {info['total_bytes']/1048576:.1f} MiB")
        self.remote_frame.configure(text='DOS files · '+self.disk_space.get())
        self.doscom.set('COM'+str(info['com']))
        self.connection_state()

    def diagnostics(self):
        if self.busy or not self.client:return
        self.work('Testing connection and reading UART status…',lambda:('diagnostics',self.client.diagnostics()))

    def apply_connection(self):
        if self.busy or not self.client:return
        try:v=self.profile_values()
        except ValueError as exc:messagebox.showerror('Settings',str(exc));return
        self.work('Negotiating settings; an unconfirmed switch rolls back in about 12 seconds…',lambda:('settings',self.client.apply_settings(v['baud'],v['flow'],v['peer'],v['block'])),cancellable=False)

    def apply_ftdi(self):
        if self.busy:return
        try:value=int(self.latency.get())
        except ValueError:messagebox.showerror('FTDI latency','Enter 0 to keep the current value, or 2..255 ms.');return
        if value==0:self.status.set('FTDI latency left unchanged.');return
        if not 2<=value<=255:messagebox.showerror('FTDI latency','Use 2..255 ms.');return
        device=self.device.get().strip()
        def action():
            if self.latency_session and self.latency_session.device!=device:self.latency_session.restore(authorize=True);self.latency_session=None
            if self.latency_session is None:self.latency_session=LatencySession(device)
            if not self.latency_session.apply(value,authorize=True):raise ValueError('This device has no FTDI latency control; PTYs and other adapters can still transfer normally.')
            return ('latency',value)
        self.work('Applying FTDI latency; Linux may request administrator authentication…',action,cancellable=False)

    def disconnect(self):
        client=self.client;latency=self.latency_session
        def action():
            if client:
                try:client.abort()
                finally:client.transport.close()
            warning=''
            if latency:
                try:latency.restore(authorize=True)
                except (OSError,ValueError) as exc:warning=' Latency restoration: '+str(exc)
            return ('disconnected',warning)
        self.work('Disconnecting and restoring adapter settings…',action,cancellable=False)

    def queue_totals(self,index=None,done=0):
        if not self.plan:return
        if index is None and self.active_progress is not None:index,done=self.active_progress
        complete=sum(j.status in ('Complete','Verified','Skipped') for j in self.plan.jobs)
        failed=sum(j.status=='Failed' for j in self.plan.jobs)
        transferred=sum(j.meta.size for j in self.plan.jobs if j.action in ('send','receive') and j.status in ('Complete','Verified'))
        if index is not None:
            job=self.plan.jobs[index]
            if job.action in ('send','receive') and job.status=='Running':transferred+=done
        elapsed=max(.01,time.monotonic()-self.queue_started);rate=max(0,transferred-self.queue_base)/elapsed
        remaining=max(0,self.plan.bytes-transferred);eta=f'{int(remaining/rate)//60}m {int(remaining/rate)%60}s' if rate else '—'
        percent=(100*transferred/self.plan.bytes if self.plan.bytes else
                 100*complete/len(self.plan.jobs) if self.plan.jobs else 0)
        self.queue_progress['value']=min(100,percent);self.queue_percent.set(f'{min(100,int(percent))}%')
        # ETA is meaningful only while the queue is actually running.
        suffix=f' · ETA {eta}' if self.plan.status=='Running' and rate else ''
        self.overall.set(f'{complete}/{len(self.plan.jobs)} finished · {failed} failed · {transferred:,}/{self.plan.bytes:,} bytes'+suffix)



def main():
    parser=argparse.ArgumentParser(description='NuXT Link 3 serial file manager. Connection settings are negotiated with the DOS server.')
    parser.add_argument('--profile');parser.add_argument('--device')
    parser.add_argument('--baud',type=int,choices=BAUDS);parser.add_argument('--flow',choices=('none','rtscts'))
    parser.add_argument('--peer',choices=tuple(PEERS));parser.add_argument('--block',type=int,choices=(128,256,512))
    parser.add_argument('--latency',type=int,help='FTDI milliseconds; 0 leaves the adapter unchanged')
    parser.add_argument('--com',choices=('COM1','COM2','COM3','COM4'),help='DOS port reminder/profile; changes locally with DOS C key or command line')
    parser.add_argument('--local');parser.add_argument('--remote');parser.add_argument('--connect',action='store_true')
    args=parser.parse_args();values={}
    if args.profile:
        store=Profiles()
        if args.profile not in store.items:parser.error('Unknown saved profile')
        values.update(store.items[args.profile])
    values.update({k:v for k,v in vars(args).items() if k in DEFAULTS and v is not None})
    try:values=validate(values)
    except ValueError as exc:parser.error(str(exc))
    root=tk.Tk();app=App(root,values)
    if args.profile:app.profile_name.set(args.profile)
    if args.connect:root.after(200,app.connect)
    root.mainloop()

if __name__=='__main__':main()
