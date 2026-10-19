"""Reviewable recursive plans, persistent reports, verified incremental copies."""
from __future__ import annotations
from dataclasses import dataclass, field, asdict
from pathlib import Path
import datetime
import json
import os
import time
from link import (Metadata, Entry, LinkError, RemoteError, Cancelled, dos_path,
                  join_path, valid_name, suggest_name, dos_key, local_metadata,
                  preserve_metadata, metadata_dir, checksums, atomic_json, META_FORMAT)

BACKUP_FORMAT = 'NuXT Link file backup 2'


@dataclass
class Job:
    source: str
    target: str
    meta: Metadata
    action: str
    reason: str = ''
    status: str = 'Queued'
    result: dict = field(default_factory=dict)
    overwrite: bool = False
    elapsed_seconds: float = 0.0


def elapsed_label(seconds):
    seconds = max(0, seconds)
    if seconds < 60:
        return f'{seconds:.1f}s'
    whole = int(seconds)
    if whole < 3600:
        return f'{whole//60}m {whole%60:02d}s'
    return f'{whole//3600}h {whole//60%60:02d}m {whole%60:02d}s'


def compression_stats(job):
    """Original bytes / acknowledged payload, for the successful attempt only."""
    if job.action != 'send' or 'upload_payload_bytes' not in job.result:
        return None
    original = job.meta.size - job.result.get('resumed_bytes', 0)
    payload = job.result['upload_payload_bytes']
    if original <= 0 or payload <= 0:
        return None
    return original / payload, 100 * (1 - payload / original)


def completion_columns(job, status=None):
    if (status or job.status) != 'Complete' or job.action not in ('send', 'receive'):
        return '', ''
    stats = compression_stats(job)
    if stats is not None:
        ratio, saved = stats
        compression = f'{ratio:.2f}:1 ({saved:.0f}% saved)'
    elif not job.meta.size:
        compression = 'Empty file'
    elif job.result.get('resumed_bytes') == job.meta.size:
        compression = 'Already staged'
    else:
        compression = 'Raw' if job.action == 'receive' else ''
    return elapsed_label(job.elapsed_seconds), compression


def completion_report(job):
    elapsed, compression = completion_columns(job)
    if not elapsed:
        return ''
    scope = ' (last resumed portion)' if job.action == 'send' and job.result.get('resumed_bytes') else ''
    return f'Time: {elapsed} active; compression: {compression}{scope}'


@dataclass
class Plan:
    direction: str
    local_root: str
    remote_root: str
    drive: str
    jobs: list[Job] = field(default_factory=list)
    errors: list[dict] = field(default_factory=list)
    excluded: list[dict] = field(default_factory=list)
    missing: list[str] = field(default_factory=list)
    backup: bool = False
    status: str = 'Preview'

    @property
    def bytes(self):
        return sum(j.meta.size for j in self.jobs if j.action in ('send', 'receive'))

    def document(self):
        return {'format': BACKUP_FORMAT, 'updated': datetime.datetime.now().isoformat(timespec='seconds'),
                **asdict(self)}


def owned_metadata(folder):
    marker = folder / 'owner.json'
    return not folder.is_symlink() and marker.is_file() and json.loads(marker.read_text()).get('format') == META_FORMAT


def safe_local(root, relative):
    """Manifest and remote names never grant access outside the selected folder."""
    root = Path(root).absolute()
    parts = Path(relative).parts
    if Path(relative).is_absolute() or any(p in ('..', '.', '') for p in parts):
        raise ValueError('Unsafe local relative path')
    target = root
    for part in parts:
        target /= part
        if target.is_symlink():
            raise ValueError(f'Symlink is not a transfer destination: {target}')
    if not target.resolve().is_relative_to(root.resolve()):
        raise ValueError('Destination escapes the selected folder')
    return target


def read_manifest(root):
    p = Path(root) / '.nuxtlink' / 'backup.json'
    if not p.exists():
        return None
    doc = json.loads(p.read_text())
    if doc.get('format') != BACKUP_FORMAT:
        raise ValueError('Unsupported backup manifest')
    return doc


