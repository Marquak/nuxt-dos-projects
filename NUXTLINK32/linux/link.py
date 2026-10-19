"""NuXT Link protocol and Linux serial transport. Python 3 standard library only."""
from __future__ import annotations
import binascii
import fcntl
import glob
import os
from pathlib import Path
import re
import select
import struct
import json
import hashlib
import datetime
import stat
from dataclasses import dataclass, asdict
import termios
import threading
import time
import secrets
import socket

BAUDS = (1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200)
MAX_PAYLOAD = 4096
BLOCK_SIZES = (128, 256, 512, 1024, 1440, 1464, 2048, 4096)
LAN_PORT = 47588
MAGIC = b"NX"
HELLO, FIRST, NEXT = 1, 2, 3
PUT_BEGIN, PUT_DATA, PUT_END = 0x10, 0x11, 0x12
PUT_COMPRESSED = 0x14
PUT_VERIFY = 0x15
GET_BEGIN, GET_DATA, GET_END = 0x20, 0x21, 0x22
CANCEL, ACK, ERROR = 0x30, 0x80, 0xFF
STAT, MKDIR, META, HASH, HASH_STEP = 4, 5, 6, 7, 8
PUT_STATUS, GET_STATUS = 0x13, 0x24
INFO, SETTINGS, CONFIRM, DELETE, RENAME = 0x40, 0x41, 0x42, 0x50, 0x51
CLOCK, GOODBYE = 0x43, 0x44
FLOWS = ('none', 'rtscts')
PEERS = {'none': 0, 'dsr': 32, 'dcd': 128, 'both': 160}
MAX_FILE = 0x7FFFFFFF

class LinkError(Exception):
    pass
class RemoteError(LinkError):
    pass
class VersionError(LinkError):
    pass
class Cancelled(LinkError):
    pass


def crc(data: bytes, initial: int = 0xFFFF) -> int:
    return binascii.crc_hqx(data, initial)


def valid_name(name: str) -> str:
    # Preserve OEM bytes returned by DOS. Only ASCII case folding is portable
    # across the DOS code-page tables; do not Unicode-uppercase CP437 names.
    name = ''.join(c.upper() if 'a' <= c <= 'z' else c for c in name)
    try:
        raw = name.encode('cp437')
    except UnicodeEncodeError:
        raise ValueError('DOS filenames must be representable in CP437.') from None
    parts = raw.split(b'.')
    if (len(parts) > 2 or not 1 <= len(parts[0]) <= 8 or
        len(parts) == 2 and not 1 <= len(parts[1]) <= 3 or
        any(c < 33 or c in b'"*/:<>?\\|+,;=[]' for c in raw)):
        raise ValueError('Use a DOS 8.3 name without spaces, wildcards or path separators.')
    base = name.split('.')[0]
    if base in {'CON', 'PRN', 'AUX', 'NUL', 'CLOCK$'} or re.fullmatch(r'(COM|LPT)[0-9]', base):
        raise ValueError('DOS device names cannot be transferred.')
    return name


def dos_key(name: str) -> str:
    # Unlike Unicode casefold, DOS never expands one byte (e.g. ß) into two.
    result = []
    for ch in name:
        upper = ch.upper()
        try:
            representable = len(upper) == 1 and len(upper.encode('cp437')) == 1
        except UnicodeEncodeError:
            representable = False
        result.append(upper if representable else ch)
    return ''.join(result)


def suggest_name(name: str) -> str:
    try:
        return valid_name(name)
    except ValueError:
        p = Path(name)
        clean = lambda s: re.sub(r'[^A-Z0-9_-]', '_', s.upper())
        base, ext = clean(p.stem)[:8] or 'FILE', clean(p.suffix[1:])[:3]
        candidate = base + ('.'+ext if ext else '')
        try:
            return valid_name(candidate)
        except ValueError:
            return 'FILE' + ('.'+ext if ext else '')


def dos_path(path: str) -> str:
    path = path.replace('/', '\\')
    if not path.startswith('\\'):
        path = '\\' + path
    if path != '\\':
        path = '\\' + '\\'.join(valid_name(p) for p in path[1:].split('\\'))
    if len(path.encode('cp437')) > 62:
        raise ValueError('DOS path exceeds 62 bytes (excluding drive letter).')
    return path


def wire_path(path: str) -> bytes:
    return dos_path(path).encode('cp437') + b'\0'


def join_path(parent: str, name: str) -> str:
    return dos_path(parent.rstrip('\\') + '\\' + valid_name(name))


