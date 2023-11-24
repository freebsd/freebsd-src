#!/bin/sh

#
# Copyright (c) 2009 Peter Holm <pho@FreeBSD.org>
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

# Test the amd64 implementation of:
# 1. Per-process private ldt and corresponding i386 arch syscalls.
# 2. Per-process private io permission bitmap and corresponding
#    i386 arch syscalls.
# 3. Sigcontext

# The tests must be compiled on i386 and run on amd64

# All tests by kib@

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

cd /tmp
if [ "`uname -p`" = "i386" ]; then
	cat > ldt.c <<EOF
/* \$Id: ldt.c,v 1.8 2008/11/01 21:14:59 kostik Exp kostik \$ */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <machine/segments.h>
#include <machine/sysarch.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

char stack[64 * 1024];

char a[1];

int
s2ds(int sel)
{

	return (LSEL(sel, SEL_UPL));
}

unsigned char
readbyte(int sel, int offset)
{
	unsigned char res;

	__asm__ volatile(
	    "\tpushl	%%es\n"
	    "\tmovl	%1,%%es\n"
	    "\tmovb	%%es:(%2),%0\n"
	    "\tpopl	%%es\n"
	    : "=r"(res) : "r"(s2ds(sel)), "r"(offset));

	return (res);
}

void
writebyte(int sel, int offset, unsigned char val)
{

	__asm__ volatile(
	    "\tpushl	%%es\n"
	    "\tmovl	%0,%%es\n"
	    "\tmovb	%2,%%es:(%1)\n"
	    "\tpopl	%%es\n"
	    : : "r"(s2ds(sel)), "r"(offset), "r"(val) : "memory");
}

int
alloc_sel(char *base, size_t len, int type, int p)
{
	int sel;
	union descriptor descs[1], descsk[1];
	uintptr_t pb;

	memset(descs, 0, sizeof(descs));
	if (len > PAGE_SIZE) {
		len = roundup(len, PAGE_SIZE);
		len /= PAGE_SIZE;
		descs[0].sd.sd_lolimit = len & 0xffff;
		descs[0].sd.sd_hilimit = (len >> 16) & 0xf;
		descs[0].sd.sd_gran = 1;
	} else {
		descs[0].sd.sd_lolimit = len;
		descs[0].sd.sd_hilimit = 0;
		descs[0].sd.sd_gran = 0;
	}
	pb = (uintptr_t)base;
	descs[0].sd.sd_lobase = pb & 0xffffff;
	descs[0].sd.sd_hibase = (pb >> 24) & 0xff;
	descs[0].sd.sd_type = type;
	descs[0].sd.sd_dpl = SEL_UPL;
	descs[0].sd.sd_p = p;
	descs[0].sd.sd_def32 = 1;

	if ((sel = i386_set_ldt(LDT_AUTO_ALLOC, descs, 1)) == -1)
		fprintf(stderr, "i386_set_ldt: %s\n", strerror(errno));
	else if (i386_get_ldt(sel, descsk, 1) == -1) {
		fprintf(stderr, "i386_get_ldt: %s\n", strerror(errno));
		sel = -1;
	} else if (memcmp(descs, descsk, sizeof(descs)) != 0) {
		fprintf(stderr, "descs != descsk\n");
		sel = -1;
	} else
		fprintf(stderr, "selector %d\n", sel);

	return (sel);
}

int
test1(int tnum, int sel)
{
	unsigned char ar;

	writebyte(sel, 0, '1');
	ar = readbyte(sel, 0);
	if (ar == '1')
		fprintf(stderr, "test %d.1 ok\n", tnum);
	else
		fprintf(stderr, "test%d.1 failed, ar %x\n", tnum, ar);
	writebyte(sel, 0, '2');
	ar = readbyte(sel, 0);
	if (ar == '2')
		fprintf(stderr, "test %d.2 ok\n", tnum);
	else
		fprintf(stderr, "test%d.2 failed, ar %x\n", tnum, ar);
	return (sel);
}

int
test2_func(void *arg)
{
	int *sel;

	sel = arg;
	test1(2, *sel);
	rfork(0);
	test1(3, *sel);
	return (0);
}

void
test2(int sel)
{
	pid_t r;
	int status;

	r = rfork_thread(RFPROC | RFMEM, stack + sizeof(stack),
	    test2_func, &sel);
	if (r == -1) {
		fprintf(stderr, "rfork(RFPROC): %s\n", strerror(errno));
		return;
	} else {
		waitpid(r, &status, 0);
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "test2: child terminated by %s\n",
			    strsignal(WTERMSIG(status)));
		}
	}
}

