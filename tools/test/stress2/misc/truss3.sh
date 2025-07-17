#!/bin/sh

# Regression test from:
# https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=249257
# by Edward Tomasz Napierala <trasz@freebsd.org>

# panic: pgrp 2191 0xfffff8000585c900 pg_jobc 1 cnt 2
# cpuid = 10
# time = 1599941578
# KDB: stack backtrace:
# db_trace_self_wrapper() at db_trace_self_wrapper+0x2b/frame 0xfffffe0101b378f0
# vpanic() at vpanic+0x182/frame 0xfffffe0101b37940
# panic() at panic+0x43/frame 0xfffffe0101b379a0
# check_pgrp_jobc() at check_pgrp_jobc+0x124/frame 0xfffffe0101b379e0
# doenterpgrp() at doenterpgrp+0xc6/frame 0xfffffe0101b37a30
# enterpgrp() at enterpgrp+0x39e/frame 0xfffffe0101b37a80
# sys_setpgid() at sys_setpgid+0x219/frame 0xfffffe0101b37ad0
# amd64_syscall() at amd64_syscall+0x159/frame 0xfffffe0101b37bf0
# fast_syscall_common() at fast_syscall_common+0xf8/frame 0xfffffe0101b37bf0
# --- syscall (82, FreeBSD ELF64, sys_setpgid), rip = 0x8003745ba, rsp = 0x7fffffffe638, rbp = 0x7fffffffe670 ---
# KDB: enter: panic
# [ thread pid 2191 tid 100251 ]
# Stopped at      kdb_enter+0x37: movq    $0,0x10b29f6(%rip)
# db> x/s version
# version: FreeBSD 13.0-CURRENT #0 r365671: Sat Sep 12 22:01:12 CEST 2020
# pho@t1.osted.lan:/usr/src/sys/amd64/compile/PHO\012
# db>

# Fixed by r365814

# Needs a controlling terminal for job control to be active
[ -t 1 ] || { echo "Not a tty"; exit 0; }

set -i
echo id | truss -o /dev/null -f /bin/sh -i
exit 0
