#!/bin/sh

#
# Copyright (c) 2016 EMC Corp.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# Sched fuzz test scenario.

# "panic: sleeping thread" seen:
# https://people.freebsd.org/~pho/stress/log/schedfuzz.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

sysctl kern.sched_fuzz.prof_on > /dev/null 2>&1 || exit 0

: ${pct1:=0.01}
: ${pct2:=0.1}
export pct1 pct2

seton() {
	sysctl kern.sched_fuzz.sched_fuzz_scheduler="${pct1}%return(1)"
	sysctl kern.sched_fuzz.sched_fuzz_sx_slock_delay="${pct1}%schedfuzz(82)"
	sysctl kern.sched_fuzz.sched_fuzz_sx_xlock_delay="${pct1}%schedfuzz(82)"
	sysctl kern.sched_fuzz.sched_fuzz_sx_slock_sleep="${pct1}%schedfuzz(82)"
	sysctl kern.sched_fuzz.sched_fuzz_sx_xlock_sleep="${pct1}%schedfuzz(82)"
	sysctl kern.sched_fuzz.sched_fuzz_mtx_spin_lock="${pct1}%schedfuzz(44)"
	sysctl kern.sched_fuzz.sched_fuzz_mtx_spin_unlock="${pct1}%schedfuzz(44)"
	sysctl kern.sched_fuzz.sched_fuzz_malloc="${pct1}%schedfuzz(62)"
	sysctl kern.sched_fuzz.sched_fuzz_cv_wait="${pct2}%schedfuzz(100)"
	sysctl kern.sched_fuzz.sched_fuzz_cv_timed_wait="${pct2}%schedfuzz(100)"
	sysctl kern.sched_fuzz.sched_fuzz_cv_timed_wait_sig="${pct2}%schedfuzz(100)"
	sysctl kern.sched_fuzz.sched_fuzz_mtx_lock="${pct1}%schedfuzz(22)"
	sysctl kern.sched_fuzz.sched_fuzz_mtx_unlock="${pct1}%schedfuzz(22)"
	sysctl kern.sched_fuzz.sched_fuzz_sleepq="${pct1}%schedfuzz(150)"

	sysctl kern.sched_fuzz.prof_on=1
}

setoff() {
	sysctl kern.sched_fuzz.prof_on=0
}

if [ $# -eq 1 ]; then
	case "$1" in
	on)	seton
		;;
	off)	setoff
		;;
	*)	echo "Usage: $0 [on|off]"
		exit 1
		;;
	esac
else
	seton
	./procfs.sh
	setoff
fi
