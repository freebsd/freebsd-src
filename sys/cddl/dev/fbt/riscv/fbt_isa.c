/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 *
 * Portions Copyright 2006-2008 John Birrell jb@freebsd.org
 * Portions Copyright 2013 Justin Hibbits jhibbits@freebsd.org
 * Portions Copyright 2013 Howard Su howardsu@freebsd.org
 * Portions Copyright 2016-2018 Ruslan Bukin <br@bsdpad.com>
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/param.h>

#include <sys/dtrace.h>

#include <machine/riscvreg.h>
#include <machine/encoding.h>

#include "fbt.h"

#define	FBT_C_PATCHVAL		MATCH_C_EBREAK
#define	FBT_PATCHVAL		MATCH_EBREAK
#define	FBT_AFRAMES		5

int
fbt_invop(uintptr_t addr, struct trapframe *frame, uintptr_t rval)
{
	solaris_cpu_t *cpu;
	fbt_probe_t *fbt;

	cpu = &solaris_cpu[curcpu];
	fbt = fbt_probetab[FBT_ADDR2NDX(addr)];

	for (; fbt != NULL; fbt = fbt->fbtp_hashnext) {
		if ((uintptr_t)fbt->fbtp_patchpoint == addr) {
			cpu->cpu_dtrace_caller = frame->tf_ra - INSN_SIZE;

			if (fbt->fbtp_roffset == 0) {
				dtrace_probe(fbt->fbtp_id, frame->tf_a[0],
				    frame->tf_a[1], frame->tf_a[2],
				    frame->tf_a[3], frame->tf_a[4]);
			} else {
				dtrace_probe(fbt->fbtp_id, fbt->fbtp_roffset,
				    frame->tf_a[0], frame->tf_a[1], 0, 0);
			}

			cpu->cpu_dtrace_caller = 0;
			return (fbt->fbtp_savedval);
		}
	}

	return (0);
}

void
fbt_patch_tracepoint(fbt_probe_t *fbt, fbt_patchval_t val)
{

	switch(fbt->fbtp_patchval) {
	case FBT_C_PATCHVAL:
		*(uint16_t *)fbt->fbtp_patchpoint = (uint16_t)val;
		fence_i();
		break;
	case FBT_PATCHVAL:
		*fbt->fbtp_patchpoint = val;
		fence_i();
		break;
	};
}

int
fbt_provide_module_function(linker_file_t lf, int symindx,
    linker_symval_t *symval, void *opaque)
{
	fbt_probe_t *fbt, *retfbt;
	uint32_t *instr, *limit;
	const char *name;
	char *modname;
	int patchval;
	int rval;

	modname = opaque;
	name = symval->name;

	/* Check if function is excluded from instrumentation */
	if (fbt_excluded(name))
		return (0);

	/*
	 * Some assembly-language exception handlers are not suitable for
	 * instrumentation.
	 */
	if (strcmp(name, "cpu_exception_handler") == 0)
		return (0);
	if (strcmp(name, "cpu_exception_handler_user") == 0)
		return (0);
	if (strcmp(name, "cpu_exception_handler_supervisor") == 0)
		return (0);
	if (strcmp(name, "do_trap_supervisor") == 0)
		return (0);

	instr = (uint32_t *)(symval->value);
	limit = (uint32_t *)(symval->value + symval->size);

	/* Look for sd operation */
	for (; instr < limit; instr++) {
		/* Look for a non-compressed store of ra to sp */
		if (dtrace_instr_sdsp(&instr)) {
			rval = DTRACE_INVOP_SD;
			patchval = FBT_PATCHVAL;
			break;
		}

		/* Look for a 'C'-compressed store of ra to sp. */
		if (dtrace_instr_c_sdsp(&instr)) {
			rval = DTRACE_INVOP_C_SDSP;
			patchval = FBT_C_PATCHVAL;
			break;
		}
	}

	if (instr >= limit)
		return (0);

	fbt = malloc(sizeof (fbt_probe_t), M_FBT, M_WAITOK | M_ZERO);
	fbt->fbtp_name = name;
	fbt->fbtp_id = dtrace_probe_create(fbt_id, modname,
	    name, FBT_ENTRY, FBT_AFRAMES, fbt);
	fbt->fbtp_patchpoint = instr;
	fbt->fbtp_ctl = lf;
	fbt->fbtp_loadcnt = lf->loadcnt;
	fbt->fbtp_savedval = *instr;
	fbt->fbtp_patchval = patchval;
	fbt->fbtp_rval = rval;
	fbt->fbtp_symindx = symindx;

	fbt->fbtp_hashnext = fbt_probetab[FBT_ADDR2NDX(instr)];
	fbt_probetab[FBT_ADDR2NDX(instr)] = fbt;

	lf->fbt_nentries++;

	retfbt = NULL;
again:
	for (; instr < limit; instr++) {
		/* Look for non-compressed return */
		if (dtrace_instr_ret(&instr)) {
			rval = DTRACE_INVOP_RET;
			patchval = FBT_PATCHVAL;
			break;
		}

		/* Look for 'C'-compressed return */
		if (dtrace_instr_c_ret(&instr)) {
			rval = DTRACE_INVOP_C_RET;
			patchval = FBT_C_PATCHVAL;
			break;
		}
	}

	if (instr >= limit)
		return (0);

	/*
	 * We have a winner!
	 */
	fbt = malloc(sizeof (fbt_probe_t), M_FBT, M_WAITOK | M_ZERO);
	fbt->fbtp_name = name;
	if (retfbt == NULL) {
		fbt->fbtp_id = dtrace_probe_create(fbt_id, modname,
		    name, FBT_RETURN, FBT_AFRAMES, fbt);
	} else {
		retfbt->fbtp_probenext = fbt;
		fbt->fbtp_id = retfbt->fbtp_id;
	}
	retfbt = fbt;

	fbt->fbtp_patchpoint = instr;
	fbt->fbtp_ctl = lf;
	fbt->fbtp_loadcnt = lf->loadcnt;
	fbt->fbtp_symindx = symindx;
	fbt->fbtp_rval = rval;
	fbt->fbtp_roffset = (uintptr_t)instr - (uintptr_t)symval->value;
	fbt->fbtp_savedval = *instr;
	fbt->fbtp_patchval = patchval;
	fbt->fbtp_hashnext = fbt_probetab[FBT_ADDR2NDX(instr)];
	fbt_probetab[FBT_ADDR2NDX(instr)] = fbt;

	lf->fbt_nentries++;

	instr++;
	goto again;
}
