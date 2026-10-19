import json
import errno
import struct
import sys
import tempfile
import threading
import unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'linux'))
from link import *
from transfers import *
from transfer_queue import append_plan, execute_queue


class CompletionTests(unittest.TestCase):
    def test_download_metadata_failure_restores_overwritten_file_and_stage(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);dest=root/'DATA.BIN';dest.write_bytes(b'old data')
            temp=root/'new.part';temp.write_bytes(b'verified new data')
            meta=Metadata(temp.stat().st_size,0x5d53,0x6000,32)
            with patch('link.preserve_metadata',side_effect=OSError('metadata write failed')):
                with self.assertRaisesRegex(OSError,'metadata write failed'):
                    commit_download(temp,dest,meta,True)
            self.assertEqual(dest.read_bytes(),b'old data')
            self.assertEqual(temp.read_bytes(),b'verified new data')
            self.assertFalse((root/'new.part.previous').exists())

    def test_download_overwrite_falls_back_when_hard_links_are_unavailable(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);dest=root/'DATA.BIN';dest.write_bytes(b'old data')
            temp=root/'new.part';temp.write_bytes(b'verified new data')
            meta=Metadata(temp.stat().st_size,0x5d53,0x6000,32)
            with patch('link.os.link',side_effect=OSError(errno.EOPNOTSUPP,'no hard links')):
                with patch('link.preserve_metadata',side_effect=OSError('metadata write failed')):
                    with self.assertRaisesRegex(OSError,'metadata write failed'):
                        commit_download(temp,dest,meta,True)
            self.assertEqual(dest.read_bytes(),b'old data')
            self.assertEqual(temp.read_bytes(),b'verified new data')
            self.assertFalse((root/'new.part.previous').exists())

    def test_download_metadata_failure_removes_new_destination(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);dest=root/'DATA.BIN'
            temp=root/'new.part';temp.write_bytes(b'verified new data')
            meta=Metadata(temp.stat().st_size,0x5d53,0x6000,32)
            with patch('link.preserve_metadata',side_effect=OSError('metadata write failed')):
                with self.assertRaisesRegex(OSError,'metadata write failed'):
                    commit_download(temp,dest,meta,False)
            self.assertFalse(dest.exists())
            self.assertEqual(temp.read_bytes(),b'verified new data')

    def test_download_commit_preserves_metadata_and_cleans_old_link(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);dest=root/'DATA.BIN';dest.write_bytes(b'old data')
            temp=root/'new.part';temp.write_bytes(b'verified new data')
            meta=Metadata(temp.stat().st_size,0x5d53,0x6000,32)
            commit_download(temp,dest,meta,True)
            self.assertEqual(dest.read_bytes(),b'verified new data')
            self.assertEqual(local_metadata(dest),meta)
            self.assertFalse(temp.exists())
            self.assertFalse((root/'new.part.previous').exists())

    def test_download_no_clobber_commit_and_existing_file_rejection(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);dest=root/'DATA.BIN'
            temp=root/'new.part';temp.write_bytes(b'verified new data')
            meta=Metadata(temp.stat().st_size,0x5d53,0x6000,32)
            commit_download(temp,dest,meta,False)
            self.assertEqual(dest.read_bytes(),b'verified new data')
            self.assertEqual(local_metadata(dest),meta)
            self.assertFalse(temp.exists())
            temp.write_bytes(b'another verified file')
            with self.assertRaises(FileExistsError):
                commit_download(temp,dest,meta,False)
            self.assertEqual(dest.read_bytes(),b'verified new data')
            self.assertEqual(temp.read_bytes(),b'another verified file')

    def test_ratio_uses_transferred_portion_and_handles_empty_or_raw(self):
        job=Job('src','dst',Metadata(1000,1,1,32),'send',status='Complete',elapsed_seconds=63.2,
                result={'resumed_bytes':600,'upload_payload_bytes':100})
        self.assertEqual(completion_columns(job),('1m 03s','4.00:1 (75% saved)'))
        self.assertIn('last resumed portion',completion_report(job))
        self.assertEqual(completion_columns(job,'Running'),('',''))
        job.result['upload_payload_bytes']=400
        self.assertEqual(completion_columns(job)[1],'1.00:1 (0% saved)')
        job.result={'resumed_bytes':1000,'upload_payload_bytes':0}
        self.assertEqual(completion_columns(job)[1],'Already staged')
        job.meta=Metadata(0,1,1,32)
        self.assertEqual(completion_columns(job)[1],'Empty file')
        job.meta=Metadata(1000,1,1,32);job.action='receive';job.result={}
        self.assertEqual(completion_columns(job)[1],'Raw')

    def test_retry_time_excludes_pause_and_reports_keep_results(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);src=root/'DATA.BIN';src.write_bytes(b'A'*100)
            job=Job(str(src),'\\DATA.BIN',local_metadata(src),'send')
            plan=Plan('upload',folder,'\\','C:\\',jobs=[job]);queue=append_plan(None,plan)
            class Fake:
                cancelled=threading.Event();fail=True
                def abort(self):pass
                def upload(self,*args,**kwargs):
                    if self.fail:raise RemoteError('Injected failure')
                    return {'upload_payload_bytes':50,'resumed_bytes':0,'compressed_blocks':1,'raw_blocks':0}
            client=Fake()
            with patch('transfers.time.monotonic',side_effect=[1,3]):execute_queue(client,queue)
            self.assertEqual(job.elapsed_seconds,2)
            self.assertEqual(job.status,'Failed')
            client.fail=False
            with patch('transfers.time.monotonic',side_effect=[100,104]):execute_queue(client,queue)
            self.assertEqual(job.elapsed_seconds,6)
            self.assertEqual(job.reason,'')
            self.assertEqual(job.status,'Complete')
            report=(root/'.nuxtlink/last-report.txt').read_text()
            self.assertIn('Time: 6.0s',report);self.assertIn('2.00:1 (50% saved)',report)
            doc=json.loads((root/'.nuxtlink/last-queue.json').read_text())
            self.assertEqual(doc['batches'][0]['jobs'][0]['elapsed_seconds'],6)
            with patch('transfers.time.monotonic',side_effect=AssertionError('Repeated completed job')):
                execute_queue(client,queue)
            self.assertEqual(job.elapsed_seconds,6)

    def test_staging_readback_mismatch_prevents_commit(self):
        with tempfile.TemporaryDirectory() as folder:
            src=Path(folder)/'DATA.BIN';src.write_bytes(b'A'*4096)
            c=Client(None);calls=[];phases=[]
            def request(kind,payload=b'',**kwargs):
                calls.append(kind)
                if kind==PUT_BEGIN:return struct.pack('<I',0)
                if kind==PUT_STATUS:return struct.pack('<IIHB',0,0,0xffff,1)
                if kind in (PUT_DATA,PUT_COMPRESSED,PUT_ZERO_RLE,PUT_LZ4,CANCEL):return b''
                if kind==PUT_VERIFY:return struct.pack('<IIHB',4096,4096,crc(src.read_bytes())^1,1)
                raise AssertionError(f'Unexpected commit after bad readback: {kind}')
            c.request=request
            with self.assertRaisesRegex(LinkError,'readback failed'):
                c.upload(src,'\\DATA.BIN',phase=lambda label,d,t: phases.append((label,d,t)))
            self.assertIn(PUT_VERIFY,calls);self.assertNotIn(PUT_END,calls)
            self.assertEqual(calls[-1],CANCEL)
            self.assertEqual(phases[0],('Verifying on DOS',0,4096))
            self.assertEqual(phases[-1],('Verifying on DOS',4096,4096))
