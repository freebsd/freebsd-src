#-
# Copyright (c) 2022 The FreeBSD Foundation
#
# This software was developed by Mark Johnston under sponsorship from the
# FreeBSD Foundation.
#

import gdb


def symval(name):
    return gdb.lookup_global_symbol(name).value()


def tid_to_gdb_thread(tid):
    for thread in gdb.inferiors()[0].threads():
        if thread.ptid[2] == tid:
            return thread
    else:
        return None


def all_pcpus():
    mp_maxid = symval("mp_maxid")
    cpuid_to_pcpu = symval("cpuid_to_pcpu")

    cpu = 0
    while cpu <= mp_maxid:
        pcpu = cpuid_to_pcpu[cpu]
        if pcpu:
            yield pcpu
        cpu = cpu + 1


class acttrace(gdb.Command):
    def __init__(self):
        super(acttrace, self).__init__("acttrace", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        # Save the current thread so that we can switch back after.
        curthread = gdb.selected_thread()

        for pcpu in all_pcpus():
            td = pcpu['pc_curthread']
            tid = td['td_tid']

            gdb_thread = tid_to_gdb_thread(tid)
            if gdb_thread is None:
                print("failed to find GDB thread with TID {}".format(tid))
            else:
                gdb_thread.switch()

                p = td['td_proc']
                print("Tracing command {} pid {} tid {} (CPU {})".format(
                      p['p_comm'], p['p_pid'], td['td_tid'], pcpu['pc_cpuid']))
                gdb.execute("bt")
                print()

        curthread.switch()


# Registers the command with gdb, doesn't do anything.
acttrace()
