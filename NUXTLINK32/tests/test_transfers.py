import sys, unittest, tempfile, json, os
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'linux'))
from link import *
from transfers import *

class MemoryDOS:
    drive='C:\\'
    def __init__(self):
        self.entries={'\\': []}
        self.cancelled=threading.Event()
    def listing(self,path):
        if path not in self.entries:raise RemoteError('DOS error 3')
        return self.entries[path]
    def hash(self,path):return self.data[path]

class TransferTests(unittest.TestCase):
    def test_raw_metadata_roundtrip_without_xattrs(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'HIDDEN.SYS';path.write_bytes(b'abc')
            meta=Metadata(3,0x2d5f,0x645d,7)
            with patch('os.setxattr',side_effect=OSError('unsupported')):
                preserve_metadata(path,meta)
            self.assertEqual(local_metadata(path),meta)
            self.assertFalse(path.stat().st_mode & 0o222)
            self.assertEqual(int(path.stat().st_mtime),int(meta.timestamp))
    def test_unset_dos_date_preserved_exactly(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'ZERO.DAT';path.touch()
            meta=Metadata(0,0,0,0);preserve_metadata(path,meta)
            self.assertEqual(local_metadata(path),meta)
    def test_oem_case_keys_do_not_expand_sharp_s(self):
        self.assertNotEqual(dos_key("ß.TXT"),dos_key("SS.TXT"))
        self.assertEqual(dos_key("é.TXT"),dos_key("É.TXT"))

    def test_paths_and_devices_cannot_escape_drive(self):
        for path in ('C:\\X','D:X','\\..\\X','\\A\\.\\B','\\CON.TXT','\\A\\B\\','\\A//B'):
            with self.assertRaises(ValueError,msg=path):dos_path(path)
        self.assertEqual(dos_path('dir/file.txt'),'\\DIR\\FILE.TXT')
        self.assertEqual(valid_name('ÉCOLE.DAT'),'ÉCOLE.DAT')
    def test_local_symlinks_and_traversal_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'LINK').symlink_to('/tmp',target_is_directory=True)
            for path in ('../OUT','/tmp/OUT','LINK/OUT'):
                with self.assertRaises(ValueError):safe_local(root,path)
    def test_name_collisions_are_preflighted_uniquely(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)/'Upload';root.mkdir()
            (root/'abcdefgh-one.txt').write_text('1')
            (root/'abcdefgh-two.txt').write_text('2')
            (root/'empty').mkdir()
            c=MemoryDOS()
            plan=plan_upload(c,[root],'\\',policy='overwrite')
            self.assertFalse(plan.errors)
            targets=[j.target for j in plan.jobs]
            self.assertEqual(len(targets),len(set(targets)))
            self.assertIn('\\UPLOAD\\ABCDEFGH.TXT',targets)
            self.assertIn('\\UPLOAD\\ABCDEF~1.TXT',targets)
            self.assertIn('\\UPLOAD\\EMPTY',targets)
            c.entries['\\UPLOAD']=[Entry('ABCDEFGH.TXT',Metadata(9,0,0,32)), Entry('ABCDEF~1.TXT',Metadata(9,0,0,32))]
            repeat=plan_upload(c,[root],'\\',policy='overwrite',incremental=False)
            self.assertEqual([j.target for j in repeat.jobs],targets)
    def test_backup_damage_report_and_restore_exclusion(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);file=root/'DATA.BIN';file.write_bytes(b'good')
            meta=Metadata(4,0x2d5f,0x645d,0x20);preserve_metadata(file,meta)
            _,checksum,digest=checksums(file)
            plan=Plan('download',str(root),'\\','C:\\',backup=True,status='Complete')
            plan.jobs=[Job('\\DATA.BIN',str(file),meta,'receive',status='Complete',result={'crc16':checksum,'sha256':digest})]
            save_report(plan,report_paths(plan))
            self.assertFalse(verify_backup(root)[0]['errors'])
            file.write_bytes(b'evil')
            self.assertEqual(len(verify_backup(root)[0]['errors']),1)
            restore=plan_upload(MemoryDOS(),[root],'\\',contents=True,restore=True,policy='overwrite')
            self.assertEqual(len(restore.errors),1)
            self.assertFalse(restore.jobs)
    def test_incremental_checks_data_and_reports_missing(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);file=root/'DATA.BIN';file.write_bytes(b'good')
            meta=Metadata(4,0x2d5f,0x645d,7);preserve_metadata(file,meta)
            c=MemoryDOS();c.entries['\\']=[Entry('DATA.BIN',meta),Entry('NX$CACHE',Metadata(0,0,0,18),True)]
            c.data={'\\DATA.BIN':(meta,crc(b'good'))}
            plan=plan_download(c,'\\',root,backup=True)
            self.assertEqual(plan.jobs[0].action,'verified')
            self.assertEqual(len(plan.excluded),1)
            file.chmod(0o600);file.write_bytes(b'evil')
            plan=plan_download(c,'\\',root,backup=True)
            self.assertEqual(plan.jobs[0].action,'receive')
    def test_metadata_folder_collision_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)/'.nuxtlink';p.mkdir();(p/'KEEP').write_text('mine')
            with self.assertRaises(LinkError):metadata_dir(folder)
            self.assertEqual((p/'KEEP').read_text(),'mine')

if __name__=='__main__':unittest.main(verbosity=2)
