#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2017 Konstantin Belousov <kib@FreeBSD.org>
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

# Maxime Villard <max@m00nbsd.net> spotted this problem:
# Issue with segment registers on freebsd-i386

# Fixed in r323722

[ `uname -m` = "i386" ] || exit 0

. ../default.cfg

cat > /tmp/mvillard_nest.c <<EOF
/* $Id: mvillard_nest.c,v 1.2 2017/09/15 14:33:30 kostik Exp kostik $ */

#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/ucontext.h>
#include <machine/atomic.h>
#include <machine/segments.h>
#include <machine/sysarch.h>
#include <err.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

static volatile u_int b, s;

static void *
dealloc_ldt(void *arg __unused)
{
	u_int sl;

	for (;;) {
		while (atomic_load_acq_int(&b) == 0)
			;
		sl = s;
		s = 0;
		if (sl != 0)
			i386_set_ldt(sl, NULL, 1);
		atomic_store_rel_int(&b, 0);
	}
	return (NULL);
}

static void
func(void)
{
	union descriptor desc;
	u_int sel, sl;

	bzero(&desc, sizeof(desc));
	desc.sd.sd_type = SDT_MEMRWA;
	desc.sd.sd_dpl = SEL_UPL;
	desc.sd.sd_p = 1;
	desc.sd.sd_def32 = 1;
	desc.sd.sd_gran = 1;
	desc.sd.sd_lolimit = 0xffff;
	desc.sd.sd_hilimit = 0xf;
	sl = i386_set_ldt(LDT_AUTO_ALLOC, &desc, 1);
	if ((int)sl == -1)
		err(1, "i386_set_ldt");
	sel = LSEL(sl, SEL_UPL);
	s = sl;
	__asm volatile("movw\t%w0,%%es" : : "r" (sel));
	atomic_store_rel_int(&b, 1);
	while (atomic_load_acq_int(&b) != 0)
		;
	getpid();
}

static void
sigsegv_handler(int signo __unused, siginfo_t *si __unused, void *rctx)
{
	ucontext_t *uc;

	uc = rctx;
	uc->uc_mcontext.mc_es = uc->uc_mcontext.mc_ds;
}

int
main(void)
{
	pthread_t thr;
	time_t start;
	struct sigaction sa;
	int error;

	bzero(&sa, sizeof(sa));
	sa.sa_sigaction = sigsegv_handler;
	sa.sa_flags = SA_SIGINFO;
	error = sigaction(SIGSEGV, &sa, NULL);
	if (error != 0)
		err(1, "sigaction SIGSEGV");
	error = sigaction(SIGBUS, &sa, NULL);
	if (error != 0)
		err(1, "sigaction SIGBUS");

	error = pthread_create(&thr, NULL, dealloc_ldt, NULL);
	if (error != 0)
		errc(1, error, "pthread_create");

	start = time(NULL);
	while (time(NULL) - start < 120)
	     func();
}
EOF

mycc -o /tmp/mvillard_nest -Wall -Wextra -O2 -g /tmp/mvillard_nest.c \
    -l pthread || exit 1
rm /tmp/mvillard_nest.c

/tmp/mvillard_nest; s=$?

rm /tmp/mvillard_nest
exit $s
