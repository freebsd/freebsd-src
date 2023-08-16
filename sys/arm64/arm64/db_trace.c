/*-
 * Copyright (c) 2015 The FreeBSD Foundation
 *
 * This software was developed by Semihalf under
 * the sponsorship of the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_ddb.h"

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/kdb.h>

#include <machine/pcb.h>
#include <ddb/ddb.h>
#include <ddb/db_sym.h>

#include <machine/armreg.h>
#include <machine/debug_monitor.h>
#include <machine/stack.h>
#include <machine/vmparam.h>

#define	FRAME_NORMAL	0
#define	FRAME_SYNC	1
#define	FRAME_IRQ	2
#define	FRAME_SERROR	3
#define	FRAME_UNHANDLED	4

void
db_md_list_watchpoints(void)
{

	dbg_show_watchpoint();
}

static void __nosanitizeaddress
db_stack_trace_cmd(struct thread *td, struct unwind_state *frame)
{
	c_db_sym_t sym;
	const char *name;
	db_expr_t value;
	db_expr_t offset;
	int frame_type;

	while (1) {
		sym = db_search_symbol(frame->pc, DB_STGY_ANY, &offset);
		if (sym == C_DB_SYM_NULL) {
			value = 0;
			name = "(null)";
		} else
			db_symbol_values(sym, &name, &value);

		db_printf("%s() at ", name);
		db_printsym(frame->pc, DB_STGY_PROC);
		db_printf("\n");

		if (strcmp(name, "handle_el0_sync") == 0 ||
		    strcmp(name, "handle_el1h_sync") == 0)
			frame_type = FRAME_SYNC;
		else if (strcmp(name, "handle_el0_irq") == 0 ||
		     strcmp(name, "handle_el1h_irq") == 0)
			frame_type = FRAME_IRQ;
		else if (strcmp(name, "handle_serror") == 0)
			frame_type = FRAME_SERROR;
		else if (strcmp(name, "handle_empty_exception") == 0)
			frame_type = FRAME_UNHANDLED;
		else
			frame_type = FRAME_NORMAL;

		if (frame_type != FRAME_NORMAL) {
			struct trapframe *tf;

			tf = (struct trapframe *)(uintptr_t)frame->fp - 1;
			if (!kstack_contains(td, (vm_offset_t)tf,
			    sizeof(*tf))) {
				db_printf("--- invalid trapframe %p\n", tf);
				break;
			}

			switch (frame_type) {
			case FRAME_SYNC:
				db_printf("--- exception, esr %#lx\n",
				    tf->tf_esr);
				break;
			case FRAME_IRQ:
				db_printf("--- interrupt\n");
				break;
			case FRAME_SERROR:
				db_printf("--- system error, esr %#lx\n",
				    tf->tf_esr);
				break;
			case FRAME_UNHANDLED:
				db_printf("--- unhandled exception, esr %#lx\n",
				    tf->tf_esr);
				break;
			default:
				__assert_unreachable();
				break;
			}

			frame->fp = tf->tf_x[29];
			frame->pc = ADDR_MAKE_CANONICAL(tf->tf_elr);
			if (!INKERNEL(frame->fp))
				break;
		} else {
			if (strcmp(name, "fork_trampoline") == 0)
				break;

			if (!unwind_frame(td, frame))
				break;
		}
	}
}

int __nosanitizeaddress
db_trace_thread(struct thread *thr, int count)
{
	struct unwind_state frame;
	struct pcb *ctx;

	if (thr != curthread) {
		ctx = kdb_thr_ctx(thr);

		frame.fp = (uintptr_t)ctx->pcb_x[PCB_FP];
		frame.pc = (uintptr_t)ctx->pcb_x[PCB_LR];
		db_stack_trace_cmd(thr, &frame);
	} else
		db_trace_self();
	return (0);
}

void __nosanitizeaddress
db_trace_self(void)
{
	struct unwind_state frame;

	frame.fp = (uintptr_t)__builtin_frame_address(0);
	frame.pc = (uintptr_t)db_trace_self;
	db_stack_trace_cmd(curthread, &frame);
}
