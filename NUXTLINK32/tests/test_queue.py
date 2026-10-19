import sys, tempfile, threading, json, unittest, os
from pathlib import Path
from dataclasses import asdict
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'linux'))
from link import *
from transfers import *
from transfer_queue import *

class FakeDOS:
    drive='C:\\'
    def __init__(self):self.cancelled=threading.Event();self.sent=[];self.received=[];self.metadata=[]
    def abort(self):pass
    def mkdir(self,path):pass
    def set_metadata(self,path,meta):self.metadata.append((path,meta))
    def upload(self,source,target,progress,**kwargs):
        self.sent.append((str(source),target));progress(1,1)
        return {'metadata':asdict(kwargs['meta'])}
    def download(self,source,target,progress,**kwargs):
        self.received.append((source,str(target)));Path(target).write_bytes(b'abc');progress(3,3)
        preserve_metadata(target,kwargs['expected_meta'])
        return {'metadata':asdict(kwargs['expected_meta'])}

class Queues(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name);self.c=FakeDOS()
        for name in ('ONE','TWO'):
            (self.root/name).mkdir();(self.root/name/'SAME.TXT').write_bytes(b'abc')
    def tearDown(self):self.temp.cleanup()
    def upload(self,local,remote):
        src=self.root/local/'SAME.TXT'
        return Plan('upload',str(src.parent),remote,'C:\\',jobs=[Job(str(src),remote+'\\SAME.TXT',local_metadata(src),'send')])
    def download(self,remote,local):
        dst=self.root/local/'GOT.TXT';meta=local_metadata(self.root/'ONE/SAME.TXT')
        return Plan('download',str(dst.parent),remote,'C:\\',jobs=[Job(remote+'\\SAME.TXT',str(dst),meta,'receive')])
    def test_multiple_upload_roots_keep_exact_destinations_and_reports(self):
        q=append_plan(None,self.upload('ONE','\\ONE'));append_plan(q,self.upload('TWO','\\TWO'))
        execute_queue(self.c,q)
        self.assertEqual([x[1] for x in self.c.sent],['\\ONE\\SAME.TXT','\\TWO\\SAME.TXT'])
        self.assertEqual(q.status,'Complete');self.assertEqual(len(q.jobs),2)
        doc=json.loads((self.root/'ONE/.nuxtlink/last-queue.json').read_text())
        self.assertEqual(len(doc['batches']),2)
    def test_mixed_upload_download_batches_keep_local_folders(self):
        q=append_plan(None,self.upload('ONE','\\UP'))
        append_plan(q,self.download('\\A','ONE'));append_plan(q,self.download('\\B','TWO'))
        execute_queue(self.c,q)
        self.assertEqual([p[0] for p in self.c.received],['\\A\\SAME.TXT','\\B\\SAME.TXT'])
        self.assertEqual((self.root/'ONE/GOT.TXT').read_bytes(),b'abc')
        self.assertEqual((self.root/'TWO/GOT.TXT').read_bytes(),b'abc')
        self.assertEqual(local_metadata(self.root/'TWO/GOT.TXT'),q.jobs[-1].meta)
    def test_cancel_later_batch_resume_does_not_repeat_completed_upload(self):
        q=append_plan(None,self.upload('ONE','\\ONE'));append_plan(q,self.upload('TWO','\\TWO'))
        def progress(i,j,d,t):
            if i==0 and j.status=='Complete':self.c.cancelled.set()
        with self.assertRaises(Cancelled):execute_queue(self.c,q,progress)
        self.assertEqual(q.jobs[0].status,'Complete');self.assertEqual(q.jobs[1].status,'Queued')
        self.c.cancelled.clear();execute_queue(self.c,q)
        self.assertEqual(len(self.c.sent),2);self.assertEqual(q.status,'Complete')
    def test_destination_conflict_leaves_queue_unchanged(self):
        q=append_plan(None,self.upload('ONE','\\SAME'))
        with self.assertRaisesRegex(ValueError,'Already queued'):append_plan(q,self.upload('TWO','\\same'))
        self.assertEqual(len(q.jobs),1);self.assertEqual(len(q.plans),1)
    def test_ancestor_file_conflict_is_detected(self):
        first=self.upload('ONE','\\A');first.jobs[0].target='\\A'
        q=append_plan(None,first)
        with self.assertRaisesRegex(ValueError,'queued file'):append_plan(q,self.upload('TWO','\\A'))
    def test_different_drives_and_deletions_cannot_mix(self):
        q=append_plan(None,self.upload('ONE','\\A'));plan=self.upload('TWO','\\B');plan.drive='D:\\'
        with self.assertRaises(ValueError):append_plan(q,plan)
        plan.direction='delete';plan.drive='C:\\'
        with self.assertRaises(ValueError):append_plan(q,plan)
        self.assertEqual(len(q.plans),1)
    def test_changed_upload_metadata_rejected_before_writing(self):
        q=append_plan(None,self.upload('ONE','\\ONE'));path=self.root/'ONE/SAME.TXT'
        os.utime(path,(path.stat().st_mtime+20,)*2)
        with self.assertRaisesRegex(LinkError,'metadata changed'):execute_queue(self.c,q)
        self.assertFalse(self.c.sent);self.assertEqual(q.jobs[0].status,'Failed')
    def test_later_batch_restores_earlier_directory_metadata(self):
        plan=self.upload('ONE','\\ONE');meta=Metadata(0,0x5c41,0x5000,16)
        plan.jobs.insert(0,Job(str(self.root/'ONE'),'\\ONE',meta,'directory'))
        q=append_plan(None,plan);append_plan(q,self.upload('TWO','\\ONE\\SUB'))
        execute_queue(self.c,q)
        self.assertEqual(self.c.metadata[-1],('\\ONE',meta))
        self.assertEqual(q.jobs[0].status,'Complete')
    def test_backup_manifests_remain_separate(self):
        one=self.download('\\A','ONE');two=self.download('\\B','TWO');one.backup=two.backup=True
        q=append_plan(None,one);append_plan(q,two);execute_queue(self.c,q)
        for name,path in [('ONE','\\A'),('TWO','\\B')]:
            doc=json.loads((self.root/name/'.nuxtlink/backup.json').read_text())
            self.assertEqual(doc['remote_root'],path);self.assertEqual(len(doc['jobs']),1)
    def test_changed_download_source_cannot_replace_existing_file(self):
        path=self.root/'ONE/GOT.TXT';path.write_bytes(b'keep')
        c=Client(None);old=Metadata(3,1,1,32);c.stat=lambda name:Metadata(3,1,2,32);c.abort=lambda:None
        with self.assertRaisesRegex(LinkError,'metadata changed'):
            c.download('\\FILE',path,overwrite=True,expected_meta=old)
        self.assertEqual(path.read_bytes(),b'keep')