@dataclass(frozen=True)
class Metadata:
    size: int
    date: int
    time: int
    attr: int

    def pack(self):
        if not 0 <= self.size <= MAX_FILE or self.attr & ~0x37:
            raise ValueError('Unsupported DOS file size or attributes')
        return struct.pack('<IHHB', self.size, self.date, self.time, self.attr)

    @classmethod
    def unpack(cls, data):
        if len(data) != 9:
            raise LinkError('Invalid DOS metadata response')
        item = cls(*struct.unpack('<IHHB', data))
        item.pack()
        return item

    @property
    def is_dir(self):
        return bool(self.attr & 16)

    @property
    def timestamp(self):
        try:
            return datetime.datetime(1980+(self.date >> 9), (self.date >> 5)&15,
                self.date&31, self.time >> 11, (self.time >> 5)&63, (self.time&31)*2).timestamp()
        except (ValueError, OverflowError, OSError):
            return None  # Still preserve invalid/unset raw DOS dates in metadata.

    @property
    def display_time(self):
        value = self.timestamp
        return datetime.datetime.fromtimestamp(value).strftime('%Y-%m-%d %H:%M:%S') if value is not None else f'DOS {self.date:04X}:{self.time:04X}'

    @property
    def attributes(self):
        return ''.join(c if self.attr & bit else '-' for bit, c in ((16,'D'),(1,'R'),(2,'H'),(4,'S'),(32,'A')))


@dataclass(frozen=True)
class Entry:
    name: str
    meta: Metadata
    internal: bool = False


META_FORMAT = 'NuXT Link local metadata 2'


def atomic_json(path, value):
    path = Path(path)
    temp = path.with_name(path.name + '.new')
    with temp.open('w', encoding='utf8') as out:
        json.dump(value, out, indent=2, ensure_ascii=True)
        out.write('\n')
        out.flush()
        os.fsync(out.fileno())
    os.replace(temp, path)


def metadata_dir(parent):
    Path(parent).mkdir(parents=True, exist_ok=True)
    folder = Path(parent) / '.nuxtlink'
    marker = folder / 'owner.json'
    if folder.exists():
        if folder.is_symlink() or not marker.is_file() or json.loads(marker.read_text()).get('format') != META_FORMAT:
            raise LinkError(f'{folder} is not a NuXT Link metadata folder.')
    else:
        folder.mkdir()
        atomic_json(marker, {'format': META_FORMAT})
    return folder


