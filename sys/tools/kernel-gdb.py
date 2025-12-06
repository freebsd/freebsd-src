#
# Copyright (c) 2025 Mark Johnston <markj@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

import importlib
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), "gdb"))

modules = [
    "acttrace",
    "freebsd",
    "pcpu",
    "vnet"
]


def reload_modules(modules):
    for mod in modules:
        if mod in sys.modules:
            importlib.reload(sys.modules[mod])
        else:
            importlib.import_module(mod)

reload_modules(modules)


class reload(gdb.Command):
    """
    Reload the FreeBSD kernel GDB helper scripts.
    """
    def __init__(self):
        super(reload, self).__init__("kgdb-reload", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        reload_modules(modules)


# Register the reload command with gdb.
reload()