def remote_tree(client, root, names=None, notify=lambda message: None):
    """Fully consume each DOS find-first/find-next sequence before recursing."""
    entries, errors, excluded = [], [], []
    pending = [(dos_path(root), '')]
    while pending:
        current, relative = pending.pop()
        notify('Scanning ' + client.drive[:2] + current)
        try:
            children = client.listing(current)
        except RemoteError as exc:
            errors.append({'path': current, 'error': str(exc)})
            continue
        for item in sorted(children, key=lambda e: (not e.meta.is_dir, e.name)):
            if not relative and names is not None and item.name not in names:
                continue
            try:
                remote = join_path(current, item.name)
                rel = relative + ('/' if relative else '') + item.name
                if item.internal:
                    excluded.append({'path': remote, 'reason': 'NuXT Link owned resumable upload cache'})
                    continue
                entries.append((rel, remote, item.meta))
                if item.meta.is_dir:
                    pending.append((remote, rel))
            except ValueError as exc:
                errors.append({'path': current + '\\' + item.name, 'error': str(exc)})
    return entries, errors, excluded


def unique_local(path, occupied):
    original, number = path, 1
    while path.exists() or str(path).casefold() in occupied:
        path = original.with_name(f'{original.stem} ({number}){original.suffix}')
        number += 1
    return path


def plan_download(client, root, destination, *, names=None, policy='overwrite', incremental=True,
                  backup=False, notify=lambda message: None):
    root, destination = dos_path(root), Path(destination).absolute()
    plan = Plan('download', str(destination), root, client.drive, backup=backup)
    entries, plan.errors, plan.excluded = remote_tree(client, root, names, notify)
    old = read_manifest(destination)
    if backup and old and (old['drive'], old['remote_root']) != (client.drive, root):
        raise ValueError('This backup folder belongs to a different DOS drive or root. Choose another folder.')
    old_files = {}
    if old and old.get('direction') == 'download':
        for j in old.get('jobs', []):
            old_files[j['source']] = j
    present = {remote for _, remote, _ in entries}
    if backup and old:
        plan.missing = sorted((set(old_files) | set(old.get('missing', [])))-present)
    occupied, mapped = set(), {'': destination}
    for rel, remote, meta in sorted(entries, key=lambda row: (row[0].count('/'), row[0])):
        parent = str(Path(rel).parent)
        parent = '' if parent == '.' else parent
        if mapped.get(parent) is None:
            plan.excluded.append({'path': remote, 'reason': 'Parent directory was skipped'})
            mapped[rel] = None
            continue
        try:
            dest = mapped[parent] / Path(rel).name
            safe_local(destination, str(dest.relative_to(destination)))
            if meta.is_dir:
                if dest.exists() and not dest.is_dir():
                    if policy == 'rename':
                        dest = unique_local(dest, occupied)
                    else:
                        raise ValueError('A local file occupies this directory path')
                mapped[rel] = dest
                plan.jobs.append(Job(remote, str(dest), meta, 'directory'))
                occupied.add(str(dest).casefold())
                continue
            action, reason, verified = 'receive', '', {}
            if dest.exists():
                if not dest.is_file():
                    raise ValueError('A local directory occupies this file path')
                if policy == 'skip':
                    action, reason = 'skip', 'Existing destination; contents not compared'
                elif policy == 'rename':
                    dest = unique_local(dest, occupied)
                elif incremental:
                    notify('Checking ' + client.drive[:2] + remote)
                    size, local_crc, sha = checksums(dest)
                    if size == meta.size:
                        actual, remote_crc = client.hash(remote)
                        if actual != meta:
                            raise LinkError(f'{remote}: source changed during preview')
                        if local_crc == remote_crc:
                            # CRC checks the serial/DOS side; SHA-256 also detects
                            # local damage against a previous backup manifest.
                            old_job = old_files.get(remote, {})
                            old_sha = old_job.get('result', {}).get('sha256')
                            old_crc = old_job.get('result', {}).get('crc16')
                            if not old_sha or old_crc != remote_crc or old_sha == sha:
                                action, reason = 'verified', 'Contents match; preserve exact DOS metadata'
                                verified = {'sha256': sha, 'crc16': remote_crc, 'metadata': asdict(meta)}
            job = Job(remote, str(dest), meta, action, reason, result=verified, overwrite=dest.exists() and policy=='overwrite')
            plan.jobs.append(job)
            occupied.add(str(dest).casefold())
        except (OSError, ValueError, RemoteError) as exc:
            plan.errors.append({'path': remote, 'error': str(exc)})
            if meta.is_dir:
                mapped[rel] = None
    return plan


def unique_dos(name, occupied):
    base, dot, ext = name.partition('.')
    for number in range(1, 100000):
        tail = '~'+str(number)
        candidate = base[:8-len(tail)] + tail + (dot+ext if dot else '')
        if dos_key(candidate) not in occupied:
            return candidate
    raise ValueError('Too many DOS filename collisions')


