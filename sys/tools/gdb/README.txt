This directory contains Python scripts that can be loaded by GDB to help debug
FreeBSD kernel crashes.

Add new commands and functions in their own files.  Functions with general
utility should be added to freebsd.py.  sys/tools/kernel-gdb.py is installed
into the kernel debug directory (typically /usr/lib/debug/boot/kernel).  It will
be automatically loaded by kgdb when opening a vmcore, so if you add new GDB
commands or functions, that script should be updated to import them, and you
should document them here.

When improving these scripts, you can use the "kgdb-reload" command to reload
them from /usr/lib/debug/boot/kernel/gdb/*.

To provide some rudimentary testing, selftest.py tries to exercise all of the
commands and functions defined here.  To use it, run selftest.sh to panic the
system.  Then, create a kernel dump or attach to the panicked kernel, and invoke
the script with "python import selftest" in (k)gdb.

Commands:
acttrace	Display a backtrace for all on-CPU threads
kgdb-reload     Reload all gdb modules, useful when developing the modules
                themselves.

Functions:
$PCPU(<field>[, <cpuid>])	Display the value of a PCPU/DPCPU field
$V(<variable>[, <vnet>])	Display the value of a VNET variable
