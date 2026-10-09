"""Optional, reversible FTDI latency tuning. No serial port is opened here."""
from pathlib import Path
import argparse
import os
import re
import subprocess
import sys


def latency_path(device):
    resolved = Path(device).resolve()
    if resolved.parent != Path('/dev') or not resolved.is_char_device():
        return None
    name = resolved.name
    if not re.fullmatch(r'ttyUSB[0-9]+', name):
        return None
    port = Path('/sys/bus/usb-serial/devices') / name
    if (port / 'driver').resolve().name != 'ftdi_sio':
        return None
    path = port / 'latency_timer'
    return path if path.exists() else None


def read_latency(device):
    path = latency_path(device)
    return int(path.read_text()) if path else None


def apply_latency(device, value, authorize=False):
    value = int(value)
    if not 1 <= value <= 255:
        raise ValueError('FTDI latency must be 1..255 milliseconds.')
    path = latency_path(device)
    if path is None:
        return False
    if int(path.read_text()) == value:
        return True
    try:
        path.write_text(str(value) + '\n')
    except PermissionError:
        if not authorize:
            raise PermissionError('Administrator authorization is needed for FTDI latency; use Apply FTDI latency.') from None
        result = subprocess.run(['pkexec', sys.executable, str(Path(__file__).resolve()),
                                 '--device', str(Path(device).resolve()), '--value', str(value)],
                                capture_output=True, text=True)
        if result.returncode:
            raise PermissionError(result.stderr.strip() or 'FTDI latency authorization was not completed.')
    if int(path.read_text()) != value:
        raise OSError('FTDI latency readback did not match the requested value.')
    return True


class LatencySession:
    def __init__(self, device):
        self.device = device
        self.previous = read_latency(device)
        self.applied = None

    def apply(self, value, authorize=False):
        if self.previous is None:
            return False
        if apply_latency(self.device, value, authorize):
            self.applied = int(value)
            return True
        return False

    def restore(self, authorize=False):
        if self.applied is not None and self.previous != self.applied:
            # Do not undo somebody else's intervening setting change.
            if read_latency(self.device) == self.applied:
                apply_latency(self.device, self.previous, authorize)
        self.applied = None


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Change only the selected FTDI adapter latency; no EEPROM writes.')
    parser.add_argument('--device', required=True)
    parser.add_argument('--value', required=True, type=int)
    args = parser.parse_args()
    if not apply_latency(args.device, args.value):
        raise SystemExit('Selected device is not an FTDI serial adapter.')
    print(f'{args.device}: FTDI latency {read_latency(args.device)} ms')
