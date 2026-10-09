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

BAUDS = (1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200)
MAX_PAYLOAD = 512
MAGIC = b"NX"
HELLO, FIRST, NEXT = 1, 2, 3
PUT_BEGIN, PUT_DATA, PUT_END = 0x10, 0x11, 0x12
GET_BEGIN, GET_DATA, GET_END = 0x20, 0x21, 0x22
CANCEL, ACK, ERROR = 0x30, 0x80, 0xFF
STAT, MKDIR, META, HASH, HASH_STEP = 4, 5, 6, 7, 8
PUT_STATUS, GET_STATUS = 0x13, 0x24
INFO, SETTINGS, CONFIRM, DELETE, RENAME = 0x40, 0x41, 0x42, 0x50, 0x51
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


def checksums(path, limit=None):
    checksum, digest, done = 0xffff, hashlib.sha256(), 0
    with Path(path).open('rb') as stream:
        while limit is None or done < limit:
            block = stream.read(65536 if limit is None else min(65536, limit-done))
            if not block:
                break
            checksum = crc(block, checksum)
            digest.update(block)
            done += len(block)
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
    def __init__(self, path: str, baud: int, flow="none", peer="none"):
        if baud not in BAUDS or flow not in FLOWS or peer not in PEERS:
            raise ValueError('Unsupported baud rate')
        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        self.old = None
        self.path, self.baud, self.flow, self.peer = path, baud, flow, peer
        try:
            self.old = termios.tcgetattr(self.fd)
            fcntl.flock(self.fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            if hasattr(termios, 'TIOCEXCL'):
                fcntl.ioctl(self.fd, termios.TIOCEXCL)
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
        ready, _, _ = select.select([self.fd], [], [], max(0, timeout))
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
        end = time.monotonic()+timeout
        mask = (termios.TIOCM_DSR if self.peer in ('dsr', 'both') else 0) | (termios.TIOCM_CAR if self.peer in ('dcd', 'both') else 0)
        while mask:
            state = self.modem_status()
            if state is None:
                raise LinkError('This serial device cannot report the required DSR/DCD signals.')
            if state & mask == mask:
                break
            if time.monotonic() >= end:
                raise TimeoutError('Required DSR/DCD signal is low. Check cable and DOS readiness.')
            time.sleep(.005)
        view = memoryview(data)
        while view:
            remaining = end-time.monotonic()
            if remaining <= 0:
                raise TimeoutError('Serial write timed out')
            _, ready, _ = select.select([], [self.fd], [], remaining)
            if ready:
                try:
                    n = os.write(self.fd, view)
                except BlockingIOError:
                    continue
                view = view[n:]

    def configure(self, baud, flow='none', peer='none'):
        if baud not in BAUDS or flow not in FLOWS or peer not in PEERS:
            raise ValueError('Unsupported serial settings')
        attrs = termios.tcgetattr(self.fd)
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL | (termios.CRTSCTS if flow == 'rtscts' else 0)
        attrs[4] = attrs[5] = getattr(termios, 'B'+str(baud))
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        self.baud, self.flow, self.peer = baud, flow, peer

    def modem_status(self):
        try:
            return struct.unpack('I', fcntl.ioctl(self.fd, termios.TIOCMGET, struct.pack('I', 0)))[0]
        except OSError:
            return None

    def close(self):
        if self.fd is not None:
            try:
                if self.old is not None:
                    termios.tcsetattr(self.fd, termios.TCSANOW, self.old)
            except OSError:
                pass
            try:
                if hasattr(termios, 'TIOCNXCL'):
                    fcntl.ioctl(self.fd, termios.TIOCNXCL)
            except OSError:
                pass
            os.close(self.fd)
            self.fd = None


class Client:
    """Single-worker, stop-and-wait client; retries use the identical sequence."""
    def __init__(self, transport, baud=38400, timeout=None, attempts=5):
        self.transport = transport
        self.timeout = timeout or max(3.0, (MAX_PAYLOAD+16)*10/baud + 1.0)
        self.baud, self.flow, self.peer, self.block_size = baud, "none", "none", 128
        self.attempts = attempts
        self.sequence = 0
        self.parser = Parser()
        self.retries = 0
        self.cancelled = threading.Event()

    def request(self, kind: int, payload=b'', *, cancellable=True) -> bytes:
        seq = self.sequence
        self.sequence = (seq+1) & 255
        packet = encode(kind, seq, payload)
        for attempt in range(self.attempts):
            if cancellable and self.cancelled.is_set():
                raise Cancelled('Transfer cancelled')
            self.transport.write(packet, self.timeout)
            until = time.monotonic()+self.timeout
            while time.monotonic() < until:
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
        raise LinkError('No valid reply. Check the DOS server, matching baud rates, cable, and port. Lower the speed if needed.')

    def connect(self) -> str:
        hello = self.request(HELLO).decode('ascii', errors='replace')
        if not hello.startswith('NUXTLINK/3 '):
            raise VersionError('Install the matching NuXT Link 3 DOS server and Linux client.')
        self.drive = hello.split()[1]
        return hello

    def diagnostics(self, *, cancellable=True):
        return self._diagnostics(self.request(INFO, cancellable=cancellable))

    def _diagnostics(self, data):
        fmt = '<8B6I2IHHB'
        size = struct.calcsize(fmt)
        if len(data) <= size or data[-1:] != b'\0':
            raise LinkError('Invalid server diagnostics')
        fields = ('version', 'com', 'baud_index', 'flow_id', 'peer_mask', 'fifo', 'msr', 'mcr',
                  'crc_errors', 'overruns', 'parity_errors', 'framing_errors', 'breaks', 'flow_timeouts',
                  'free_bytes', 'total_bytes', 'max_payload', 'block_size', 'pending')
        info = dict(zip(fields, struct.unpack(fmt, data[:size])))
        if (info['version'] != 3 or info['baud_index'] >= len(BAUDS) or info['flow_id'] > 1 or
            info['peer_mask'] not in PEERS.values() or info['com'] not in (1,2,3,4) or
            info['block_size'] not in (128,256,512) or not info['block_size'] <= info['max_payload'] <= MAX_PAYLOAD):
            raise LinkError('Unsupported diagnostics version or settings')
        info['baud'] = BAUDS[info['baud_index']]
        info['flow'] = FLOWS[info['flow_id']]
        info['peer'] = next((k for k,v in PEERS.items() if v == info['peer_mask']), 'none')
        info['program'] = dos_path(data[size:-1].decode('cp437'))
        self.baud, self.flow, self.peer, self.block_size = info['baud'], info['flow'], info['peer'], info['block_size']
        return info

    def apply_settings(self, baud, flow='none', peer='none', block_size=128):
        if baud not in BAUDS or flow not in FLOWS or peer not in PEERS or block_size not in (128,256,512):
            raise ValueError('Unsupported connection settings')
        old = self.diagnostics()
        desired = (baud, flow, peer, block_size)
        if desired == (old['baud'], old['flow'], old['peer'], old['block_size']):
            self.transport.configure(baud, flow, peer)
            return old
        token = secrets.randbits(32)
        packet = struct.pack('<BBBHI', BAUDS.index(baud), FLOWS.index(flow), PEERS[peer], block_size, token)
        attempts, timeout = self.attempts, self.timeout
        self.attempts = 1
        started = time.monotonic()
        prepared = False
        try:
            # A RemoteError rejects the proposal without changing either side.
            answer = self.request(SETTINGS, packet, cancellable=False)
            if answer != packet:
                raise LinkError('Connection-settings acknowledgement mismatch')
            prepared = True
            time.sleep(.28)
            self.transport.configure(baud, flow, peer)
            self.parser.buffer.clear()
            self.timeout = max(3.0, (MAX_PAYLOAD+16)*10/baud + 1.0)
            answer = self.request(CONFIRM, struct.pack('<I',token), cancellable=False)
            info = self._diagnostics(answer)
            if desired != (info['baud'], info['flow'], info['peer'], info['block_size']):
                raise LinkError('Server confirmed different settings')
            info['pending'] = 0
            return info
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
                except (OSError, LinkError):
                    pass
            # Unconfirmed changes expire after 218 BIOS ticks (about 12 seconds).
            remaining = 13.5 - (time.monotonic()-started)
            if remaining > 0:
                time.sleep(remaining)
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
        old = self.attempts
        try:
            self.attempts = 1
            self.request(CANCEL, cancellable=False)
        except (OSError, LinkError, TimeoutError):
            pass
        finally:
            self.attempts = old

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

    def upload(self, source, path, progress=lambda done,total: None, *, meta=None, overwrite=False):
        source, path = Path(source), dos_path(path)
        meta = meta or local_metadata(source)
        size, checksum, digest = checksums(source)
        if meta.is_dir or size != meta.size or size > MAX_FILE:
            raise ValueError('Invalid file size or source changed during preparation')
        before = source.stat()
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
                    self.request(PUT_DATA, data)
                    done += len(data)
                    progress(done, size)
            after = source.stat()
            if (before.st_size, before.st_mtime_ns, before.st_ino) != (after.st_size, after.st_mtime_ns, after.st_ino):
                raise LinkError('Local file changed during transfer; upload retained without committing')
            reply = self.request(PUT_END, struct.pack('<H', checksum))
            if reply != struct.pack('<H', checksum) or self.stat(path) != meta:
                raise LinkError('Upload data or DOS metadata verification failed')
            progress(size, size)
            return {'crc16': checksum, 'sha256': digest, 'resumed_bytes': offset, 'metadata': asdict(meta)}
        except BaseException:
            self.abort()
            raise

    def download(self, path, destination, progress=lambda done,total: None, *, overwrite=False):
        path, destination = dos_path(path), Path(destination)
        destination.parent.mkdir(parents=True, exist_ok=True)
        folder = metadata_dir(destination.parent) / 'partials'
        folder.mkdir(exist_ok=True)
        identity = getattr(self, 'drive', '') + path + str(destination.absolute())
        token = hashlib.sha256(identity.encode()).hexdigest()[:24]
        temp, state = folder / (token+'.part'), folder / (token+'.json')
        try:
            meta = self.stat(path)
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
            _, local_crc, digest = checksums(temp)
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
