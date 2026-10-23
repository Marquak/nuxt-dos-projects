import sys
import unittest
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'linux'))
from link import Metadata
from transfers import Job, completion_columns, completion_report
from slinkxt import App


class CodecDisplayTests(unittest.TestCase):
    def test_mixed_upload_shows_actual_codecs_and_ratio(self):
        job = Job('src', 'dst', Metadata(1000, 1, 1, 32), 'send',
                  status='Complete', elapsed_seconds=63,
                  result={'resumed_bytes': 200, 'upload_payload_bytes': 400,
                          'compressed_blocks': 3, 'raw_blocks': 1,
                          'codec_blocks': {'LZ4': 1, 'HS': 1, 'ZF': 1, 'N/A': 1}})
        self.assertEqual(completion_columns(job),
                         ('1m 03s', 'LZ4+HS+ZF+N/A · 2.00:1 (50% saved)'))
        self.assertIn('last resumed portion', completion_report(job))
        self.assertEqual(completion_columns(job, 'Running'), ('', ''))

    def test_download_and_raw_or_empty_files(self):
        job = Job('src', 'dst', Metadata(1024, 1, 1, 32), 'receive',
                  status='Complete', elapsed_seconds=2,
                  result={'resumed_bytes': 0, 'download_payload_bytes': 200,
                          'compressed_blocks': 1, 'raw_blocks': 1,
                          'codec_blocks': {'ZF': 1, 'N/A': 1}})
        self.assertEqual(completion_columns(job)[1],
                         'ZF+N/A · 5.12:1 (80% saved)')
        job.result = {'resumed_bytes': 0, 'download_payload_bytes': 1030,
                      'compressed_blocks': 0, 'raw_blocks': 1,
                      'codec_blocks': {'ZF': 0, 'N/A': 1}}
        self.assertEqual(completion_columns(job)[1], 'N/A')
        job.meta = Metadata(0, 1, 1, 32)
        job.result = {'resumed_bytes': 0, 'download_payload_bytes': 0,
                      'compressed_blocks': 0, 'raw_blocks': 0,
                      'codec_blocks': {'ZF': 0, 'N/A': 0}}
        self.assertEqual(completion_columns(job)[1], 'N/A · Empty file')

    def test_compression_sort_keeps_raw_download_at_ratio_one(self):
        raw = Job('a', 'b', Metadata(100, 1, 1, 32), 'receive', status='Complete',
                  result={'download_payload_bytes': 103, 'resumed_bytes': 0,
                          'compressed_blocks': 0, 'codec_blocks': {'N/A': 1}})
        packed = Job('c', 'd', Metadata(100, 1, 1, 32), 'send', status='Complete',
                     result={'upload_payload_bytes': 50, 'resumed_bytes': 0,
                             'compressed_blocks': 1, 'codec_blocks': {'LZ4': 1}})

        class FakeTree:
            _labels = {'compression': 'Compression'}
            _directories = set()
            order = ['job:1', 'job:0']

            def get_children(self): return self.order[:]
            def set(self, row, column): return completion_columns((raw, packed)[int(row[-1])])[1]
            def move(self, row, _parent, position):
                self.order.remove(row)
                self.order.insert(position, row)
            def heading(self, *_args, **_kwargs): pass

        tree = FakeTree()
        App.sort_table(SimpleNamespace(plan=SimpleNamespace(jobs=[raw, packed])),
                       tree, 'compression', False)
        self.assertEqual(tree.order, ['job:0', 'job:1'])


if __name__ == '__main__':
    unittest.main()