def plan_upload(client, sources, root, *, policy='skip', incremental=True, contents=False,
                restore=False, notify=lambda message: None):
    """Map every name and detect collisions before any remote mutation."""
    sources = [Path(p).absolute() for p in sources]
    if not sources:
        raise ValueError('Select files or folders to upload.')
    local_root = sources[0] if contents else sources[0].parent
    plan = Plan('upload', str(local_root), dos_path(root), client.drive)
    manifest = read_manifest(local_root) if restore else None
    manifest_files = {}
    if restore and manifest:
        # Use relative paths so the entire backup can be moved on Linux.
        old_root = Path(manifest['local_root'])
        for job in manifest.get('jobs', []):
            try:
                rel = Path(job['target']).relative_to(old_root)
                safe_local(local_root, str(rel))
            except (KeyError, ValueError):
                raise ValueError('Unsafe path in backup manifest') from None
            if job['status'] in ('Complete', 'Verified'):
                manifest_files[str(rel)] = job
    if restore and not manifest:
        raise ValueError('Choose a backup folder containing .nuxtlink/backup.json')
    if restore:
        # Restore only verified manifest members, including empty directories.
        candidates = sorted(manifest_files, key=lambda p: (len(Path(p).parts), p))
    else:
        candidates = None
    directory_cache, reserved, assigned = {}, {}, {}
    def children(remote):
        if remote not in directory_cache:
            try:
                items = client.listing(remote)
            except RemoteError as exc:
                # Parent does not exist yet only when a mkdir is already planned.
                if any(j.target == remote and j.action == 'directory' for j in plan.jobs):
                    items = []
                else:
                    raise exc
            directory_cache[remote] = {dos_key(e.name): e for e in items}
        return directory_cache[remote]
    if candidates is not None:
        pending = [(local_root / rel, rel) for rel in candidates]
    elif contents:
        pending = [(p, p.name) for p in sorted(local_root.iterdir())]
    else:
        pending = [(p, p.name) for p in sources]
    mapped = {'': dos_path(root)}
    while pending:
        source, rel = pending.pop(0)
        notify('Preparing ' + str(source))
        parent = str(Path(rel).parent)
        parent = '' if parent == '.' else parent
        remote_parent = mapped.get(parent)
        if remote_parent is None:
            plan.excluded.append({'path': str(source), 'reason': 'Parent directory was skipped'})
            continue
        try:
            if source.is_symlink():
                raise ValueError('Symlinks cannot be represented on DOS')
            if source.name == '.nuxtlink' and source.is_dir() and owned_metadata(source):
                continue
            if not source.is_dir() and not source.is_file():
                raise ValueError('Only regular files and folders can be transferred')
            meta = local_metadata(source)
            member = manifest_files.get(rel) if restore else None
            if member:
                archived = Metadata(**member['meta'])
                if not archived.is_dir:
                    size, checksum, digest = checksums(source)
                    if size != archived.size or digest != member['result'].get('sha256'):
                        raise ValueError('Backup file is damaged or modified (SHA-256 mismatch)')
                meta = archived
            name = valid_name(source.name) if restore else dos_key(suggest_name(source.name))
            known = children(remote_parent)
            used = reserved.setdefault(remote_parent, set())
            reason = '' if name == source.name else 'Name mapped to DOS 8.3'
            # Two local names mapping to one DOS name must NEVER overwrite each other.
            if dos_key(name) in used:
                name = unique_dos(name, used)
                reason = '8.3 name collision resolved with a unique name'
            existing = known.get(dos_key(name))
            if existing and existing.internal:
                raise ValueError('Destination is NuXT Link\'s private resume cache')
            if existing and policy == 'rename' and not (meta.is_dir and existing.meta.is_dir and restore):
                name = unique_dos(name, used | set(known))
                existing = None
                reason = 'Existing destination preserved; renamed copy'
            remote = join_path(remote_parent, name)
            used.add(dos_key(name))
            if meta.is_dir:
                if existing and not existing.meta.is_dir:
                    raise ValueError('A DOS file occupies this directory path')
                mapped[rel] = remote
                plan.jobs.append(Job(str(source), remote, meta, 'directory', reason))
                if candidates is None:
                    pending[0:0] = [(p, str(Path(rel)/p.name)) for p in sorted(source.iterdir())]
                continue
            action, verified = 'send', {}
            if existing:
                if existing.meta.is_dir:
                    raise ValueError('A DOS directory occupies this file path')
                if policy == 'skip':
                    action, reason = 'skip', 'Existing destination; contents not compared'
                elif incremental and existing.meta.size == meta.size:
                    actual, remote_crc = client.hash(remote)
                    size, checksum, digest = checksums(source)
                    if size == meta.size and checksum == remote_crc:
                        action, reason = 'verified', 'Contents match; preserve exact DOS metadata'
                        verified = {'crc16': checksum, 'sha256': digest, 'metadata': asdict(meta)}
            plan.jobs.append(Job(str(source), remote, meta, action, reason, result=verified,
                                 overwrite=existing is not None and policy == 'overwrite'))
        except (OSError, ValueError, RemoteError) as exc:
            plan.errors.append({'path': str(source), 'error': str(exc)})
            if source.is_dir():
                mapped[rel] = None
    return plan


