/*
 * Copyright (c) 2000
 *	John Baldwin <jhb@FreeBSD.org>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * This module holds the global variables used by KTR and the ktr_tracepoint()
 * function that does the actual tracing.
 */

#include "opt_ktr.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/ktr.h>
#include <sys/libkern.h>
#include <sys/linker_set.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <machine/globals.h>
#include <machine/stdarg.h>

#ifndef KTR_MASK
#define	KTR_MASK	(KTR_GEN)
#endif

#ifndef KTR_CPUMASK
#define	KTR_CPUMASK	(~0)
#endif

#ifdef SMP
#define KTR_CPU		cpuid
#else
#define KTR_CPU		0
#endif

#ifdef KTR_EXTEND
/*
 * This variable is used only by gdb to work out what fields are in
 * ktr_entry.
 */
int     ktr_extend = 1;
SYSCTL_INT(_debug, OID_AUTO, ktr_extend, CTLFLAG_RD, &ktr_extend, 1, "");
#else
int     ktr_extend = 0;
SYSCTL_INT(_debug, OID_AUTO, ktr_extend, CTLFLAG_RD, &ktr_extend, 0, "");
#endif	/* KTR_EXTEND */

int	ktr_cpumask = KTR_CPUMASK;
SYSCTL_INT(_debug, OID_AUTO, ktr_cpumask, CTLFLAG_RW, &ktr_cpumask, KTR_CPUMASK, "");

int	ktr_mask = KTR_MASK;
SYSCTL_INT(_debug, OID_AUTO, ktr_mask, CTLFLAG_RW, &ktr_mask, KTR_MASK, "");

int	ktr_entries = KTR_ENTRIES;
SYSCTL_INT(_debug, OID_AUTO, ktr_entries, CTLFLAG_RD, &ktr_entries, KTR_ENTRIES, "");

volatile int	ktr_idx = 0;
struct	ktr_entry ktr_buf[KTR_ENTRIES];

#ifdef KTR_VERBOSE
int	ktr_verbose = 1;
#else
int	ktr_verbose = 0;
#endif
SYSCTL_INT(_debug, OID_AUTO, ktr_verbose, CTLFLAG_RW, &ktr_verbose, 0, "");

#ifdef KTR
#ifdef KTR_EXTEND
void
ktr_tracepoint(u_int mask, char *filename, u_int line, char *format, ...)
#else
void
ktr_tracepoint(u_int mask, char *format, u_long arg1, u_long arg2, u_long arg3,
	       u_long arg4, u_long arg5)
#endif
{
	struct ktr_entry *entry;
	int newindex, saveindex, saveintr;
#ifdef KTR_EXTEND
	va_list ap;
#endif

	if ((ktr_mask & mask) == 0)
		return;
#ifdef KTR_EXTEND
	if (((1 << KTR_CPU) & ktr_cpumask) == 0)
		return;
#endif
	saveintr = save_intr();
	disable_intr();
	do {
		saveindex = ktr_idx;
		newindex = (saveindex + 1) & (KTR_ENTRIES - 1);
	} while (atomic_cmpset_rel_int(&ktr_idx, saveindex, newindex) == 0);
	entry = &ktr_buf[saveindex];
	restore_intr(saveintr);
	nanotime(&entry->ktr_tv);
#ifdef KTR_EXTEND
	strncpy(entry->ktr_filename, filename, KTRFILENAMESIZE - 1);
	entry->ktr_filename[KTRFILENAMESIZE - 1] = '\0';
	entry->ktr_line = line;
	entry->ktr_cpu = KTR_CPU;
	va_start(ap, format);
	vsnprintf(entry->ktr_desc, KTRDESCSIZE, format, ap);
	va_end(ap);
	if (ktr_verbose) {
#ifdef SMP
		printf("cpu%d ", entry->ktr_cpu);
#endif
		if (ktr_verbose > 1)
			printf("%s.%d\t", entry->ktr_filename, entry->ktr_line);
		va_start(ap, format);
		vprintf(format, ap);
		printf("\n");
		va_end(ap);
	}
#else
	entry->ktr_desc = format;
	entry->ktr_parm1 = arg1;
	entry->ktr_parm2 = arg2;
	entry->ktr_parm3 = arg3;
	entry->ktr_parm4 = arg4;
	entry->ktr_parm5 = arg5;
#endif
}
#endif	/* KTR */
