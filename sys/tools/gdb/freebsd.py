#
# Copyright (c) 2025 Mark Johnston <markj@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

import gdb


def symval(name):
    sym = gdb.lookup_global_symbol(name)
    if sym is None:
        sym = gdb.lookup_static_symbol(name)
        if sym is None:
            raise gdb.GdbError(f"Symbol '{name}' not found")
    return sym.value()


def _queue_foreach(head, field, headf, nextf):
    elm = head[headf]
    while elm != 0:
        yield elm
        elm = elm[field][nextf]


def list_foreach(head, field):
    """sys/queue.h-style iterator."""
    return _queue_foreach(head, field, "lh_first", "le_next")


def tailq_foreach(head, field):
    """sys/queue.h-style iterator."""
    return _queue_foreach(head, field, "tqh_first", "tqe_next")


def linker_file_foreach():
    """Iterate over loaded linker files."""
    return tailq_foreach(symval("linker_files"), "link")


def pcpu_foreach():
    mp_maxid = symval("mp_maxid")
    cpuid_to_pcpu = symval("cpuid_to_pcpu")

    cpu = 0
    while cpu <= mp_maxid:
        pcpu = cpuid_to_pcpu[cpu]
        if pcpu:
            yield pcpu
        cpu = cpu + 1


def tid_to_gdb_thread(tid):
    """Convert a FreeBSD kernel thread ID to a gdb inferior thread."""
    for thread in gdb.inferiors()[0].threads():
        if thread.ptid[2] == tid:
            return thread
    else:
        return None


def tdfind(tid, pid=-1):
    """Convert a FreeBSD kernel thread ID to a struct thread pointer."""
    td = tdfind.cached_threads.get(int(tid))
    if td:
        return td

    for p in list_foreach(symval("allproc"), "p_list"):
        if pid != -1 and pid != p['p_pid']:
            continue
        for td in tailq_foreach(p['p_threads'], "td_plist"):
            ntid = td['td_tid']
            tdfind.cached_threads[int(ntid)] = td
            if ntid == tid:
                return td


tdfind.cached_threads = dict()