def local_metadata(path):
    path = Path(path)
    st = path.stat()
    raw = None
    stored = path.parent / '.nuxtlink' / 'files.json'
    if stored.is_file():
        document = json.loads(stored.read_text())
        if document.get('format') == META_FORMAT:
            raw = document.get('files', {}).get(path.name)
    if raw is None:
        try:
            raw = json.loads(os.getxattr(path, 'user.nuxtlink.dos'))
        except (OSError, ValueError):
            pass
    if raw:
        previous = Metadata(**raw['dos'])
        attr = (previous.attr & ~16) | (16 if path.is_dir() else 0)
        if raw.get('mtime_ns') == st.st_mtime_ns:
            return Metadata(0 if path.is_dir() else st.st_size, previous.date, previous.time, attr)
    else:
        attr = (16 if path.is_dir() else 32) | (0 if st.st_mode & 0o222 else 1)
    dt = datetime.datetime.fromtimestamp(st.st_mtime)
    if not 1980 <= dt.year <= 2107:
        raise ValueError(f'{path}: modification date is outside DOS range 1980–2107.')
    return Metadata(0 if path.is_dir() else st.st_size, (dt.year-1980)<<9 | dt.month<<5 | dt.day,
                    dt.hour<<11 | dt.minute<<5 | dt.second//2, attr)


def preserve_metadata(path, meta: Metadata):
    path = Path(path)
    if meta.timestamp is not None:
        os.utime(path, (meta.timestamp, meta.timestamp))
    # DOS attributes are stored losslessly; don't turn Linux directories
    # read-only, since DOS's R bit does not restrict traversal/creation there.
    if not meta.is_dir:
        mode = stat.S_IMODE(path.stat().st_mode)
        os.chmod(path, mode & ~0o222 if meta.attr & 1 else mode | stat.S_IWUSR)
    value = {'dos': asdict(meta), 'mtime_ns': path.stat().st_mtime_ns}
    folder = metadata_dir(path.parent)
    index = folder / 'files.json'
    document = json.loads(index.read_text()) if index.exists() else {'format': META_FORMAT, 'files': {}}
    if document.get('format') != META_FORMAT:
        raise LinkError(f'Invalid local metadata index: {index}')
    document['files'][path.name] = value
    atomic_json(index, document)
    try:
        os.setxattr(path, 'user.nuxtlink.dos', json.dumps(value).encode())
    except OSError:
        pass  # Sidecar index is authoritative on filesystems without xattrs.


def checksums(path, limit=None, progress=None):
    checksum, digest, done = 0xffff, hashlib.sha256(), 0
    with Path(path).open('rb') as stream:
        while limit is None or done < limit:
            block = stream.read(65536 if limit is None else min(65536, limit-done))
            if not block:
                break
            checksum = crc(block, checksum)
            digest.update(block)
            done += len(block)
            if progress is not None:
                progress(done, limit)
    return done, checksum, digest.hexdigest()


def ports() -> list[str]:
    found = glob.glob('/dev/serial/by-id/*') + glob.glob('/dev/ttyUSB*') + glob.glob('/dev/ttyACM*') + glob.glob('/dev/ttyS*')
    seen, result = set(), []
    for p in sorted(found, key=lambda p: (not p.startswith('/dev/serial/'), p)):
        real = os.path.realpath(p)
        if real not in seen:
            seen.add(real)
            result.append(p)
    return result


def encode(kind: int, seq: int, payload: bytes = b'') -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ValueError('Frame too large')
    body = struct.pack('<BBH', kind, seq, len(payload)) + payload
    return MAGIC + body + struct.pack('<H', crc(body))


class Parser:
    def __init__(self):
        self.buffer = bytearray()
        self.bad_crc = 0

    def feed(self, data: bytes):
        self.buffer.extend(data)

    def pop(self):
        while True:
            i = self.buffer.find(MAGIC)
            if i < 0:
                self.buffer[:] = self.buffer[-1:] if self.buffer.endswith(b'N') else b''
                return None
            if i:
                del self.buffer[:i]
            if len(self.buffer) < 6:
                return None
            kind, seq, length = struct.unpack_from('<BBH', self.buffer, 2)
            if length > MAX_PAYLOAD:
                del self.buffer[0]
                continue
            size = 8 + length
            if len(self.buffer) < size:
                return None
            body = bytes(self.buffer[2:6+length])
            check, = struct.unpack_from('<H', self.buffer, 6+length)
            if crc(body) != check:
                self.bad_crc += 1
                del self.buffer[0]
                continue
            data = bytes(self.buffer[6:6+length])
            del self.buffer[:size]
            return kind, seq, data


class SerialPort:
    def __init__(self, path: str, baud: int, flow="none", peer="none", *, interrupted=None):
        if baud not in BAUDS or flow not in FLOWS or peer not in PEERS:
            raise ValueError('Unsupported baud rate')
        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        self.old = None
        self.exclusive = False
        self.interrupted = interrupted if interrupted is not None else threading.Event()
        self.path, self.baud, self.flow, self.peer = path, baud, flow, peer
        try:
            fcntl.flock(self.fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            if hasattr(termios, 'TIOCEXCL'):
                fcntl.ioctl(self.fd, termios.TIOCEXCL)
                self.exclusive = True
            self.old = termios.tcgetattr(self.fd)
            attrs = termios.tcgetattr(self.fd)
            attrs[0] = 0
            attrs[1] = 0
            attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL | (termios.CRTSCTS if flow == "rtscts" else 0)
            attrs[3] = 0
            attrs[4] = attrs[5] = getattr(termios, 'B'+str(baud))
            attrs[6][termios.VMIN] = 0
            attrs[6][termios.VTIME] = 0
            termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
            termios.tcflush(self.fd, termios.TCIOFLUSH)
            try:
                fcntl.ioctl(self.fd, termios.TIOCMBIS,
                            struct.pack('I', termios.TIOCM_DTR | termios.TIOCM_RTS))
            except OSError:
                pass  # PTYs and some USB drivers have no modem-control ioctl.
        except BaseException:
            self.close()
            raise

    def read(self, timeout: float) -> bytes:
        self.check_interrupted()
        ready, _, _ = select.select([self.fd], [], [], max(0, min(.1, timeout)))
        self.check_interrupted()
        if not ready:
            return b''
        try:
            data = os.read(self.fd, 4096)
        except BlockingIOError:
            return b''
        if not data:
            raise LinkError('Serial device disconnected')
        return data

    def write(self, data: bytes, timeout: float):
        self.check_interrupted()
        end = time.monotonic()+timeout
        mask = (termios.TIOCM_DSR if self.peer in ('dsr', 'both') else 0) | (termios.TIOCM_CAR if self.peer in ('dcd', 'both') else 0)
        while mask:
            self.check_interrupted()
            state = self.modem_status()
            if state is None:
                raise LinkError('This serial device cannot report the required DSR/DCD signals.')
            if state & mask == mask:
                break
            if time.monotonic() >= end:
                raise TimeoutError('Required DSR/DCD signal is low. Check cable and DOS readiness.')
            self.interrupted.wait(.005)
        view = memoryview(data)
        while view:
            self.check_interrupted()
            remaining = end-time.monotonic()
            if remaining <= 0:
                raise TimeoutError('Serial write timed out')
            _, ready, _ = select.select([], [self.fd], [], min(.1, remaining))
            self.check_interrupted()
            if ready:
                try:
                    n = os.write(self.fd, view)
                except BlockingIOError:
                    continue
                view = view[n:]

    def check_interrupted(self):
        if self.interrupted.is_set():
            raise Cancelled('Disconnect requested; resumable files retained.')

    def configure(self, baud, flow='none', peer='none'):
        if baud not in BAUDS or flow not in FLOWS or peer not in PEERS:
            raise ValueError('Unsupported serial settings')
        attrs = termios.tcgetattr(self.fd)
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL | (termios.CRTSCTS if flow == 'rtscts' else 0)
        attrs[4] = attrs[5] = getattr(termios, 'B'+str(baud))
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        self.baud, self.flow, self.peer = baud, flow, peer

    def discard(self):
        """Only during idle discovery/speed changes, never during a transfer."""
        termios.tcflush(self.fd, termios.TCIOFLUSH)

    def modem_status(self):
        try:
            return struct.unpack('I', fcntl.ioctl(self.fd, termios.TIOCMGET, struct.pack('I', 0)))[0]
        except OSError:
            return None

    def close(self):
        fd, self.fd = self.fd, None
        if fd is not None:
            try:
                if self.old is not None:
                    # Do not leave flow-stalled bytes queued at the old baud.
                    try:termios.tcflush(fd, termios.TCOFLUSH)
                    except (OSError, termios.error):pass
                    try:termios.tcsetattr(fd, termios.TCSANOW, self.old)
                    except (OSError, termios.error):pass
                if self.exclusive and hasattr(termios, 'TIOCNXCL'):
                    try:fcntl.ioctl(fd, termios.TIOCNXCL)
                    except OSError:pass
            finally:
                # termios.error is NOT an OSError. Always release the descriptor,
                # even when an unplugged adapter rejects settings restoration.
                os.close(fd)


class LanPort:
    """One complete protocol frame per unfragmented IPv4/UDP datagram."""
    mode = 'lan'

    def __init__(self, address: str, *, interrupted=None):
        try:
            socket.inet_pton(socket.AF_INET, address)
        except OSError as exc:
            raise ValueError('Enter the DOS machine IPv4 address.') from exc
        self.interrupted = interrupted if interrupted is not None else threading.Event()
        self.address = address
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            self.sock.setblocking(False)
            self.sock.connect((address, LAN_PORT))
        except BaseException:
            self.close()
            raise

    def check_interrupted(self):
        if self.interrupted.is_set():
            raise Cancelled('Disconnect requested; resumable files retained.')

    def write(self, data: bytes, timeout: float):
        if len(data) > 1472:
            raise ValueError('LAN packet exceeds the DOS receive limit.')
        end = time.monotonic() + timeout
        while True:
            self.check_interrupted()
            remaining = end - time.monotonic()
            if remaining <= 0:
                raise TimeoutError('LAN send timed out')
            _, ready, _ = select.select([], [self.sock], [], min(.1, remaining))
            if ready:
                sent = self.sock.send(data)
                if sent != len(data):
                    raise LinkError('Incomplete LAN datagram')
                return

    def read(self, timeout: float) -> bytes:
        self.check_interrupted()
        ready, _, _ = select.select([self.sock], [], [], max(0, min(.1, timeout)))
        self.check_interrupted()
        return self.sock.recv(1514) if ready else b''

    def close(self):
        sock, self.sock = self.sock, None
        if sock is not None:
            sock.close()


class Client:
    """Single-worker, stop-and-wait client; retries use the identical sequence."""
    def __init__(self, transport, baud=38400, timeout=None, attempts=5):
        self.transport = transport
        self.timeout = timeout or max(3.0, (512+16)*10/baud + 1.0)
        self.baud, self.flow, self.peer, self.block_size = baud, "none", "none", 128
        self.compress_uploads = True
        self.attempts = attempts
        self.sequence = 0
        self.parser = Parser()
        self.retries = 0
        self.cancelled = threading.Event()
        self.stopping = getattr(transport, 'interrupted', threading.Event())

    def check_stopping(self):
        if self.stopping.is_set():
            raise Cancelled('Disconnect requested; resumable files retained.')

    def request(self, kind: int, payload=b'', *, cancellable=True) -> bytes:
        seq = self.sequence
        self.sequence = (seq+1) & 255
        packet = encode(kind, seq, payload)
        for attempt in range(self.attempts):
            self.check_stopping()
            if cancellable and self.cancelled.is_set():
                raise Cancelled('Transfer cancelled')
            self.transport.write(packet, self.timeout)
            until = time.monotonic()+self.timeout
            while time.monotonic() < until:
                self.check_stopping()
                # Finish the in-flight CRC-checked reply before honoring cancellation.
                frame = self.parser.pop()
                if frame is None:
                    self.parser.feed(self.transport.read(min(0.1, until-time.monotonic())))
                    continue
                response, response_seq, data = frame
                if response_seq != seq:
                    continue  # A delayed duplicate response from an earlier request.
                if response == ERROR:
                    raise RemoteError(data.decode('ascii', errors='replace'))
                if response != ACK:
                    continue
                return data
            # A truncated/noisy reply must not hold the parser behind a false
            # length field forever. A complete reply fits within this timeout.
            self.parser.buffer.clear()
            if attempt + 1 < self.attempts:
                self.retries += 1
        if getattr(self.transport, 'mode', None) == 'lan':
            raise LinkError('No LAN reply. Check NUXNET, the packet driver, MTCPCFG, DOS IP and Linux firewall.')
        raise LinkError('No valid reply. Check the DOS server, matching baud rates, cable, and port. Lower the speed if needed.')

    def connect(self) -> str:
        hello = self.request(HELLO).decode('ascii', errors='replace')
        if not hello.startswith('NUXTLINK/6 '):
            raise VersionError('Install both the NuXT Link 3.3.1 DOS server and Linux client.')
        match = re.fullmatch(r'NUXTLINK/6 ([A-Z]:\\) .+', hello)
        if not match:
            raise LinkError('Invalid server greeting or drive')
        self.drive = match.group(1)
        return hello

    def establish(self, baud, flow='none', peer='none', block_size=128,
                  notify=lambda text: None):
        """Discover an idle server, then confirm the requested settings.

        Servers start at 9600. A still-running server may be at its
        previous speed. Never send break or reset modem lines to force it.
        """
        if baud not in BAUDS or flow not in FLOWS or peer not in PEERS or block_size not in BLOCK_SIZES or block_size in (1024,1440,1464):
            raise ValueError('Unsupported connection settings')
        attempts, timeout = self.attempts, self.timeout
        rates = list(dict.fromkeys((9600, baud, 38400, 115200, 57600, 19200, 4800, 2400, 1200)))
        try:
            self.attempts = 2
            for index, rate in enumerate(rates):
                if self.cancelled.is_set():
                    raise Cancelled('Connection cancelled')
                # Previous wrong-speed probes can leave a partial DOS frame.
                # Its bounded receive timeout must expire before another rate.
                if index and self.cancelled.wait(1.1):
                    raise Cancelled('Connection cancelled')
                self.transport.configure(rate, 'none', 'none')
                if hasattr(self.transport, 'discard'): self.transport.discard()
                self.parser.buffer.clear()
                self.baud, self.flow, self.peer = rate, 'none', 'none'
                self.timeout = max(1.5, 400.0/rate + .8)
                notify(f'Checking DOS server at {rate} baud...')
                # Let USB control transfers and the DOS startup banner settle.
                if self.cancelled.wait(.2): raise Cancelled('Connection cancelled')
                try:
                    hello = self.connect()
                    break
                except (VersionError, Cancelled):
                    raise
                except (LinkError, TimeoutError):
                    continue
            else:
                raise LinkError('No DOS server replied at any supported speed. Check its COM port and cable settings.')
        finally:
            self.attempts, self.timeout = attempts, timeout
        notify(f'Server found at {rate}; confirming {baud} baud...')
        info = self.apply_settings(baud, flow, peer, block_size)
        return hello, info

    def diagnostics(self, *, cancellable=True):
        return self._diagnostics(self.request(INFO, cancellable=cancellable))

    def _clock_request(self, payload=b''):
        try:
            data = self.request(CLOCK, payload, cancellable=False)
        except RemoteError as exc:
            if str(exc) == 'Unknown command':
                raise VersionError('Date/time sync needs DOS server 3.2.1 or newer. Update NUXFER and restart it.') from exc
            raise
        try:
            year, month, day, hour, minute, second, hundredth = struct.unpack('<H6B', data)
            if not 1980 <= year <= 2099 or hundredth > 99:
                raise ValueError('Out of range')
            return datetime.datetime(year, month, day, hour, minute, second, hundredth*10000)
        except (ValueError, struct.error) as exc:
            raise LinkError('Invalid DOS clock readback; check the DOS date/time.') from exc

    def read_clock(self):
        return self._clock_request()

    def sync_clock(self):
        # DOS stores local wall time, with no timezone. Sample in the worker,
        # immediately before transmission, not when the UI button was drawn.
        target = datetime.datetime.now()
        if not 1980 <= target.year <= 2099:
            raise ValueError('Linux date is outside the DOS clock range (1980-2099).')
        started = time.monotonic()
        actual = self._clock_request(struct.pack('<H6B', target.year, target.month, target.day,
            target.hour, target.minute, target.second, target.microsecond//10000))
        if abs((actual-target).total_seconds()) > 2:
            raise LinkError('DOS clock update was not verified; check the DOS date/time.')
        # A cached ACK after a retry describes the original update. Read again
        # with a new sequence to confirm that the clock is still ticking there.
        actual = self.read_clock()
        elapsed = time.monotonic()-started
        if abs((actual-target).total_seconds()-elapsed) > 2:
            raise LinkError('DOS clock differs after sync; check the DOS date/time and try again.')
        return actual

    def _diagnostics(self, data):
        fmt = '<8B6I2IHHB'
        size = struct.calcsize(fmt)
        if len(data) <= size or data[-1:] != b'\0':
            raise LinkError('Invalid server diagnostics')
        fields = ('version', 'com', 'baud_index', 'flow_id', 'peer_mask', 'fifo', 'msr', 'mcr',
                  'crc_errors', 'overruns', 'parity_errors', 'framing_errors', 'breaks', 'flow_timeouts',
                  'free_bytes', 'total_bytes', 'max_payload', 'block_size', 'pending')
        info = dict(zip(fields, struct.unpack(fmt, data[:size])))
        lan = info['com'] == 0
        if (info['version'] != 6 or info['baud_index'] >= len(BAUDS) or info['flow_id'] > 1 or
            info['peer_mask'] not in PEERS.values() or info['com'] not in (0,1,2,3,4) or
            info['block_size'] not in BLOCK_SIZES or
            (lan and (info['block_size'] != 1440 or info['max_payload'] != 1440)) or
            (not lan and (info['block_size'] in (1024,1440,1464) or info['max_payload'] != MAX_PAYLOAD))):
            raise LinkError('Unsupported diagnostics version or settings')
        info['transport'] = 'lan' if lan else 'serial'
        info['baud'] = BAUDS[info['baud_index']]
        info['flow'] = FLOWS[info['flow_id']]
        info['peer'] = next((k for k,v in PEERS.items() if v == info['peer_mask']), 'none')
        info['program'] = dos_path(data[size:-1].decode('cp437'))
        self.baud, self.flow, self.peer, self.block_size = info['baud'], info['flow'], info['peer'], info['block_size']
        return info

    def apply_settings(self, baud, flow='none', peer='none', block_size=128):
        if baud not in BAUDS or flow not in FLOWS or peer not in PEERS or block_size not in BLOCK_SIZES or block_size in (1024,1440,1464):
            raise ValueError('Unsupported connection settings')
        old = self.diagnostics()
        desired = (baud, flow, peer, block_size)
        if desired == (old['baud'], old['flow'], old['peer'], old['block_size']):
            self.transport.configure(baud, flow, peer)
            self.timeout = max(3.0, (max(512,block_size)+16)*10/baud + 1.0)
            return old
        token = secrets.randbits(32)
        packet = struct.pack('<BBBHI', BAUDS.index(baud), FLOWS.index(flow), PEERS[peer], block_size, token)
        attempts, timeout = self.attempts, self.timeout
        self.attempts = 1
        started = time.monotonic()
        prepared = False
        try:
            # A RemoteError rejects the proposal without changing either side.
            try:
                answer = self.request(SETTINGS, packet, cancellable=False)
            except RemoteError:
                raise
            except Cancelled:
                raise
            except (LinkError, TimeoutError):
                answer = None  # Server may already have switched after a lost ACK.
            if answer is not None and answer != packet:
                raise LinkError('Connection-settings acknowledgement mismatch')
            prepared = True
            self.stopping.wait(.28)
            self.check_stopping()
            self.transport.configure(baud, flow, peer)
            if hasattr(self.transport, 'discard'): self.transport.discard()
            self.parser.buffer.clear()
            self.timeout = max(3.0, (max(512,block_size)+16)*10/baud + 1.0)
            self.attempts = 2
            answer = self.request(CONFIRM, struct.pack('<I',token), cancellable=False)
            info = self._diagnostics(answer)
            if desired != (info['baud'], info['flow'], info['peer'], info['block_size']):
                raise LinkError('Server confirmed different settings')
            info['pending'] = 0
            return info
        except Cancelled:
            raise
        except RemoteError:
            if not prepared:
                raise
            failure = 'Server rejected the new settings'
        except (OSError, LinkError) as exc:
            failure = str(exc)
        finally:
            self.attempts = attempts
        # A final ACK may be lost after the server committed. Probe before rollback.
        self.attempts = 1
        try:
            if prepared:
                try:
                    info = self.diagnostics(cancellable=False)
                    if desired == (info['baud'],info['flow'],info['peer'],info['block_size']):
                        if info['pending']:
                            info = self._diagnostics(self.request(CONFIRM,struct.pack('<I',token),cancellable=False))
                        info['pending'] = 0
                        return info
                except Cancelled:
                    raise
                except (OSError, LinkError):
                    pass
            # Unconfirmed changes expire after 218 BIOS ticks (about 12 seconds).
            remaining = 13.5 - (time.monotonic()-started)
            if remaining > 0:
                self.stopping.wait(remaining)
                self.check_stopping()
            self.transport.configure(old['baud'],old['flow'],old['peer'])
            self.parser.buffer.clear()
            self.timeout = timeout
            try:
                self.diagnostics(cancellable=False)
            except (OSError, LinkError) as exc:
                raise LinkError(f'{failure}. Recovery could not be confirmed; check DOS settings before reconnecting.') from exc
            raise LinkError(f'{failure}. Both ends recovered their previous settings.')
        finally:
            self.attempts = attempts

    def delete(self, path, meta, *, protected=False):
        return self.request(DELETE, bytes([bool(protected)]) + meta.pack() + wire_path(path))

    def rename(self, source, destination, meta=None):
        meta = meta or self.stat(source)
        self.request(RENAME, meta.pack()+wire_path(source)+wire_path(destination))
        actual = self.stat(destination)
        if actual != meta:
            raise LinkError('Rename completed but metadata verification did not match')
        return actual

    def abort(self):
        # Pause closes DOS handles but retains upload staging for reconnection.
        if self.stopping.is_set():return False
        old, timeout = self.attempts, self.timeout
        try:
            self.attempts = 1
            # A missing peer must not turn cleanup into another long transfer.
            # At most 0.5s writing and 0.5s waiting for this tiny ACK.
            self.timeout = min(timeout, .5)
            self.request(CANCEL, cancellable=False)
            return True
        except (OSError, termios.error, LinkError):
            return False
        finally:
            self.attempts, self.timeout = old, timeout

    def goodbye(self):
        """Tell the LAN DOS server this client has disconnected; never wait long."""
        if getattr(self.transport, 'mode', None) != 'lan' or self.stopping.is_set():
            return False
        old_attempts, old_timeout = self.attempts, self.timeout
        try:
            self.attempts, self.timeout = 1, min(self.timeout, .5)
            self.request(GOODBYE, cancellable=False)
            return True
        except (OSError, LinkError):
            return False
        finally:
            self.attempts, self.timeout = old_attempts, old_timeout

    def listing(self, path='\\'):
        result, kind, payload = [], FIRST, wire_path(path)
        while True:
            data = self.request(kind, payload)
            if not data:
                return result
            if len(data) < 11 or len(data) > 22 or data[-1:] != b'\0':
                raise LinkError('Invalid directory entry')
            m = bytearray(data[:9])
            internal = bool(m[8] & 0x80)
            m[8] &= 0x7f
            name = data[9:-1].decode('cp437')
            result.append(Entry(name, Metadata.unpack(m), internal))
            kind, payload = NEXT, b''

    def partials(self, discard=False):
        reply = self.request(0x31 if discard else 0x32)
        if len(reply) != 4:
            raise LinkError("Invalid partial-cache response")
        return struct.unpack("<HH", reply)

    def stat(self, path):
        return Metadata.unpack(self.request(STAT, wire_path(path)))

    def mkdir(self, path):
        self.request(MKDIR, wire_path(path))

    def set_metadata(self, path, meta):
        self.request(META, meta.pack()+wire_path(path))

    def scan(self, command, progress=lambda done,total: None):
        while True:
            data = self.request(command)
            if len(data) != 11:
                raise LinkError('Invalid resume/verification response')
            done, total, checksum, ready = struct.unpack('<IIHB', data)
            if done > total or ready not in (0, 1):
                raise LinkError('Invalid scan progress')
            progress(done, total)
            if ready:
                return done, checksum

    def hash(self, path, progress=lambda done,total: None):
        try:
            meta = Metadata.unpack(self.request(HASH, wire_path(path)))
            done, checksum = self.scan(HASH_STEP, progress)
            if done != meta.size:
                raise LinkError('Remote file changed during verification')
            if self.stat(path) != meta:
                raise LinkError('Remote metadata changed during verification')
            return meta, checksum
        except BaseException:
            self.abort()
            raise

    def upload(self, source, path, progress=lambda done,total: None, *, meta=None, overwrite=False,
               phase=lambda label,done,total: None):
        source, path = Path(source), dos_path(path)
        meta = meta or local_metadata(source)
        size, checksum, digest = checksums(source)
        if meta.is_dir or size != meta.size or size > MAX_FILE:
            raise ValueError('Invalid file size or source changed during preparation')
        before = source.stat()
        compressed_blocks = raw_blocks = wire_bytes = 0
        if self.compress_uploads:
            from heatshrink_codec import packet as compress_packet
        try:
            reply = self.request(PUT_BEGIN, meta.pack()+struct.pack('<HB', checksum, overwrite)+wire_path(path))
            if len(reply) != 4:
                raise LinkError('Invalid upload resume response')
            offset, = struct.unpack('<I', reply)
            scanned, prefix = self.scan(PUT_STATUS)
            if offset > size or scanned != offset or checksums(source, offset)[1] != prefix:
                raise LinkError('Retained upload differs from source. Keep it for inspection or remove its cache pair on DOS.')
            with source.open('rb') as stream:
                stream.seek(offset)
                done = offset
                progress(done, size)
                while done < size:
                    data = stream.read(min(self.block_size, size-done))
                    if not data:
                        raise LinkError('Local file became shorter during transfer')
                    packed = compress_packet(data) if self.compress_uploads else None
                    if packed is not None:
                        self.request(PUT_COMPRESSED, packed)
                        compressed_blocks += 1
                        wire_bytes += len(packed)
                    else:
                        self.request(PUT_DATA, data)
                        raw_blocks += 1
                        wire_bytes += len(data)
                    done += len(data)
                    progress(done, size)
            after = source.stat()
            if (before.st_size, before.st_mtime_ns, before.st_ino) != (after.st_size, after.st_mtime_ns, after.st_ino):
                raise LinkError('Local file changed during transfer; upload retained without committing')
            phase('Verifying on DOS', 0, size)
            verified_size, verified_crc = self.scan(PUT_VERIFY,
                lambda done,total: phase('Verifying on DOS',done,total))
            if verified_size != size or verified_crc != checksum:
                raise LinkError('DOS staging-file readback failed; original destination and partial retained')
            phase('Saving on DOS', 0, 0)
            reply = self.request(PUT_END, struct.pack('<H', checksum))
            if reply != struct.pack('<H', checksum) or self.stat(path) != meta:
                raise LinkError('Upload data or DOS metadata verification failed')
            progress(size, size)
            return {'crc16': checksum, 'sha256': digest, 'resumed_bytes': offset, 'metadata': asdict(meta),
                    'compressed_blocks': compressed_blocks, 'raw_blocks': raw_blocks,
                    'upload_payload_bytes': wire_bytes}
        except BaseException:
            self.abort()
            raise

    def download(self, path, destination, progress=lambda done,total: None, *, overwrite=False,
                 expected_meta=None, phase=lambda label,done,total: None):
        path, destination = dos_path(path), Path(destination)
        destination.parent.mkdir(parents=True, exist_ok=True)
        folder = metadata_dir(destination.parent) / 'partials'
        folder.mkdir(exist_ok=True)
        identity = getattr(self, 'drive', '') + path + str(destination.absolute())
        token = hashlib.sha256(identity.encode()).hexdigest()[:24]
        temp, state = folder / (token+'.part'), folder / (token+'.json')
        try:
            meta = self.stat(path)
            if expected_meta is not None and meta != expected_meta:
                raise LinkError('Source metadata changed since preview; refresh the plan')
            if meta.is_dir:
                raise ValueError('Select a folder transfer for directories.')
            wanted = {'source': identity, 'metadata': asdict(meta)}
            resume = temp.exists() and state.exists() and json.loads(state.read_text()) == wanted
            if not resume or temp.stat().st_size > meta.size:
                temp.write_bytes(b'')
                atomic_json(state, wanted)
            offset, prefix, _ = checksums(temp)
            for attempt in range(2):
                actual = Metadata.unpack(self.request(GET_BEGIN, struct.pack('<I',offset)+wire_path(path)))
                if actual != meta:
                    raise LinkError('Source changed before download; refresh and retry.')
                scanned, remote_prefix = self.scan(GET_STATUS)
                if scanned == offset and prefix == remote_prefix:
                    break
                self.abort()
                temp.write_bytes(b'')
                offset, prefix = 0, 0xffff
            else:
                raise LinkError('Download resume prefix could not be verified')
            done, checksum = offset, prefix
            with temp.open('ab') as stream:
                progress(done, meta.size)
                while done < meta.size:
                    data = self.request(GET_DATA)
                    if not data or len(data) > meta.size-done:
                        raise LinkError('Unexpected remote file length')
                    stream.write(data)
                    checksum = crc(data, checksum)
                    done += len(data)
                    progress(done, meta.size)
                result = self.request(GET_END)
                stream.flush()
                os.fsync(stream.fileno())
            if result != struct.pack('<H', checksum):
                raise LinkError('Downloaded file checksum did not match; partial retained')
            phase('Verifying download', 0, meta.size)
            _, local_crc, digest = checksums(temp, progress=lambda done,_:
                phase('Verifying download',done,meta.size))
            if local_crc != checksum:
                raise LinkError('Local disk verification failed; partial retained')
            if overwrite:
                os.replace(temp, destination)
            else:
                os.link(temp, destination)  # Atomic no-clobber commit on the same filesystem.
                temp.unlink()
            preserve_metadata(destination, meta)
            state.unlink(missing_ok=True)
            progress(meta.size, meta.size)
            return {'crc16': checksum, 'sha256': digest, 'resumed_bytes': offset, 'metadata': asdict(meta)}
        except BaseException:
            self.abort()
            raise
