"""DOS-side file management with reviewable, interruptible deletion plans."""
from pathlib import Path
from link import dos_path, dos_key, join_path, RemoteError, Cancelled
from transfers import Plan, Job, remote_tree, save_report, report_paths


def plan_delete(client, root, names, report_root, notify=lambda text: None):
    if not names:
        raise ValueError('Select DOS files or folders to delete.')
    info = client.diagnostics()
    root = dos_path(root)
    for name in names:
        path = dos_key(join_path(root, name))
        server = dos_key(info['program'])
        if server == path or server.startswith(path+'\\'):
            raise ValueError('The selection contains the running DOS server. Exit it locally before removing it.')
    entries, errors, excluded = remote_tree(client, root, names, notify)
    if errors or excluded:
        raise ValueError('Deletion preview is incomplete or contains a protected cache: ' +
                         '; '.join(e.get('error',e.get('reason','')) for e in errors+excluded))
    plan = Plan('delete', str(Path(report_root).absolute()), root, client.drive)
    for rel, remote, meta in sorted(entries, key=lambda row: (-row[1].count('\\'), row[2].is_dir, row[1])):
        reason = 'Protected: '+meta.attributes if meta.attr & 7 else ''
        plan.jobs.append(Job(remote, remote, meta, 'rmdir' if meta.is_dir else 'delete', reason))
    return plan


def execute_delete(client, plan, progress=lambda index,job,done,total: None, *, protected=False):
    if plan.direction != 'delete':
        raise ValueError('Not a deletion queue')
    paths = report_paths(plan)
    plan.errors = [e for e in plan.errors if not e.get('runtime')]
    plan.status = 'Running'
    save_report(plan, paths)
    current = None
    try:
        for index, job in enumerate(plan.jobs):
            if client.cancelled.is_set():
                raise Cancelled('Deletion queue cancelled; completed deletions cannot be undone.')
            if job.status == 'Complete':
                continue
            current = job
            job.status = 'Running'
            progress(index, job, 0, 1)
            try:
                client.delete(job.source, job.meta, protected=protected)
                job.status = 'Complete'
            except RemoteError as exc:
                job.status, job.reason = 'Failed', str(exc)
                plan.errors.append({'path': job.source, 'error': str(exc), 'runtime': True})
            progress(index, job, 1, 1)
            save_report(plan, paths)
        plan.status = 'Complete with omissions' if plan.errors else 'Complete'
    except BaseException as exc:
        if current and current.status == 'Running':
            current.status = 'Cancelled' if isinstance(exc, Cancelled) else 'Failed'
        plan.status = 'Cancelled' if isinstance(exc, Cancelled) else 'Interrupted'
        plan.errors.append({'path': 'queue', 'error': str(exc), 'runtime': True})
        client.abort()
        save_report(plan, paths)
        raise
    save_report(plan, paths)
    return str(paths[1])
