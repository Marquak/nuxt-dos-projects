"""User-editable connection profiles; portable settings only, no credentials."""
import json
import os
from pathlib import Path
from link import BAUDS, FLOWS, PEERS, BLOCK_SIZES, atomic_json

DEFAULTS = dict(transport='serial', address='', device='', baud=115200, flow='rtscts', peer='none', block=4096,
                latency=2, compression=True, com='COM1', local=str(Path.home()), remote='\\')


def validate(values):
    result = {key: values.get(key, value) for key,value in DEFAULTS.items()}
    if result['transport'] != 'serial':
        raise ValueError('This NuXT Link release uses serial only')
    if result['baud'] not in BAUDS or result['flow'] not in FLOWS or result['peer'] not in PEERS:
        raise ValueError('Invalid connection profile settings')
    if (result['block'] not in BLOCK_SIZES or result['latency'] not in (0,*range(2,256)) or
        result['transport']=='serial' and result['block'] in (1024,1440,1464)):
        raise ValueError('Invalid block size or latency')
    if not isinstance(result['compression'], bool):
        raise ValueError('Invalid compression setting')
    if result['com'] not in ('COM1','COM2','COM3','COM4'):
        raise ValueError('Invalid DOS COM port')
    if any(not isinstance(result[k], str) for k in ('device','address','com','local','remote')):
        raise ValueError('Invalid profile path')
    return result


class Profiles:
    def __init__(self, path=None):
        self.path = Path(path) if path else Path(os.environ.get('XDG_CONFIG_HOME', Path.home()/'.config'))/'nuxtlink-serial'/'profiles.json'
        self.items = {}
        if self.path.exists():
            doc = json.loads(self.path.read_text())
            if doc.get('version') != 1:
                raise ValueError('Unsupported profiles file')
            self.items = {str(k): validate(v) for k,v in doc['profiles'].items()}
        elif path is None:
            # Import the Fast preview first, then any distinct serial profiles
            # from the old combined serial/LAN client. Never alter either store.
            for directory in ('nuxtlink-fast', 'nuxtlink'):
                legacy = self.path.parent.parent/directory/'profiles.json'
                try:
                    doc = json.loads(legacy.read_text())
                    if doc.get('version') != 1 or not isinstance(doc.get('profiles'), dict):
                        continue
                except (OSError, ValueError, TypeError):
                    continue
                for old_name, values in doc['profiles'].items():
                    try:
                        name = str(old_name).strip()
                        if not name or len(name) > 80:
                            continue
                        profile = validate(values)
                    except (AttributeError, TypeError, ValueError):
                        continue
                    if name in self.items:
                        if self.items[name] == profile:
                            continue
                        base = name[:63] + ' (old serial)'
                        name = base
                        suffix = 2
                        while name in self.items:
                            name = base[:75] + f' {suffix}'
                            suffix += 1
                    self.items[name] = profile

    def save(self, name, values):
        name = name.strip()
        if not name or len(name) > 80:
            raise ValueError('Profile name must be 1..80 characters')
        self.items[name] = validate(values)
        self.write()

    def delete(self, name):
        self.items.pop(name, None)
        self.write()

    def write(self):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        atomic_json(self.path, {'version':1, 'profiles': self.items})
