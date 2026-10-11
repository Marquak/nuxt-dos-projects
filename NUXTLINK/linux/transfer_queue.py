"""Append-only transfer batches with fixed paths and per-batch backup manifests.

FileZilla's per-item local/remote path model was a design reference. This is
original Python code; it does not embed FileZilla's C++ code or dependencies.
"""
from pathlib import Path
from link import Cancelled, LinkError, dos_key, metadata_dir, atomic_json, preserve_metadata
from transfers import execute, save_report, report_paths, safe_local

FINISHED = ('Complete', 'Verified', 'Skipped')


class TransferQueue:
    direction = 'mixed'
    backup = False

    def __init__(self):
        self.plans = []
        self.jobs = []
        self.owners = {}
        self.status = 'Preview'

    @property
    def drive(self): return self.plans[0].drive
    @property
    def local_root(self): return self.plans[0].local_root
    @property
    def remote_root(self): return 'Multiple folders'
    @property
    def bytes(self): return sum(p.bytes for p in self.plans)
    @property
    def errors(self): return [e for p in self.plans for e in p.errors]
    @property
    def missing(self): return [e for p in self.plans for e in p.missing]
    @property
    def excluded(self): return [e for p in self.plans for e in p.excluded]

    def owner(self, job): return self.owners[id(job)]

    def append(self, plan):
        if plan.direction == 'delete':
            raise ValueError('Clear the transfer queue before preparing deletions.')
        if self.plans and plan.drive != self.drive:
            raise ValueError('This queue belongs to a different DOS drive. Clear it first.')
        # Validate the complete addition before changing anything. Separate
        # folders are fine; two writers to the same file need a deliberate choice.
        destinations = {}
        for batch in [*self.plans, plan]:
            for job in batch.jobs:
                if job.action == 'skip': continue
                key = (batch.direction, dos_key(job.target) if batch.direction == 'upload'
                       else str(Path(job.target).absolute()))
                if key in destinations:
                    previous = destinations[key]
                    if not (job.action == previous.action == 'directory' and
                            job.meta == previous.meta and job.source == previous.source):
                        raise ValueError(f'Already queued destination: {job.target}. Choose another folder or clear the queue.')
                destinations[key] = job
        for (direction, path), job in destinations.items():
            sep = '\\' if direction == 'upload' else '/'
            parent = path.rsplit(sep, 1)[0]
            while parent:
                ancestor = destinations.get((direction, parent))
                if ancestor and not ancestor.meta.is_dir:
                    raise ValueError(f'A queued file occupies the destination folder: {parent}')
                parent = parent.rsplit(sep, 1)[0] if sep in parent else ''
        self.plans.append(plan)
        self.jobs.extend(plan.jobs)
        self.owners.update((id(job), plan) for job in plan.jobs)
        self.status = 'Preview'

    def document(self):
        return {'format': 'NuXT Link queue 1', 'status': self.status,
                'drive': self.drive, 'batches': [p.document() for p in self.plans]}


def append_plan(existing, plan):
    if plan.direction == 'delete':
        if existing and existing.jobs:
            raise ValueError('Clear the current queue before preparing deletions.')
        return plan
    if existing is not None and existing.direction == 'delete':
        raise ValueError('Clear the deletion queue before adding transfers.')
    if isinstance(existing, TransferQueue):
        existing.append(plan)
        return existing
    queue = TransferQueue()
    if existing is not None: queue.append(existing)
    queue.append(plan)
    return queue


def queue_report(queue):
    folder = metadata_dir(queue.local_root)
    atomic_json(folder/'last-queue.json', queue.document())
    path = folder/'last-report.txt'
    lines = [f'NuXT Link: {queue.status}', f'Drive: {queue.drive}', '']
    for i, plan in enumerate(queue.plans, 1):
        lines.append(f'Batch {i}: {plan.direction}, {plan.local_root} <-> {plan.remote_root}')
        for job in plan.jobs:
            lines.append(f'{job.status}: {job.source} -> {job.target}')
            if job.reason: lines.append('  '+job.reason)
        lines.extend(f'ERROR: {e["path"]}: {e["error"]}' for e in plan.errors)
        lines.extend(f'EXCLUDED: {e["path"]}: {e["reason"]}' for e in plan.excluded)
        lines.extend(f'MISSING: {name}' for name in plan.missing)
        lines.append('')
    path.write_text('\n'.join(lines)+'\n', encoding='utf8')
    return str(path)


def execute_queue(client, queue, progress=lambda index,job,done,total: None):
    if not isinstance(queue, TransferQueue):
        return execute(client, queue, progress)
    queue.status = 'Running'
    try:
        offset = 0
        for plan in queue.plans:
            if client.cancelled.is_set(): raise Cancelled('Queue cancelled; remaining batches retained.')
            if plan.status != 'Complete':
                execute(client, plan, lambda i,j,d,t,base=offset: progress(base+i,j,d,t))
            offset += len(plan.jobs)
        # A later batch can create children in an earlier batch's directory.
        # Restore all such directories once more, deepest first, after ALL writes.
        directories, later_parents = [], set()
        offset = len(queue.jobs)
        for plan in reversed(queue.plans):
            offset -= len(plan.jobs)
            key = lambda path: (plan.direction, dos_key(path) if plan.direction == 'upload' else str(Path(path).absolute()))
            for i, job in enumerate(plan.jobs):
                if job.action == 'directory' and job.status == 'Complete' and key(job.target) in later_parents:
                    directories.append((offset+i, job, plan))
            sep = '\\' if plan.direction == 'upload' else '/'
            for job in plan.jobs:
                if job.status not in ('Complete','Verified') or job.action == 'skip': continue
                parent = job.target.rsplit(sep,1)[0]
                while parent:
                    later_parents.add(key(parent))
                    parent = parent.rsplit(sep,1)[0] if sep in parent else ''
        directories.sort(key=lambda item: item[1].target.count('\\') + item[1].target.count('/'), reverse=True)
        for index, job, plan in directories:
            if client.cancelled.is_set(): raise Cancelled('Queue cancelled; directory metadata still pending.')
            try:
                if plan.direction == 'download':
                    safe_local(plan.local_root, str(Path(job.target).relative_to(plan.local_root)))
                    preserve_metadata(job.target, job.meta)
                else: client.set_metadata(job.target, job.meta)
            except (OSError, LinkError, ValueError) as exc:
                job.status = 'Cancelled' if isinstance(exc, Cancelled) else 'Failed'
                job.reason = str(exc)
                plan.status = 'Interrupted'
                plan.errors.append({'path': job.target, 'error': str(exc), 'runtime': True})
                save_report(plan, report_paths(plan))
                raise
            progress(index,job,0,0)
        for plan in queue.plans: save_report(plan, report_paths(plan))
        queue.status = 'Complete with omissions' if queue.errors or queue.missing or any(j.status == 'Skipped' for j in queue.jobs) else 'Complete'
    except BaseException as exc:
        queue.status = 'Cancelled' if isinstance(exc, Cancelled) else 'Interrupted'
        queue_report(queue)
        raise
    return queue_report(queue)
