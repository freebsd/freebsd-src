/*-
 * Copyright (c) 2015 The FreeBSD Foundation
 * Copyright (c) 2016 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 * Copyright (c) 2020 John Baldwin <jhb@FreeBSD.org>
 *
 * Portions of this software were developed by Semihalf under
 * the sponsorship of the FreeBSD Foundation.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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
#include <sys/param.h>
#include <sys/kdb.h>
#include <sys/proc.h>

#include <ddb/ddb.h>
#include <ddb/db_sym.h>

#include <machine/pcb.h>
#include <machine/riscvreg.h>
#include <machine/stack.h>
#include <machine/vmparam.h>

void
db_md_list_watchpoints(void)
{

}

static void
db_stack_trace_cmd(struct thread *td, struct unwind_state *frame)
{
	const char *name;
	db_expr_t offset;
	db_expr_t value;
	c_db_sym_t sym;
	uint64_t pc;

	while (1) {
		pc = frame->pc;

		sym = db_search_symbol(pc, DB_STGY_ANY, &offset);
		if (sym == C_DB_SYM_NULL) {
			value = 0;
			name = "(null)";
		} else
			db_symbol_values(sym, &name, &value);

		db_printf("%s() at ", name);
		db_printsym(frame->pc, DB_STGY_PROC);
		db_printf("\n");

		if (strcmp(name, "cpu_exception_handler_supervisor") == 0 ||
		    strcmp(name, "cpu_exception_handler_user") == 0) {
			struct trapframe *tf;

			tf = (struct trapframe *)(uintptr_t)frame->sp;
			if (!__is_aligned(tf, _Alignof(*tf)) ||
			    !kstack_contains(td, (vm_offset_t)tf,
			    sizeof(*tf))) {
				db_printf("--- invalid trapframe %p\n", tf);
				break;
			}

			if ((tf->tf_scause & SCAUSE_INTR) != 0) {
				db_printf("--- interrupt %ld\n",
				    tf->tf_scause & SCAUSE_CODE);
			} else if (tf->tf_scause == SCAUSE_ECALL_USER) {
				db_printf("--- syscall");
				db_decode_syscall(td, td->td_sa.code);
				db_printf("\n");
			} else {
				db_printf("--- exception %ld, tval = %#lx\n",
				    tf->tf_scause & SCAUSE_CODE,
				    tf->tf_stval);
			}
			frame->sp = tf->tf_sp;
			frame->fp = tf->tf_s[0];
			frame->pc = tf->tf_sepc;
			if (!INKERNEL(frame->fp))
				break;
			continue;
		}

		if (strcmp(name, "fork_trampoline") == 0)
			break;

		if (!unwind_frame(td, frame))
			break;
	}
}

int
db_trace_thread(struct thread *thr, int count)
{
	struct unwind_state frame;
	struct pcb *ctx;

	ctx = kdb_thr_ctx(thr);

	frame.sp = ctx->pcb_sp;
	frame.fp = ctx->pcb_s[0];
	frame.pc = ctx->pcb_ra;
	db_stack_trace_cmd(thr, &frame);
	return (0);
}

void
db_trace_self(void)
{
	struct unwind_state frame;
	uintptr_t sp;

	__asm __volatile("mv %0, sp" : "=&r" (sp));

	frame.sp = sp;
	frame.fp = (uintptr_t)__builtin_frame_address(0);
	frame.pc = (uintptr_t)db_trace_self;
	db_stack_trace_cmd(curthread, &frame);
}
