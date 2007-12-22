/*-
 * Copyright (c) 2006 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <machine/frame.h>
#include <machine/md_var.h>

#include <ia64/disasm/disasm.h>

int
ia64_emulate(struct trapframe *tf, struct thread *td)
{
	struct asm_bundle bundle;
	struct asm_inst *i;
	int slot;

	if (!asm_decode(tf->tf_special.iip, &bundle))
		return (SIGILL);

	slot = ((tf->tf_special.psr & IA64_PSR_RI) == IA64_PSR_RI_0) ? 0 :
	    ((tf->tf_special.psr & IA64_PSR_RI) == IA64_PSR_RI_1) ? 1 : 2;
	if (slot == 1 && bundle.b_templ[slot] == 'L')
		slot++;

	i = bundle.b_inst + slot;
	switch (i->i_op) {
	case ASM_OP_BRL:
		/*
		 * We get the fault even if the predicate is false, so we
		 * need to check the predicate first and simply advance to
		 * the next bundle in that case.
		 */
		if (!(tf->tf_special.pr & (1UL << i->i_oper[0].o_value))) {
			tf->tf_special.psr &= ~IA64_PSR_RI;
			tf->tf_special.iip += 16;
			return (0);
		}
		/*
		 * The brl.cond is the simplest form. We only have to set
		 * the IP to the address in the instruction and return.
		 */
		if (i->i_cmpltr[0].c_type == ASM_CT_COND) {
			tf->tf_special.psr &= ~IA64_PSR_RI;
			tf->tf_special.iip += i->i_oper[1].o_value;
			return (0);
		}
		/* Sanity check... */
		if (i->i_cmpltr[0].c_type != ASM_CT_CALL)
			break;
		/*
		 * The brl.call is more difficult as we need to set-up the
		 * call properly.
		 */
		break;
	default:
		break;
	}

	return (SIGILL);
}
