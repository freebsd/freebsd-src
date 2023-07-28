/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1994-1996 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/signal.h>
#include <x86/specialreg.h>
#include <x86/trap.h>
#include <x86/x86_var.h>

#include <x86/linux/linux_x86.h>

#define LINUX_T_UNKNOWN  255
static int _bsd_to_linux_trapcode[] = {
	LINUX_T_UNKNOWN,	/* 0 */
	6,			/* 1  T_PRIVINFLT */
	LINUX_T_UNKNOWN,	/* 2 */
	3,			/* 3  T_BPTFLT */
	LINUX_T_UNKNOWN,	/* 4 */
	LINUX_T_UNKNOWN,	/* 5 */
	16,			/* 6  T_ARITHTRAP */
	254,			/* 7  T_ASTFLT */
	LINUX_T_UNKNOWN,	/* 8 */
	13,			/* 9  T_PROTFLT */
	1,			/* 10 T_TRCTRAP */
	LINUX_T_UNKNOWN,	/* 11 */
	14,			/* 12 T_PAGEFLT */
	LINUX_T_UNKNOWN,	/* 13 */
	17,			/* 14 T_ALIGNFLT */
	LINUX_T_UNKNOWN,	/* 15 */
	LINUX_T_UNKNOWN,	/* 16 */
	LINUX_T_UNKNOWN,	/* 17 */
	0,			/* 18 T_DIVIDE */
	2,			/* 19 T_NMI */
	4,			/* 20 T_OFLOW */
	5,			/* 21 T_BOUND */
	7,			/* 22 T_DNA */
	8,			/* 23 T_DOUBLEFLT */
	9,			/* 24 T_FPOPFLT */
	10,			/* 25 T_TSSFLT */
	11,			/* 26 T_SEGNPFLT */
	12,			/* 27 T_STKFLT */
	18,			/* 28 T_MCHK */
	19,			/* 29 T_XMMFLT */
	15			/* 30 T_RESERVED */
};

/*
 * If FreeBSD & Linux have a difference of opinion about what a trap
 * means, deal with it here.
 */
int
linux_translate_traps(int signal, int trap_code)
{
	if (signal != SIGBUS)
		return (signal);
	switch (trap_code) {
	case T_PROTFLT:
	case T_TSSFLT:
	case T_DOUBLEFLT:
	case T_PAGEFLT:
		return (SIGSEGV);
	default:
		return (signal);
	}
}

int
bsd_to_linux_trapcode(int code)
{

	return (code < nitems(_bsd_to_linux_trapcode) ?
	    _bsd_to_linux_trapcode[code] : LINUX_T_UNKNOWN);
}

u_int
linux_x86_elf_hwcap2(void)
{
	static u_int elf_hwcap2 = 0;
	static bool elf_hwcap2_valid = false;

	if (!elf_hwcap2_valid) {
		if ((cpu_stdext_feature & CPUID_STDEXT_FSGSBASE) != 0)
			elf_hwcap2 |= LINUX_HWCAP2_FSGSBASE;
		elf_hwcap2_valid = true;
	}
	return (elf_hwcap2);
}
