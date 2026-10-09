"""Tests use synthetic bytes only; no commercial game files are required."""
import contextlib
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import patch as tool


class PatchTests(unittest.TestCase):
    def fixture(self):
        data = b'example'+b'\xcd\x2f'+b'payload'
        expected = data[:7]+b'\x30\xc0'+data[9:]
        spec = dict(size=len(data), sha256=tool.digest(data), output='SETUPNX.EXE',
                    offsets=(7,), patched_sha256=tool.digest(expected))
        return data, expected, spec

    def test_exact_change(self):
        original, expected, spec = self.fixture()
        self.assertEqual(tool.patch_bytes(original, spec), expected)
        self.assertEqual(original[7:9], b'\xcd\x2f')

    def combined_fixture(self):
        # Synthetic instruction snippets, not an embedded commercial executable.
        data = b'header' + b'\xcd\x2f' * 2 + b'\x74\x07\x75\x69' + b'end'
        expected = b'header' + b'\x30\xc0' * 2 + b'\x90\x90\xeb\x69' + b'end'
        spec = dict(size=len(data), sha256=tool.digest(data), output='DUNE2FX.EXE',
                    offsets=(6, 8), extra_patches=((10, b'\x74\x07', b'\x90\x90'),
                    (12, b'\x75\x69', b'\xeb\x69')), patched_sha256=tool.digest(expected))
        return data, expected, spec

    def test_combined_keeps_both_xms_fixes(self):
        data, expected, spec = self.combined_fixture()
        self.assertEqual(tool.patch_bytes(data, spec), expected)
        self.assertEqual(data[6:10], b'\xcd\x2f' * 2)

    def test_manual_check_opcode_guard(self):
        data, _, spec = self.combined_fixture()
        spec['extra_patches'] = ((10, b'\x75\x07', b'\x90\x90'),)
        with self.assertRaises(ValueError): tool.patch_bytes(data, spec)

    def test_patch_cannot_change_file_size(self):
        data, _, spec = self.combined_fixture()
        spec['extra_patches'] = ((10, b'\x74\x07', b'\x90'),)
        with self.assertRaises(ValueError): tool.patch_bytes(data, spec)

    def test_unknown_version_rejected(self):
        data, _, spec = self.fixture()
        for bad in (data+b'!', b'X'+data[1:]):
            with self.assertRaises(ValueError): tool.patch_bytes(bad, spec)

    def test_opcode_guard(self):
        data, _, spec = self.fixture()
        spec['offsets'] = (0,)
        with self.assertRaises(ValueError): tool.patch_bytes(data, spec)

    def test_output_checksum_guard(self):
        data, _, spec = self.fixture()
        spec['patched_sha256'] = '0'*64
        with self.assertRaises(ValueError): tool.patch_bytes(data, spec)

    def test_cli_no_overwrite_and_check(self):
        data, expected, spec = self.fixture()
        with tempfile.TemporaryDirectory() as temp, patch.object(tool, 'SPECS', {'SETUP.EXE': spec}):
            root = Path(temp); game = root/'game'; game.mkdir()
            source = game/'setup.exe'; source.write_bytes(data)
            out = root/'patched'
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(tool.main([str(game), '--check']), 0)
                self.assertFalse(out.exists())
                self.assertEqual(tool.main([str(game), '--output', str(out)]), 0)
                self.assertEqual(tool.main([str(game), '--output', str(out)]), 1)
                self.assertEqual(tool.main([str(game), '--output', str(game)]), 1)
            self.assertEqual(source.read_bytes(), data)
            self.assertEqual((out/'SETUPNX.EXE').read_bytes(), expected)

    def test_all_inputs_validated_before_output(self):
        data, _, spec = self.fixture()
        with tempfile.TemporaryDirectory() as temp, patch.object(tool, 'SPECS', {'SETUP.EXE': spec, 'DUNE2.EXE': spec}):
            root = Path(temp); (root/'SETUP.EXE').write_bytes(data)
            out = root/'patched'
            with contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(tool.main([str(root), '--output', str(out)]), 1)
            self.assertFalse(out.exists())

    def test_ambiguous_case_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root/'SETUP.EXE').write_bytes(b'a'); (root/'setup.exe').write_bytes(b'a')
            with self.assertRaises(ValueError): tool.find_input(root, 'SETUP.EXE')


if __name__ == '__main__': unittest.main()