int
main(int argc, char *argv[])
{
	int sel;

	sel = alloc_sel(a, 1, SDT_MEMRWA, 1);
	if (sel == -1)
		return (1);

	test1(1, sel);
	test2(sel);
	return (0);
}
EOF
	cc -o ldt_static_i386 -Wall -static ldt.c
	rm ldt.c

	cat > ioperm.c <<EOF
/* \$Id: ioperm.c,v 1.3 2008/11/02 15:43:33 kostik Exp \$ */

#include <machine/sysarch.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned int port_num = 0x130;

unsigned char
inb(unsigned int port)
{
	unsigned char data;

	__asm __volatile("inb %%dx,%0" : "=a" (data) : "d" (port));
	return (data);
}

void
sigbus_handler(int signo)
{

	fprintf(stderr, "Got SIGBUS\n");
	exit(0);
}

int
main(int argc, char *argv[])
{
	struct sigaction sa;
	unsigned int length1;
	int enable1;

	if (i386_get_ioperm(port_num, &length1, &enable1) == -1) {
		fprintf(stderr, "get 1: %s\n", strerror(errno));
		return (1);
	}
	if (length1 != 0 && enable1 != 0) {
		fprintf(stderr, "enable1: enabled\n");
		return (1);
	}
	if (i386_set_ioperm(port_num, 1, 1) == -1) {
		fprintf(stderr, "set 1: %s\n", strerror(errno));
		return (1);
	}
	inb(port_num);
	if (i386_set_ioperm(port_num, 1, 0) == -1) {
		fprintf(stderr, "set 2: %s\n", strerror(errno));
		return (1);
	}
	if (i386_get_ioperm(port_num, &length1, &enable1) == -1) {
		fprintf(stderr, "get 1: %s\n", strerror(errno));
		return (1);
	}
	if (enable1 != 0) {
		fprintf(stderr, "enable2: enabled\n");
		return (1);
	}
	fprintf(stderr, "And now we should get SIGBUS\n");
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigbus_handler;
	if (sigaction(SIGBUS, &sa, NULL) == -1) {
		fprintf(stderr, "sigaction(SIGBUS): %s\n", strerror(errno));
		return (1);
	}
	inb(port_num);

	return (0);
}
EOF
	cc -o ioperm_static_i386 -Wall -static ioperm.c
	rm ioperm.c

	cat > fault.c <<EOF
/* \$Id: fault.c,v 1.5 2008/10/28 17:39:16 kostik Exp \$ */

#include <sys/types.h>
#include <sys/ucontext.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern char *fault_instr;
int run;

void
sigsegv_sigaction(int signo, siginfo_t *si, void *c)
{
	ucontext_t *uc;
	mcontext_t *mc;

	uc = c;
	mc = &uc->uc_mcontext;
	printf("SIGSEGV run %d err %x ds %x ss %x es %x fs %x gs %x\n",
	    run, mc->mc_err, mc->mc_ds, mc->mc_ss, mc->mc_es, mc->mc_fs,
	    mc->mc_gs);
	switch (run) {
	case 0:
		mc->mc_ds = 0x1111;
		break;
	case 1:
		mc->mc_es = 0x1111;
		break;
	case 2:
		mc->mc_fs = 0x1111;
		break;
	case 3:
		mc->mc_gs = 0x1111;
		break;
	case 4:
		mc->mc_ss = 0x1111;
		break;
	case 5:
		_exit(11);
	}
	run++;
}

void
fault(void)
{

	__asm__ volatile(".globl\tfault_instr;fault_instr:\ttestl\t\$0,0\n");
}

int
main(int argc, char *argv[])
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = sigsegv_sigaction;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGSEGV, &sa, NULL) == -1) {
		fprintf(stderr, "sigaction: %s\n", strerror(errno));
		return (1);
	}
	if (sigaction(SIGBUS, &sa, NULL) == -1) {
		fprintf(stderr, "sigaction: %s\n", strerror(errno));
		return (1);
	}

	fault();

	return (0);
}
EOF
	cc -o fault_static_i386 -Wall -static fault.c
	rm fault.c
fi

if [ "`uname -p`" = "amd64" ]; then
	[ -x ldt_static_i386 ]    && ./ldt_static_i386
	[ -x ioperm_static_i386 ] && ./ioperm_static_i386
	[ -x fault_static_i386 ]  && { ./fault_static_i386; rm fault_static_i386.core; }
fi
exit 0
