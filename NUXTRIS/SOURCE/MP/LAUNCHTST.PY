"""Focused regressions for relay EOF and serial ownership cleanup."""
import termios
import tempfile
from pathlib import Path
import unittest
from unittest.mock import Mock, patch
import DOSBOXX as launcher


class LauncherChecks(unittest.TestCase):
    def test_adapter_preferences_roundtrip_and_validation(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            launcher.remember(root, '/dev/ttyUSB0', 57600, 'none')
            self.assertEqual(launcher.preferences(root),
                dict(device='/dev/ttyUSB0', baud=57600, flow='none'))
            (root/'NXTUSB.JSON').write_text('{"baud": 123, "flow": "bad", "device": "relative"}')
            self.assertEqual(launcher.preferences(root), {})
            (root/'NXTUSB.JSON').write_text('broken')
            self.assertEqual(launcher.preferences(root), {})

    def test_cleanup_does_not_unlock_someone_elses_port(self):
        with patch.object(launcher.fcntl, 'ioctl') as ioctl, \
             patch.object(launcher.os, 'close') as close:
            launcher.close_uart(42, None, False)
            ioctl.assert_not_called()
            close.assert_called_once_with(42)

    def test_failed_restore_still_releases_and_closes(self):
        with patch.object(launcher.termios, 'tcsetattr', side_effect=OSError('unplugged')), \
             patch.object(launcher.fcntl, 'ioctl') as ioctl, \
             patch.object(launcher.os, 'close') as close:
            with self.assertRaises(OSError):
                launcher.close_uart(42, [], True)
            ioctl.assert_called_once_with(42, termios.TIOCNXCL)
            close.assert_called_once_with(42)

    def test_eof_flushes_queued_tail_with_partial_writes(self):
        conn = Mock()
        payload = bytes(range(256))
        conn.recv.side_effect = [payload, b'']
        sent = bytearray()
        calls = 0

        def ready(readers, writers, errors, timeout):
            nonlocal calls
            calls += 1
            return ([conn], [], []) if calls <= 2 else ([], [42], [])

        def write(fd, data):
            sent.extend(data[:7])
            return min(7, len(data))

        with patch.object(launcher.select, 'select', side_effect=ready), \
             patch.object(launcher.os, 'write', side_effect=write):
            launcher.relay(conn, 42, lambda: True)
        self.assertEqual(sent, payload)


if __name__ == '__main__':
    unittest.main()
