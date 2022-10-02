/*-
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/proc.h>
#include <sys/reg.h>
#include <sys/smp.h>
#include <sys/stack.h>

#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/stack.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <ddb/ddb.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>

static db_varfcn_t db_frame;
static db_varfcn_t db_frame_seg;

CTASSERT(sizeof(struct dbreg) == sizeof(((struct pcpu *)NULL)->pc_dbreg));

/*
 * Machine register set.
 */
#define	DB_OFFSET(x)	(db_expr_t *)offsetof(struct trapframe, x)
struct db_variable db_regs[] = {
	{ "cs",		DB_OFFSET(tf_cs),	db_frame_seg },
	{ "ds",		DB_OFFSET(tf_ds),	db_frame_seg },
	{ "es",		DB_OFFSET(tf_es),	db_frame_seg },
	{ "fs",		DB_OFFSET(tf_fs),	db_frame_seg },
	{ "gs",		DB_OFFSET(tf_gs),	db_frame_seg },
	{ "ss",		DB_OFFSET(tf_ss),	db_frame_seg },
	{ "rax",	DB_OFFSET(tf_rax),	db_frame },
	{ "rcx",        DB_OFFSET(tf_rcx),	db_frame },
	{ "rdx",	DB_OFFSET(tf_rdx),	db_frame },
	{ "rbx",	DB_OFFSET(tf_rbx),	db_frame },
	{ "rsp",	DB_OFFSET(tf_rsp),	db_frame },
	{ "rbp",	DB_OFFSET(tf_rbp),	db_frame },
	{ "rsi",	DB_OFFSET(tf_rsi),	db_frame },
	{ "rdi",	DB_OFFSET(tf_rdi),	db_frame },
	{ "r8",		DB_OFFSET(tf_r8),	db_frame },
	{ "r9",		DB_OFFSET(tf_r9),	db_frame },
	{ "r10",	DB_OFFSET(tf_r10),	db_frame },
	{ "r11",	DB_OFFSET(tf_r11),	db_frame },
	{ "r12",	DB_OFFSET(tf_r12),	db_frame },
	{ "r13",	DB_OFFSET(tf_r13),	db_frame },
	{ "r14",	DB_OFFSET(tf_r14),	db_frame },
	{ "r15",	DB_OFFSET(tf_r15),	db_frame },
	{ "rip",	DB_OFFSET(tf_rip),	db_frame },
	{ "rflags",	DB_OFFSET(tf_rflags),	db_frame },
};
struct db_variable *db_eregs = db_regs + nitems(db_regs);

static int
db_frame_seg(struct db_variable *vp, db_expr_t *valuep, int op)
{
	uint16_t *reg;

	if (kdb_frame == NULL)
		return (0);

	reg = (uint16_t *)((uintptr_t)kdb_frame + (db_expr_t)vp->valuep);
	if (op == DB_VAR_GET)
		*valuep = *reg;
	else
		*reg = *valuep;
	return (1);
}

static int
db_frame(struct db_variable *vp, db_expr_t *valuep, int op)
{
	long *reg;

	if (kdb_frame == NULL)
		return (0);

	reg = (long *)((uintptr_t)kdb_frame + (db_expr_t)vp->valuep);
	if (op == DB_VAR_GET)
		*valuep = *reg;
	else
		*reg = *valuep;
	return (1);
}

#define NORMAL		0
#define	TRAP		1
#define	INTERRUPT	2
#define	SYSCALL		3

static void db_nextframe(struct amd64_frame **, db_addr_t *, struct thread *);
static void db_print_stack_entry(const char *, db_addr_t, void *);
static void decode_syscall(int, struct thread *);

static void
db_print_stack_entry(const char *name, db_addr_t callpc, void *frame)
{

	db_printf("%s() at ", name != NULL ? name : "??");
	db_printsym(callpc, DB_STGY_PROC);
	if (frame != NULL)
		db_printf("/frame 0x%lx", (register_t)frame);
	db_printf("\n");
}

