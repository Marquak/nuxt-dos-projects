"""User-editable connection profiles; portable settings only, no credentials."""
import json
import os
from pathlib import Path
from link import BAUDS, FLOWS, PEERS, BLOCK_SIZES, atomic_json

DEFAULTS = dict(device='', baud=38400, flow='none', peer='none', block=128,
                latency=2, compression=True, com='COM1', local=str(Path.home()), remote='\\')


def validate(values):
    result = {key: values.get(key, value) for key,value in DEFAULTS.items()}
    if result['baud'] not in BAUDS or result['flow'] not in FLOWS or result['peer'] not in PEERS:
        raise ValueError('Invalid connection profile settings')
    if result['block'] not in BLOCK_SIZES or result['latency'] not in (0,*range(2,256)):
        raise ValueError('Invalid block size or latency')
    if not isinstance(result['compression'], bool):
        raise ValueError('Invalid compression setting')
    if result['com'] not in ('COM1','COM2','COM3','COM4'):
        raise ValueError('Invalid DOS COM port')
    if any(not isinstance(result[k], str) for k in ('device','com','local','remote')):
        raise ValueError('Invalid profile path')
    return result


class Profiles:
    def __init__(self, path=None):
        self.path = Path(path) if path else Path(os.environ.get('XDG_CONFIG_HOME', Path.home()/'.config'))/'nuxtlink'/'profiles.json'
        self.items = {}
        if self.path.exists():
            doc = json.loads(self.path.read_text())
            if doc.get('version') != 1:
                raise ValueError('Unsupported profiles file')
            self.items = {str(k): validate(v) for k,v in doc['profiles'].items()}

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
