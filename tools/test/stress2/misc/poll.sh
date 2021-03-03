#!/bin/sh

# A pipe_poll() regression test.
# Python test scenario by Alexander Motin <mav@FreeBSD.org>
# https://reviews.freebsd.org/D21333

# Hang seen:
# $ procstat -k 19529
#   PID    TID COMM        KSTACK
# 19529 101381 python3.7 - ... _sleep kqueue_kevent kern_kevent_fp kern_kevent kern_kevent_generic sys_kevent amd64_syscall fast_syscall_common
# 19529 101630 python3.7 - ... _sleep pipe_read dofileread kern_readv sys_read amd64_syscall fast_syscall_common
# 19529 101631 python3.7 - ... _sleep umtxq_sleep do_sem2_wait __umtx_op_sem2_wait amd64_syscall fast_syscall_common
# $

# Fixed by r351348

[ -z "`type python3 2>/dev/null`" ] && exit 0
cat > /tmp/poll.py <<EOF
#!/usr/local/bin/python3

import concurrent.futures
import asyncio

procpool = concurrent.futures.ProcessPoolExecutor(
    max_workers=1,
)

def x():
    return ['x'] * 10241

async def say():
    for i in range(100000):
        await asyncio.get_event_loop().run_in_executor(procpool, x)
        print(i)

loop = asyncio.get_event_loop()
loop.run_until_complete(say())
loop.close()
EOF
chmod +x /tmp/poll.py

log=/tmp/poll.log
start=`date +%s`
cpuset -l 0 /tmp/poll.py > $log &
pid=$!
sleep 60
s1=`wc -l < $log`
sleep 60
s2=`wc -l < $log`
while pgrep -qf poll.py; do pkill -f poll.py; done
wait $pid
[ $s2 -gt $s1 ] && s=0 || s=1

rm -f /tmp/poll.py $log
exit $s

dtrace -wn '*::pipe_poll:entry {@rw[execname,probefunc] = count(); }'
