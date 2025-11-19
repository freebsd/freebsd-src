#
# Copyright (c) 2025 Mark Johnston <markj@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

import gdb
from freebsd import *


class pcpu(gdb.Function):
    """
    A function to look up PCPU and DPCPU fields by name.

    To look up the value of the PCPU field foo on CPU n, use
    $PCPU("foo", n).  This works for DPCPU fields too.  If the CPU ID is
    omitted, and the currently selected thread is on-CPU, that CPU is
    used, otherwise an error is raised.
    """

    def __init__(self):
        super(pcpu, self).__init__("PCPU")

    def invoke(self, field, cpuid=-1):
        if cpuid == -1:
            cpuid = tdfind(gdb.selected_thread().ptid[2])['td_oncpu']
            if cpuid == -1:
                raise gdb.error("Currently selected thread is off-CPU")
            if cpuid < 0 or cpuid > symval("mp_maxid"):
                raise gdb.error(f"Currently selected on invalid CPU {cpuid}")
        pcpu = symval("cpuid_to_pcpu")[cpuid]

        # Are we dealing with a PCPU or DPCPU field?
        field = field.string()
        for f in gdb.lookup_type("struct pcpu").fields():
            if f.name == "pc_" + field:
                return pcpu["pc_" + field]

        def uintptr_t(val):
            return val.cast(gdb.lookup_type("uintptr_t"))

        # We're dealing with a DPCPU field.  This is handled similarly
        # to VNET symbols, see vnet.py for comments.
        pcpu_base = pcpu['pc_dynamic']
        pcpu_entry = symval("pcpu_entry_" + field)
        pcpu_entry_addr = uintptr_t(pcpu_entry.address)

        for lf in linker_file_foreach():
            block = gdb.block_for_pc(lf['ops']['cls']['methods'][0]['func'])
            elf_file_t = gdb.lookup_type("elf_file_t", block).target()
            ef = lf.cast(elf_file_t)

            file_type = lf['ops']['cls']['name'].string()
            if file_type == "elf64":
                start = uintptr_t(ef['pcpu_start'])
                if start == 0:
                    continue
                end = uintptr_t(ef['pcpu_stop'])
                base = uintptr_t(ef['pcpu_base'])
            elif file_type == "elf64_obj":
                for i in range(ef['nprogtab']):
                    pe = ef['progtab'][i]
                    if pe['name'].string() == "set_pcpu":
                        start = uintptr_t(pe['origaddr'])
                        end = start + uintptr_t(pe['size'])
                        base = uintptr_t(pe['addr'])
                        break
                else:
                    continue
            else:
                path = lf['pathname'].string()
                raise gdb.error(f"{path} has unexpected linker file type {file_type}")

            if pcpu_entry_addr >= start and pcpu_entry_addr < end:
                obj = gdb.Value(pcpu_base + pcpu_entry_addr - start + base)
                return obj.cast(pcpu_entry.type.pointer()).dereference()


# Register with gdb.
pcpu()