def report_paths(plan):
    folder = metadata_dir(plan.local_root)
    return folder / ('backup.json' if plan.backup else 'last-transfer.json'), folder / 'last-report.txt'


def save_report(plan, paths):
    manifest, report = paths
    atomic_json(manifest, plan.document())
    lines = [f'NuXT Link — {plan.status}', f'Drive: {plan.drive}    Folder: {plan.remote_root}',
             f'Direction: {plan.direction}', '']
    for j in plan.jobs:
        lines.append(f'{j.status}: {j.source} -> {j.target}  [{j.meta.attributes}; {j.meta.display_time}]')
        if j.reason:
            lines.append('  '+j.reason)
        if j.result:
            lines.append('  '+json.dumps(j.result, sort_keys=True))
        if completion_report(j):
            lines.append('  '+completion_report(j))
    for e in plan.errors:
        lines.append(f'ERROR: {e["path"]}: {e["error"]}')
    for e in plan.excluded:
        lines.append(f'EXCLUDED: {e["path"]}: {e["reason"]}')
    for p in plan.missing:
        lines.append(f'MISSING ON DOS: {p} (previous local copy retained)')
    report.write_text('\n'.join(lines)+'\n', encoding='utf8')


def execute(client, plan, progress=lambda index,job,done,total: None, stage=None):
    paths = report_paths(plan)
    plan.errors = [e for e in plan.errors if not e.get('runtime')]
    plan.status = 'Running'
    save_report(plan, paths)
    directories = []
    current = None
    try:
        for index, job in enumerate(plan.jobs):
            if client.cancelled.is_set():
                raise Cancelled('Queue cancelled. Start/resume to continue retained partial files.')
            if job.status in ('Complete', 'Verified', 'Skipped'):
                if job.action == 'directory':
                    directories.append((index, job))
                continue
            current = job
            if job.status in ('Failed', 'Cancelled'):
                job.reason = ''
            job.status = 'Running'
            started = time.monotonic() if job.action in ('send', 'receive') else None
            progress(index, job, 0, job.meta.size)
            try:
                if plan.direction == 'upload' and job.action in ('send', 'verified') and local_metadata(job.source) != job.meta:
                    raise LinkError('Local metadata changed since preview; refresh the plan')
                if plan.direction == 'download':
                    safe_local(plan.local_root, str(Path(job.target).relative_to(plan.local_root)))
                if job.action == 'directory':
                    if plan.direction == 'download':
                        Path(job.target).mkdir(parents=True, exist_ok=True)
                    else:
                        client.mkdir(job.target)
                    directories.append((index, job))
                    job.status = 'Created; metadata pending'
                elif job.action == 'skip':
                    job.status = 'Skipped'
                elif job.action == 'verified':
                    local = job.target if plan.direction == 'download' else job.source
                    remote = job.source if plan.direction == 'download' else job.target
                    size, checksum, digest = checksums(local)
                    actual, remote_crc = client.hash(remote)
                    if size != job.meta.size or digest != job.result.get('sha256') or checksum != remote_crc:
                        raise LinkError('Contents changed since preview; build a new transfer plan')
                    if plan.direction == 'download' and actual != job.meta:
                        raise LinkError('DOS metadata changed since preview; build a new transfer plan')
                    if plan.direction == 'download':
                        preserve_metadata(job.target, job.meta)
                    else:
                        client.set_metadata(job.target, job.meta)
                    job.status = 'Verified'
                elif plan.direction == 'download':
                    phase = {} if stage is None else {'phase': lambda label,d,t: stage(index,job,label,d,t)}
                    job.result = client.download(job.source, job.target,
                        lambda d,t: progress(index, job, d, t), overwrite=job.overwrite,
                        expected_meta=job.meta, **phase)
                    if Metadata(**job.result['metadata']) != job.meta:
                        raise LinkError('Source metadata changed since preview; refresh the plan')
                    job.status = 'Complete'
                else:
                    phase = {} if stage is None else {'phase': lambda label,d,t: stage(index,job,label,d,t)}
                    job.result = client.upload(job.source, job.target,
                        lambda d,t: progress(index, job, d, t), meta=job.meta,
                        overwrite=job.overwrite, **phase)
                    job.status = 'Complete'
            except RemoteError as exc:
                job.status, job.reason = 'Failed', str(exc)
                plan.errors.append({'path': job.source, 'error': str(exc), 'runtime': True})
                client.abort()
            except OSError as exc:
                # Serial I/O failures and local disk failures both stop the queue;
                # continuing could cause a stream of misleading follow-on errors.
                job.status, job.reason = 'Failed', str(exc)
                raise
            finally:
                if started is not None:
                    job.elapsed_seconds += max(0, time.monotonic() - started)
            progress(index, job, job.meta.size, job.meta.size)
            save_report(plan, paths)
        # Apply directory timestamps last, deepest first. Creating child files
        # and metadata sidecars must not change the final exported timestamp.
        for index, job in reversed(directories):
            current = job
            if client.cancelled.is_set():
                raise Cancelled("Queue cancelled; resumable files retained.")
            try:
                if plan.direction == 'download':
                    safe_local(plan.local_root, str(Path(job.target).relative_to(plan.local_root)))
                    preserve_metadata(job.target, job.meta)
                else:
                    client.set_metadata(job.target, job.meta)
                job.status = 'Complete'
            except (OSError, RemoteError) as exc:
                job.status, job.reason = 'Failed', str(exc)
                plan.errors.append({'path': job.source, 'error': str(exc), 'runtime': True})
            progress(index, job, 0, 0)
        plan.status = 'Complete with omissions' if plan.errors or plan.missing or any(j.action == 'skip' for j in plan.jobs) else 'Complete'
    except BaseException as exc:
        client.abort()
        plan.status = 'Cancelled' if isinstance(exc, Cancelled) else 'Interrupted'
        if current and current.status in ('Running','Created; metadata pending'):
            current.status = 'Cancelled' if isinstance(exc, Cancelled) else 'Failed'
            current.reason = str(exc)
        plan.errors.append({'path': 'queue', 'error': str(exc), 'runtime': True})
        save_report(plan, paths)
        raise
    save_report(plan, paths)
    return str(paths[1])