/*
 * Figure out the next frame up in the call stack.
 */
static void
db_nextframe(struct amd64_frame **fp, db_addr_t *ip, struct thread *td)
{
	struct trapframe *tf;
	int frame_type;
	long rip, rsp, rbp;
	db_expr_t offset;
	c_db_sym_t sym;
	const char *name;

	rip = db_get_value((long) &(*fp)->f_retaddr, 8, FALSE);
	rbp = db_get_value((long) &(*fp)->f_frame, 8, FALSE);

	/*
	 * Figure out frame type.  We look at the address just before
	 * the saved instruction pointer as the saved EIP is after the
	 * call function, and if the function being called is marked as
	 * dead (such as panic() at the end of dblfault_handler()), then
	 * the instruction at the saved EIP will be part of a different
	 * function (syscall() in this example) rather than the one that
	 * actually made the call.
	 */
	frame_type = NORMAL;
	sym = db_search_symbol(rip - 1, DB_STGY_ANY, &offset);
	db_symbol_values(sym, &name, NULL);
	if (name != NULL) {
		if (strcmp(name, "calltrap") == 0 ||
		    strcmp(name, "fork_trampoline") == 0 ||
		    strcmp(name, "mchk_calltrap") == 0 ||
		    strcmp(name, "nmi_calltrap") == 0 ||
		    strcmp(name, "Xdblfault") == 0)
			frame_type = TRAP;
		else if (strncmp(name, "Xatpic_intr", 11) == 0 ||
		    strncmp(name, "Xapic_isr", 9) == 0 ||
		    strcmp(name, "Xxen_intr_upcall") == 0 ||
		    strcmp(name, "Xtimerint") == 0 ||
		    strcmp(name, "Xipi_intr_bitmap_handler") == 0 ||
		    strcmp(name, "Xcpustop") == 0 ||
		    strcmp(name, "Xcpususpend") == 0 ||
		    strcmp(name, "Xrendezvous") == 0)
			frame_type = INTERRUPT;
		else if (strcmp(name, "Xfast_syscall") == 0 ||
		    strcmp(name, "Xfast_syscall_pti") == 0 ||
		    strcmp(name, "fast_syscall_common") == 0)
			frame_type = SYSCALL;
#ifdef COMPAT_FREEBSD32
		else if (strcmp(name, "Xint0x80_syscall") == 0)
			frame_type = SYSCALL;
#endif
	}

	/*
	 * Normal frames need no special processing.
	 */
	if (frame_type == NORMAL) {
		*ip = (db_addr_t) rip;
		*fp = (struct amd64_frame *) rbp;
		return;
	}

	db_print_stack_entry(name, rip, &(*fp)->f_frame);

	/*
	 * Point to base of trapframe which is just above the
	 * current frame.
	 */
	tf = (struct trapframe *)((long)*fp + 16);

	if (INKERNEL((long) tf)) {
		rsp = tf->tf_rsp;
		rip = tf->tf_rip;
		rbp = tf->tf_rbp;
		switch (frame_type) {
		case TRAP:
			db_printf("--- trap %#r", tf->tf_trapno);
			break;
		case SYSCALL:
			db_printf("--- syscall");
			db_decode_syscall(tf->tf_rax, td);
			break;
		case INTERRUPT:
			db_printf("--- interrupt");
			break;
		default:
			panic("The moon has moved again.");
		}
		db_printf(", rip = %#lr, rsp = %#lr, rbp = %#lr ---\n", rip,
		    rsp, rbp);
	}

	*ip = (db_addr_t) rip;
	*fp = (struct amd64_frame *) rbp;
}

