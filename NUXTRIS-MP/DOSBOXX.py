#!/usr/bin/env python3
"""NuXTris DOSBox-X serial launcher. Original code, MIT. Linux stdlib only.

DOSBox-X uses a local transparent socket; this helper owns the real UART.
Linux/FTDI handles physical RTS/CTS without synchronous USB status polling
inside the emulator. Byte streams and the game's CRC are left intact.
"""
import argparse
import array
import fcntl
import json
import os
from pathlib import Path
import select
import socket
import stat
import subprocess
import tempfile
import termios
import time

LIMIT = 16384
BAUDS = (9600, 19200, 38400, 57600, 115200)


def preferences(root):
    """Local launcher preferences only; never executable config content."""
    try:
        data = json.loads((root/'NXTUSB.JSON').read_text())
        if not isinstance(data, dict):
            return {}
        return {k: v for k, v in data.items() if
                (k == 'device' and isinstance(v, str) and v.startswith('/dev/')) or
                (k == 'baud' and type(v) is int and v in BAUDS) or
                (k == 'flow' and v in ('rtscts', 'none'))}
    except (OSError, ValueError):
        return {}


def remember(root, device, baud, flow):
    tmp = root/'NXTUSB.NEW'
    try:
        tmp.write_text(json.dumps(dict(device=device, baud=baud, flow=flow))+'\n')
        tmp.replace(root/'NXTUSB.JSON')
    except OSError as exc:
        print(f'Could not save adapter preferences: {exc}', flush=True)


def relay(conn, fd, running):
    """Bounded, lossless buffers, partial writes and independent directions."""
    conn.setblocking(False)
    to_uart, to_guest = bytearray(), bytearray()
    while running():
        readers = ([conn] if len(to_uart) < LIMIT else [])
        if len(to_guest) < LIMIT:
            readers.append(fd)
        writers = ([fd] if to_uart else []) + ([conn] if to_guest else [])
        ready_r, ready_w, _ = select.select(readers, writers, [], .05)
        if conn in ready_r:
            try:
                chunk = conn.recv(min(4096, LIMIT-len(to_uart)))
                if not chunk:
                    break
                to_uart.extend(chunk)
            except BlockingIOError:
                pass
        if fd in ready_r:
            try:
                chunk = os.read(fd, min(4096, LIMIT-len(to_guest)))
                if not chunk:
                    raise OSError('Serial device disconnected')
                to_guest.extend(chunk)
            except BlockingIOError:
                pass
        if fd in ready_w:
            try:
                n = os.write(fd, to_uart)
                del to_uart[:n]
            except BlockingIOError:
                pass
        if conn in ready_w:
            try:
                n = conn.send(to_guest)
                del to_guest[:n]
            except BlockingIOError:
                pass

    # Preserve the final queued control bytes on an orderly guest shutdown.
    # Bound the wait so a cable with CTS held low cannot trap the launcher.
    deadline = time.monotonic() + .5
    while to_uart:
        wait = deadline - time.monotonic()
        if wait <= 0:
            raise RuntimeError('Serial shutdown timed out with unsent bytes.')
        if select.select([], [fd], [], wait)[1]:
            try:
                n = os.write(fd, to_uart)
                del to_uart[:n]
            except BlockingIOError:
                pass


def close_uart(fd, old, exclusive):
    """Only remove our own exclusivity; close even if restoration fails."""
    try:
        try:
            if old is not None:
                termios.tcsetattr(fd, termios.TCSANOW, old)
        finally:
            if exclusive:
                fcntl.ioctl(fd, termios.TIOCNXCL)
    finally:
        os.close(fd)


def main():
    root = Path(__file__).resolve().parent
    saved = preferences(root)
    parser = argparse.ArgumentParser(description='Launch NuXTris using a USB serial cable.')
    parser.add_argument('device', nargs='?', default=saved.get('device'), help='serial device; remembered after first use')
    parser.add_argument('role', nargs='?', choices=['host', 'join', 'sudden'], help='optional menu preset')
    parser.add_argument('--baud', type=int, choices=BAUDS, default=saved.get('baud', 38400))
    parser.add_argument('--flow', choices=['rtscts', 'none'], default=saved.get('flow', 'rtscts'))
    parser.add_argument('--name', help='optional name preset; otherwise use the game menu')
    args = parser.parse_args()
    name = args.name.upper() if args.name is not None else None
    if name is not None and not (1 <= len(name) <= 8 and name.isascii() and name.isalnum()):
        parser.error('Name must contain 1-8 letters or digits.')
    if not args.device:
        parser.error('Choose a device once, for example: sh DOSBOXX.SH /dev/ttyUSB0')
    if not stat.S_ISCHR(os.stat(args.device).st_mode):
        parser.error('Device must be a serial character device.')
    if not (root/'NXTNET.EXE').exists():
        parser.error('NXTNET.EXE must be beside this launcher.')
    fd = os.open(args.device, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    old = None
    exclusive = False
    proc = None
    try:
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        fcntl.ioctl(fd, termios.TIOCEXCL)
        exclusive = True
        old = termios.tcgetattr(fd)
        attrs = termios.tcgetattr(fd)
        attrs[0] = attrs[1] = attrs[3] = 0
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        if args.flow == 'rtscts':
            attrs[2] |= termios.CRTSCTS
        attrs[4] = attrs[5] = getattr(termios, 'B'+str(args.baud))
        attrs[6][termios.VMIN] = 1
        attrs[6][termios.VTIME] = 0
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        termios.tcflush(fd, termios.TCIOFLUSH)
        fcntl.ioctl(fd, termios.TIOCMBIS, array.array('i', [termios.TIOCM_DTR | termios.TIOCM_RTS]))
        remember(root, args.device, args.baud, args.flow)
        with socket.socket() as listener, tempfile.TemporaryDirectory(prefix='nuxtris-serial-') as tmp:
            listener.bind(('127.0.0.1', 0))
            listener.listen(1)
            listener.settimeout(.2)
            port = listener.getsockname()[1]
            command = '/BRIDGE /COM:1'
            command += f' /BAUD:{args.baud}'
            command += ' /RTS' if args.flow == 'rtscts' else ' /3WIRE'
            if args.role:
                command += ' /JOIN' if args.role == 'join' else ' /HOST'
            if args.role == 'sudden':
                command += ' /SUDDEN'
            if name:
                command += f' /NAME:{name}'
            conf = Path(tmp)/'serial.conf'
            conf.write_text(f'''[sdl]
output=surface
[dosbox]
machine=svga_s3
memsize=4
[cpu]
core=normal
cycles=fixed 12000
[serial]
serial1=nullmodem server:127.0.0.1 port:{port} transparent:1 txdelay:1 rxdelay:1000
serial2=disabled
[autoexec]
mount c "{root}"
c:
NXTNET {command}
exit
''')
            print(f'{args.device}: {args.baud} baud, {args.flow}. Close this DOSBox-X window when finished.', flush=True)
            proc = subprocess.Popen(['dosbox-x', '-conf', str(conf)])
            while proc.poll() is None:
                try:
                    conn, _ = listener.accept()
                    break
                except socket.timeout:
                    continue
            else:
                raise RuntimeError('DOSBox-X exited before opening its serial connection.')
            with conn:
                conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                relay(conn, fd, lambda: proc.poll() is None)
            proc.wait(timeout=5)
    finally:
        if proc is not None and proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        close_uart(fd, old, exclusive)


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
        raise SystemExit(f'Serial launcher: {exc}')
    except KeyboardInterrupt:
        raise SystemExit(130)
