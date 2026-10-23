"""Peer exit, adapter removal and cancellation, using only private PTYs/fakes."""
import errno
import os
import pty
import sys
import termios
import threading
import time
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'linux'))
from link import SerialPort, Client, Cancelled, INFO, CANCEL
import test_connection as connection


class ShutdownTests(unittest.TestCase):
    def open_port(self, **kwargs):
        master, slave = pty.openpty()
        self.addCleanup(os.close, slave)
        self.addCleanup(os.close, master)
        port = SerialPort(os.ttyname(slave), 115200, **kwargs)
        self.addCleanup(port.close)
        return port

    def test_termios_eio_still_closes_fd_and_is_idempotent(self):
        port = self.open_port()
        fd = port.fd
        error = termios.error(errno.EIO, 'Input/output error')
        with patch('link.termios.tcflush', side_effect=error), \
             patch('link.termios.tcsetattr', side_effect=error):
            port.close()
        self.assertIsNone(port.fd)
        with self.assertRaises(OSError):os.fstat(fd)
        port.close()

    def test_close_syscall_failure_cannot_retry_stale_fd(self):
        port = self.open_port()
        real_close = os.close
        def close_then_error(fd):
            real_close(fd)
            raise OSError(errno.EIO, 'Input/output error')
        with patch('link.os.close', side_effect=close_then_error) as close:
            with self.assertRaises(OSError):port.close()
            port.close()
            self.assertEqual(close.call_count, 1)

    def test_silent_server_abort_has_short_budget_and_one_attempt(self):
        clock = [0.]
        class SilentPort:
            writes = []
            def write(self, data, timeout):
                self.writes.append((data[2], timeout))
                clock[0] += timeout
            def read(self, timeout):
                clock[0] += timeout
                return b''
        port = SilentPort(); client = Client(port, timeout=40, attempts=5)
        with patch('link.time.monotonic', side_effect=lambda:clock[0]):
            self.assertFalse(client.abort())
        self.assertEqual(port.writes, [(CANCEL, .5)])
        self.assertLessEqual(clock[0], 1.001)
        self.assertEqual((client.timeout,client.attempts), (40,5))

    def test_disconnect_interrupts_no_reply_without_retrying(self):
        port = self.open_port(); client = Client(port, timeout=40)
        timer = threading.Timer(.05, client.stopping.set)
        timer.start()
        try:
            started = time.monotonic()
            with self.assertRaises(Cancelled):client.request(INFO)
            self.assertLess(time.monotonic()-started, .8)
            self.assertEqual(client.retries, 0)
            self.assertFalse(client.abort())
        finally:timer.join()

    def test_disconnect_interrupts_low_peer_signals(self):
        port = self.open_port(peer='both')
        timer = threading.Timer(.05, port.interrupted.set);timer.start()
        try:
            with patch.object(port, 'modem_status', return_value=0):
                started = time.monotonic()
                with self.assertRaises(Cancelled):port.write(b'test', 40)
                self.assertLess(time.monotonic()-started, .8)
        finally:timer.join()

    def test_disconnect_interrupts_blocked_write(self):
        port = self.open_port()
        def blocked(read, write, errors, timeout):
            self.assertLessEqual(timeout, .1)
            port.interrupted.set()
            return [], [], []
        with patch('link.select.select', side_effect=blocked),patch('link.os.write') as write:
            with self.assertRaises(Cancelled):port.write(b'test', 40)
            write.assert_not_called()


class SettingsShutdownTests(unittest.TestCase):
    # Reuse the baud-aware port and fake clock without duplicating its suite.
    setUp = connection.Connections.setUp
    tearDown = connection.Connections.tearDown
    make = connection.Connections.make
    def test_disconnect_during_settings_does_not_wait_for_rollback(self):
        port, client = self.make()
        original_write = port.write
        def write(data, timeout):
            original_write(data, timeout)
            from link import SETTINGS
            if data[2] == SETTINGS:client.stopping.set()
        port.write = write
        with self.assertRaises(Cancelled):client.apply_settings(115200)
        self.assertLess(self.clock.value, 1)
        self.assertEqual(client.attempts, 5)

    def test_disconnect_during_recovery_does_not_wait_for_rollback(self):
        port, client = self.make()
        port.drop['all_confirm'] = True
        original_wait = client.stopping.wait
        def wait(seconds):
            if seconds > 1:client.stopping.set()
            return original_wait(0 if client.stopping.is_set() else seconds)
        client.stopping.wait = wait
        with self.assertRaises(Cancelled):client.apply_settings(115200)
        self.assertLess(self.clock.value, 10)
