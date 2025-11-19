#
# Copyright (c) 2022 The FreeBSD Foundation
#
# This software was developed by Mark Johnston under sponsorship from the
# FreeBSD Foundation.
#
# SPDX-License-Identifier: BSD-2-Clause
#

import gdb
from freebsd import *
from pcpu import *


class acttrace(gdb.Command):
    """
    Print the stack trace of all threads that were on-CPU at the time of
    the panic.
    """

    def __init__(self):
        super(acttrace, self).__init__("acttrace", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        # Save the current thread so that we can switch back after.
        curthread = gdb.selected_thread()

        for pcpu in pcpu_foreach():
            td = pcpu['pc_curthread']
            tid = td['td_tid']

            gdb_thread = tid_to_gdb_thread(tid)
            if gdb_thread is None:
                raise gdb.error(f"failed to find GDB thread with TID {tid}")
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