static int __nosanitizeaddress __nosanitizememory
db_backtrace(struct thread *td, struct trapframe *tf, struct amd64_frame *frame,
    db_addr_t pc, register_t sp, int count)
{
	struct amd64_frame *actframe;
	const char *name;
	db_expr_t offset;
	c_db_sym_t sym;
	boolean_t first;

	if (count == -1)
		count = 1024;

	first = TRUE;
	while (count-- && !db_pager_quit) {
		sym = db_search_symbol(pc, DB_STGY_ANY, &offset);
		db_symbol_values(sym, &name, NULL);

		/*
		 * Attempt to determine a (possibly fake) frame that gives
		 * the caller's pc.  It may differ from `frame' if the
		 * current function never sets up a standard frame or hasn't
		 * set one up yet or has just discarded one.  The last two
		 * cases can be guessed fairly reliably for code generated
		 * by gcc.  The first case is too much trouble to handle in
		 * general because the amount of junk on the stack depends
		 * on the pc (the special handling of "calltrap", etc. in
		 * db_nextframe() works because the `next' pc is special).
		 */
		actframe = frame;
		if (first) {
			first = FALSE;
			if (sym == C_DB_SYM_NULL && sp != 0) {
				/*
				 * If a symbol couldn't be found, we've probably
				 * jumped to a bogus location, so try and use
				 * the return address to find our caller.
				 */
				db_print_stack_entry(name, pc, NULL);
				pc = db_get_value(sp, 8, FALSE);
				if (db_search_symbol(pc, DB_STGY_PROC,
				    &offset) == C_DB_SYM_NULL)
					break;
				continue;
			} else if (tf != NULL) {
				int instr;

				instr = db_get_value(pc, 4, FALSE);
				if ((instr & 0xffffffff) == 0xe5894855) {
					/* pushq %rbp; movq %rsp, %rbp */
					actframe = (void *)(tf->tf_rsp - 8);
				} else if ((instr & 0xffffff) == 0xe58948) {
					/* movq %rsp, %rbp */
					actframe = (void *)tf->tf_rsp;
					if (tf->tf_rbp == 0) {
						/* Fake frame better. */
						frame = actframe;
					}
				} else if ((instr & 0xff) == 0xc3) {
					/* ret */
					actframe = (void *)(tf->tf_rsp - 8);
				} else if (offset == 0) {
					/* Probably an assembler symbol. */
					actframe = (void *)(tf->tf_rsp - 8);
				}
			} else if (name != NULL &&
			    strcmp(name, "fork_trampoline") == 0) {
				/*
				 * Don't try to walk back on a stack for a
				 * process that hasn't actually been run yet.
				 */
				db_print_stack_entry(name, pc, actframe);
				break;
			}
		}

		db_print_stack_entry(name, pc, actframe);

		if (actframe != frame) {
			/* `frame' belongs to caller. */
			pc = (db_addr_t)
			    db_get_value((long)&actframe->f_retaddr, 8, FALSE);
			continue;
		}

		db_nextframe(&frame, &pc, td);

		if (INKERNEL((long)pc) && !INKERNEL((long)frame)) {
			sym = db_search_symbol(pc, DB_STGY_ANY, &offset);
			db_symbol_values(sym, &name, NULL);
			db_print_stack_entry(name, pc, frame);
			break;
		}
		if (!INKERNEL((long) frame)) {
			break;
		}
	}

	return (0);
}

void
db_trace_self(void)
{
	struct amd64_frame *frame;
	db_addr_t callpc;
	register_t rbp;

	__asm __volatile("movq %%rbp,%0" : "=r" (rbp));
	frame = (struct amd64_frame *)rbp;
	callpc = (db_addr_t)db_get_value((long)&frame->f_retaddr, 8, FALSE);
	frame = frame->f_frame;
	db_backtrace(curthread, NULL, frame, callpc, 0, -1);
}

int
db_trace_thread(struct thread *thr, int count)
{
	struct pcb *ctx;
	struct trapframe *tf;

	ctx = kdb_thr_ctx(thr);
	tf = thr == kdb_thread ? kdb_frame : NULL;
	return (db_backtrace(thr, tf, (struct amd64_frame *)ctx->pcb_rbp,
	    ctx->pcb_rip, ctx->pcb_rsp, count));
}

void
db_md_list_watchpoints(void)
{

	dbreg_list_watchpoints();
}