def verify_backup(root, notify=lambda message: None):
    root = Path(root).absolute()
    doc = read_manifest(root)
    if doc is None:
        raise ValueError('No NuXT Link backup manifest in this folder')
    errors, checked = [], 0
    old_root = Path(doc['local_root'])
    for job in doc['jobs']:
        try:
            path = safe_local(root, str(Path(job['target']).relative_to(old_root)))
            notify('Verifying '+str(path))
            if job['status'] not in ('Complete', 'Verified'):
                raise ValueError('Entry did not complete in the backup')
            meta = Metadata(**job['meta'])
            if meta.is_dir:
                if not path.is_dir():
                    raise ValueError('Directory missing')
            else:
                size, checksum, digest = checksums(path)
                if size != meta.size or digest != job['result'].get('sha256'):
                    raise ValueError('Missing or damaged file: SHA-256 mismatch')
            if local_metadata(path) != meta:
                raise ValueError('DOS metadata differs from the backup manifest')
            checked += 1
        except (OSError, ValueError) as exc:
            errors.append({'path': job.get('target','?'), 'error': str(exc)})
    errors.extend(doc.get('errors', []))
    errors.extend({'path': p, 'error': 'Missing on DOS at backup time'} for p in doc.get('missing', []))
    result = {'checked': checked, 'errors': errors, 'verified_at': datetime.datetime.now().isoformat()}
    folder = metadata_dir(root)
    atomic_json(folder/'verification.json', result)
    lines = [f'{checked} entries verified; {len(errors)} issues.'] + [f'{e["path"]}: {e["error"]}' for e in errors]
    report = folder/'verification.txt'
    report.write_text('\n'.join(lines)+'\n')
    return result, str(report)
