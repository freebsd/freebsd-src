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
 * THIS SOFTWARE IS PROVIDED BY JOHN BALDWIN AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JOHN BALDWIN OR THE VOICES IN HIS HEAD
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

#include "opt_ddb.h"
#include "opt_ktr.h"

#include <sys/param.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/libkern.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <machine/globals.h>
#include <machine/stdarg.h>

#include <ddb/ddb.h>

#ifndef KTR_ENTRIES
#define	KTR_ENTRIES	1024
#endif

#ifndef KTR_MASK
#define	KTR_MASK	(KTR_GEN)
#endif

#ifndef KTR_CPUMASK
#define	KTR_CPUMASK	(~0)
#endif

#ifdef SMP
#define KTR_CPU		PCPU_GET(cpuid)
#else
#define KTR_CPU		0
#endif

#ifdef KTR_EXTEND
#define KTR_EXTEND_DEFAULT	1
#else
#define KTR_EXTEND_DEFAULT	0
#endif

#ifdef KTR_VERBOSE
#define KTR_VERBOSE_DEFAULT	1
#else
#define KTR_VERBOSE_DEFAULT	0
#endif

SYSCTL_NODE(_debug, OID_AUTO, ktr, CTLFLAG_RD, 0, "KTR options");

/*
 * This variable is used only by gdb to work out what fields are in
 * ktr_entry.
 */
int     ktr_extend = KTR_EXTEND_DEFAULT;
SYSCTL_INT(_debug_ktr, OID_AUTO, extend, CTLFLAG_RD, &ktr_extend, 0, "");

int	ktr_cpumask = KTR_CPUMASK;
TUNABLE_INT("debug.ktr.cpumask", &ktr_cpumask);
SYSCTL_INT(_debug_ktr, OID_AUTO, cpumask, CTLFLAG_RW, &ktr_cpumask, 0, "");

int	ktr_mask = KTR_MASK;
TUNABLE_INT("debug.ktr.mask", &ktr_mask);
SYSCTL_INT(_debug_ktr, OID_AUTO, mask, CTLFLAG_RW, &ktr_mask, 0, "");

int	ktr_entries = KTR_ENTRIES;
SYSCTL_INT(_debug_ktr, OID_AUTO, entries, CTLFLAG_RD, &ktr_entries, 0, "");

volatile int	ktr_idx = 0;
struct	ktr_entry ktr_buf[KTR_ENTRIES];

int	ktr_verbose = KTR_VERBOSE_DEFAULT;
TUNABLE_INT("debug.ktr.verbose", &ktr_verbose);
SYSCTL_INT(_debug_ktr, OID_AUTO, verbose, CTLFLAG_RW, &ktr_verbose, 0, "");

#ifdef KTR
#ifdef KTR_EXTEND
void
ktr_tracepoint(u_int mask, const char *filename, u_int line,
	       const char *format, ...)
#else
void
ktr_tracepoint(u_int mask, const char *format, u_long arg1, u_long arg2,
	       u_long arg3, u_long arg4, u_long arg5)
#endif
{
	struct ktr_entry *entry;
	int newindex, saveindex;
	critical_t savecrit;
#ifdef KTR_EXTEND
	va_list ap;
#endif

	if (panicstr)
		return;
	if ((ktr_mask & mask) == 0)
		return;
	savecrit = critical_enter();
	if (((1 << KTR_CPU) & ktr_cpumask) == 0) {
		critical_exit(savecrit);
		return;
	}
	atomic_clear_int(&ktr_cpumask, 1 << KTR_CPU);
	do {
		saveindex = ktr_idx;
		newindex = (saveindex + 1) & (KTR_ENTRIES - 1);
	} while (atomic_cmpset_rel_int(&ktr_idx, saveindex, newindex) == 0);
	entry = &ktr_buf[saveindex];
	/*
	 * XXX: The ktr_cpumask atomic ops should make this unnecessary.
	 */
	if ((ktr_mask & (KTR_LOCK | KTR_WITNESS)) != 0)
		getnanotime(&entry->ktr_tv);
	else
		nanotime(&entry->ktr_tv);
	atomic_set_int(&ktr_cpumask, 1 << KTR_CPU);
	entry->ktr_cpu = KTR_CPU;
	critical_exit(savecrit);
#ifdef KTR_EXTEND
	entry->ktr_filename = filename;
	entry->ktr_line = line;
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

#ifdef DDB

struct tstate {
	int	cur;
	int	first;
};
static	struct tstate tstate;
static	int db_ktr_verbose;
static	int db_mach_vtrace(void);

#define	NUM_LINES_PER_PAGE	18

DB_SHOW_COMMAND(ktr, db_ktr_all)
{
	int	c, lines;

	lines = NUM_LINES_PER_PAGE;
	tstate.cur = (ktr_idx - 1) & (KTR_ENTRIES - 1);
	tstate.first = -1;
	if (strcmp(modif, "v") == 0)
		db_ktr_verbose = 1;
	else
		db_ktr_verbose = 0;
	while (db_mach_vtrace())
		if (--lines == 0) {
			db_printf("--More--");
			c = cngetc();
			db_printf("\r");
			switch (c) {
			case '\n':	/* one more line */
				lines = 1;
				break;
			case ' ':	/* one more page */
				lines = NUM_LINES_PER_PAGE;
				break;
			default:
				db_printf("\n");
				return;
			}
		}
}

static int
db_mach_vtrace(void)
{
	struct ktr_entry	*kp;

	if (tstate.cur == tstate.first) {
		db_printf("--- End of trace buffer ---\n");
		return (0);
	}
	kp = &ktr_buf[tstate.cur];

	/* Skip over unused entries. */
#ifdef KTR_EXTEND
	if (kp->ktr_desc[0] == '\0') {
#else
	if (kp->ktr_desc == NULL) {
#endif
		db_printf("--- End of trace buffer ---\n");
		return (0);
	}
	db_printf("%d: ", tstate.cur);
	if (db_ktr_verbose)
		db_printf("%4ld.%06ld ", (long)kp->ktr_tv.tv_sec,
		    kp->ktr_tv.tv_nsec / 1000);
#ifdef KTR_EXTEND
#ifdef SMP
	db_printf("cpu%d ", kp->ktr_cpu);
#endif
	if (db_ktr_verbose)
		db_printf("%s.%d\t", kp->ktr_filename, kp->ktr_line);
	db_printf("%s", kp->ktr_desc);
#else
	db_printf(kp->ktr_desc, kp->ktr_parm1, kp->ktr_parm2, kp->ktr_parm3,
	    kp->ktr_parm4, kp->ktr_parm5);
#endif
	db_printf("\n");

	if (tstate.first == -1)
		tstate.first = tstate.cur;

	if (--tstate.cur < 0)
		tstate.cur = KTR_ENTRIES - 1;

	return (1);
}

#endif	/* DDB */
#endif	/* KTR */
