#
# Copyright (c) 2025 Mark Johnston <markj@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

import gdb
from freebsd import *


class vnet(gdb.Function):
    """
    A function to look up VNET variables by name.

    To look at the value of a VNET variable V_foo, print $V("foo").  The
    currently selected thread's VNET is used by default, but can be optionally
    specified as a second parameter, e.g., $V("foo", <vnet>), where <vnet> is a
    pointer to a struct vnet (e.g., vnet0 or allprison.tqh_first->pr_vnet) or a
    string naming a jail.
    """

    def __init__(self):
        super(vnet, self).__init__("V")

    def invoke(self, sym, vnet=None):
        sym = sym.string()
        if sym.startswith("V_"):
            sym = sym[len("V_"):]
        if gdb.lookup_symbol("sysctl___kern_features_vimage")[0] is None:
            return symval(sym)

        # Look up the VNET's base address.
        if vnet is None:
            vnet = tdfind(gdb.selected_thread().ptid[2])['td_vnet']
            if not vnet:
                # If curthread->td_vnet == NULL, vnet0 is the current vnet.
                vnet = symval("vnet0")
        elif vnet.type.is_string_like:
            vnet = vnet.string()
            for prison in tailq_foreach(symval("allprison"), "pr_list"):
                if prison['pr_name'].string() == vnet:
                    vnet = prison['pr_vnet']
                    break
            else:
                raise gdb.error(f"No prison named {vnet}")

        def uintptr_t(val):
            return val.cast(gdb.lookup_type("uintptr_t"))

        # Now the tricky part: compute the address of the symbol relative
        # to the selected VNET.  In the compiled kernel this is done at
        # load time by applying a magic transformation to relocations
        # against symbols in the vnet linker set.  Here we have to apply
        # the transformation manually.
        vnet_data_base = vnet['vnet_data_base']
        vnet_entry = symval("vnet_entry_" + sym)
        vnet_entry_addr = uintptr_t(vnet_entry.address)

        # First, which kernel module does the symbol belong to?
        for lf in linker_file_foreach():
            # Find the bounds of this linker file's VNET linker set.  The
            # struct containing the bounds depends on the type of the linker
            # file, and unfortunately both are called elf_file_t.  So we use a
            # PC value from the compilation unit (either link_elf.c or
            # link_elf_obj.c) to disambiguate.
            block = gdb.block_for_pc(lf['ops']['cls']['methods'][0]['func'])
            elf_file_t = gdb.lookup_type("elf_file_t", block).target()
            ef = lf.cast(elf_file_t)

            file_type = lf['ops']['cls']['name'].string()
            if file_type == "elf64":
                start = uintptr_t(ef['vnet_start'])
                if start == 0:
                    # This linker file doesn't have a VNET linker set.
                    continue
                end = uintptr_t(ef['vnet_stop'])
                base = uintptr_t(ef['vnet_base'])
            elif file_type == "elf64_obj":
                for i in range(ef['nprogtab']):
                    pe = ef['progtab'][i]
                    if pe['name'].string() == "set_vnet":
                        start = uintptr_t(pe['origaddr'])
                        end = start + uintptr_t(pe['size'])
                        base = uintptr_t(pe['addr'])
                        break
                else:
                    # This linker file doesn't have a VNET linker set.
                    continue
            else:
                path = lf['pathname'].string()
                raise gdb.error(f"{path} has unexpected linker file type {file_type}")

            if vnet_entry_addr >= start and vnet_entry_addr < end:
                # The symbol belongs to this linker file, so compute the final
                # address.
                obj = gdb.Value(vnet_data_base + vnet_entry_addr - start + base)
                return obj.cast(vnet_entry.type.pointer()).dereference()


# Register with gdb.
vnet()
