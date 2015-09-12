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
 *
 * $FreeBSD$
 *
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/cdefs.h>
#include <sys/param.h>

#include <sys/dtrace.h>

#include "fbt.h"

#define	FBT_PUSHL_EBP		0x55
#define	FBT_MOVL_ESP_EBP0_V0	0x8b
#define	FBT_MOVL_ESP_EBP1_V0	0xec
#define	FBT_MOVL_ESP_EBP0_V1	0x89
#define	FBT_MOVL_ESP_EBP1_V1	0xe5
#define	FBT_REX_RSP_RBP		0x48

#define	FBT_POPL_EBP		0x5d
#define	FBT_RET			0xc3
#define	FBT_RET_IMM16		0xc2
#define	FBT_LEAVE		0xc9

#ifdef __amd64__
#define	FBT_PATCHVAL		0xcc
#else
#define	FBT_PATCHVAL		0xf0
#endif

#define	FBT_ENTRY	"entry"
#define	FBT_RETURN	"return"

int
fbt_invop(uintptr_t addr, uintptr_t *stack, uintptr_t rval)
{
	solaris_cpu_t *cpu = &solaris_cpu[curcpu];
	uintptr_t stack0, stack1, stack2, stack3, stack4;
	fbt_probe_t *fbt = fbt_probetab[FBT_ADDR2NDX(addr)];

	for (; fbt != NULL; fbt = fbt->fbtp_hashnext) {
		if ((uintptr_t)fbt->fbtp_patchpoint == addr) {
			if (fbt->fbtp_roffset == 0) {
				int i = 0;
				/*
				 * When accessing the arguments on the stack,
				 * we must protect against accessing beyond
				 * the stack.  We can safely set NOFAULT here
				 * -- we know that interrupts are already
				 * disabled.
				 */
				DTRACE_CPUFLAG_SET(CPU_DTRACE_NOFAULT);
				cpu->cpu_dtrace_caller = stack[i++];
				stack0 = stack[i++];
				stack1 = stack[i++];
				stack2 = stack[i++];
				stack3 = stack[i++];
				stack4 = stack[i++];
				DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_NOFAULT |
				    CPU_DTRACE_BADADDR);

				dtrace_probe(fbt->fbtp_id, stack0, stack1,
				    stack2, stack3, stack4);

				cpu->cpu_dtrace_caller = 0;
			} else {
#ifdef __amd64__
				/*
				 * On amd64, we instrument the ret, not the
				 * leave.  We therefore need to set the caller
				 * to assure that the top frame of a stack()
				 * action is correct.
				 */
				DTRACE_CPUFLAG_SET(CPU_DTRACE_NOFAULT);
				cpu->cpu_dtrace_caller = stack[0];
				DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_NOFAULT |
				    CPU_DTRACE_BADADDR);
#endif

				dtrace_probe(fbt->fbtp_id, fbt->fbtp_roffset,
				    rval, 0, 0, 0);
				cpu->cpu_dtrace_caller = 0;
			}

			return (fbt->fbtp_rval);
		}
	}

	return (0);
}

void
fbt_patch_tracepoint(fbt_probe_t *fbt, fbt_patchval_t val)
{

	*fbt->fbtp_patchpoint = val;
}

