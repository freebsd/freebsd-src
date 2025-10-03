#
# Copyright (c) 2025 Mark Johnston <markj@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

import gdb

cmds = ["acttrace",
        "p $V(\"tcbinfo\")",
        "p $V(\"tcbinfo\", vnet0)",
        "p $V(\"pf_status\")",
        "p $V(\"pf_status\", \"gdbselftest\")",
        "p $PCPU(\"curthread\")",
        "p $PCPU(\"curthread\", 0)",
        "p/x $PCPU(\"hardclocktime\", 1)",
        "p $PCPU(\"pqbatch\")[0][0]",
        "p $PCPU(\"ss\", 1)",
        ]

for cmd in cmds:
    try:
        print(f"Running command: '{cmd}'")
        gdb.execute(cmd)
    except gdb.error as e:
        print(f"Command '{cmd}' failed: {e}")
        break

# We didn't hit any unexpected errors.  This isn't as good as actually
# verifying the output, but it's better than nothing.
print("Everything seems OK")