int
fbt_provide_module_function(linker_file_t lf, int symindx,
    linker_symval_t *symval, void *opaque)
{
	char *modname = opaque;
	const char *name = symval->name;
	fbt_probe_t *fbt, *retfbt;
	int j;
	int size;
	uint8_t *instr, *limit;

	if ((strncmp(name, "dtrace_", 7) == 0 &&
	    strncmp(name, "dtrace_safe_", 12) != 0) ||
	    strcmp(name, "trap_check") == 0) {
		/*
		 * Anything beginning with "dtrace_" may be called
		 * from probe context unless it explicitly indicates
		 * that it won't be called from probe context by
		 * using the prefix "dtrace_safe_".
		 *
		 * Additionally, we avoid instrumenting trap_check() to avoid
		 * the possibility of generating a fault in probe context before
		 * DTrace's fault handler is called.
		 */
		return (0);
	}

	if (name[0] == '_' && name[1] == '_')
		return (0);

	size = symval->size;

	instr = (uint8_t *) symval->value;
	limit = (uint8_t *) symval->value + symval->size;

#ifdef __amd64__
	while (instr < limit) {
		if (*instr == FBT_PUSHL_EBP)
			break;

		if ((size = dtrace_instr_size(instr)) <= 0)
			break;

		instr += size;
	}

	if (instr >= limit || *instr != FBT_PUSHL_EBP) {
		/*
		 * We either don't save the frame pointer in this
		 * function, or we ran into some disassembly
		 * screw-up.  Either way, we bail.
		 */
		return (0);
	}
#else
	if (instr[0] != FBT_PUSHL_EBP)
		return (0);

	if (!(instr[1] == FBT_MOVL_ESP_EBP0_V0 &&
	    instr[2] == FBT_MOVL_ESP_EBP1_V0) &&
	    !(instr[1] == FBT_MOVL_ESP_EBP0_V1 &&
	    instr[2] == FBT_MOVL_ESP_EBP1_V1))
		return (0);
#endif

	fbt = malloc(sizeof (fbt_probe_t), M_FBT, M_WAITOK | M_ZERO);
	fbt->fbtp_name = name;
	fbt->fbtp_id = dtrace_probe_create(fbt_id, modname,
	    name, FBT_ENTRY, 3, fbt);
	fbt->fbtp_patchpoint = instr;
	fbt->fbtp_ctl = lf;
	fbt->fbtp_loadcnt = lf->loadcnt;
	fbt->fbtp_rval = DTRACE_INVOP_PUSHL_EBP;
	fbt->fbtp_savedval = *instr;
	fbt->fbtp_patchval = FBT_PATCHVAL;
	fbt->fbtp_symindx = symindx;

	fbt->fbtp_hashnext = fbt_probetab[FBT_ADDR2NDX(instr)];
	fbt_probetab[FBT_ADDR2NDX(instr)] = fbt;

	lf->fbt_nentries++;

	retfbt = NULL;
again:
	if (instr >= limit)
		return (0);

	/*
	 * If this disassembly fails, then we've likely walked off into
	 * a jump table or some other unsuitable area.  Bail out of the
	 * disassembly now.
	 */
	if ((size = dtrace_instr_size(instr)) <= 0)
		return (0);

#ifdef __amd64__
	/*
	 * We only instrument "ret" on amd64 -- we don't yet instrument
	 * ret imm16, largely because the compiler doesn't seem to
	 * (yet) emit them in the kernel...
	 */
	if (*instr != FBT_RET) {
		instr += size;
		goto again;
	}
#else
	if (!(size == 1 &&
	    (*instr == FBT_POPL_EBP || *instr == FBT_LEAVE) &&
	    (*(instr + 1) == FBT_RET ||
	    *(instr + 1) == FBT_RET_IMM16))) {
		instr += size;
		goto again;
	}
#endif

	/*
	 * We (desperately) want to avoid erroneously instrumenting a
	 * jump table, especially given that our markers are pretty
	 * short:  two bytes on x86, and just one byte on amd64.  To
	 * determine if we're looking at a true instruction sequence
	 * or an inline jump table that happens to contain the same
	 * byte sequences, we resort to some heuristic sleeze:  we
	 * treat this instruction as being contained within a pointer,
	 * and see if that pointer points to within the body of the
	 * function.  If it does, we refuse to instrument it.
	 */
	for (j = 0; j < sizeof (uintptr_t); j++) {
		caddr_t check = (caddr_t) instr - j;
		uint8_t *ptr;

		if (check < symval->value)
			break;

		if (check + sizeof (caddr_t) > (caddr_t)limit)
			continue;

		ptr = *(uint8_t **)check;

		if (ptr >= (uint8_t *) symval->value && ptr < limit) {
			instr += size;
			goto again;
		}
	}

	/*
	 * We have a winner!
	 */
	fbt = malloc(sizeof (fbt_probe_t), M_FBT, M_WAITOK | M_ZERO);
	fbt->fbtp_name = name;

	if (retfbt == NULL) {
		fbt->fbtp_id = dtrace_probe_create(fbt_id, modname,
		    name, FBT_RETURN, 3, fbt);
	} else {
		retfbt->fbtp_next = fbt;
		fbt->fbtp_id = retfbt->fbtp_id;
	}

	retfbt = fbt;
	fbt->fbtp_patchpoint = instr;
	fbt->fbtp_ctl = lf;
	fbt->fbtp_loadcnt = lf->loadcnt;
	fbt->fbtp_symindx = symindx;

#ifndef __amd64__
	if (*instr == FBT_POPL_EBP) {
		fbt->fbtp_rval = DTRACE_INVOP_POPL_EBP;
	} else {
		ASSERT(*instr == FBT_LEAVE);
		fbt->fbtp_rval = DTRACE_INVOP_LEAVE;
	}
	fbt->fbtp_roffset =
	    (uintptr_t)(instr - (uint8_t *) symval->value) + 1;

#else
	ASSERT(*instr == FBT_RET);
	fbt->fbtp_rval = DTRACE_INVOP_RET;
	fbt->fbtp_roffset =
	    (uintptr_t)(instr - (uint8_t *) symval->value);
#endif

	fbt->fbtp_savedval = *instr;
	fbt->fbtp_patchval = FBT_PATCHVAL;
	fbt->fbtp_hashnext = fbt_probetab[FBT_ADDR2NDX(instr)];
	fbt_probetab[FBT_ADDR2NDX(instr)] = fbt;

	lf->fbt_nentries++;

	instr += size;
	goto again;
}
