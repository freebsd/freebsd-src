/*-
 * Copyright (c) 2004-2005, Juniper Networks, Inc.
 * All rights reserved.
 *
 *	JNPR: db_trace.c,v 1.8 2007/08/09 11:23:32 katta
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/proc.h>
#include <sys/stack.h>
#include <sys/sysent.h>

#include <machine/db_machdep.h>
#include <machine/md_var.h>
#include <machine/mips_opcode.h>
#include <machine/pcb.h>
#include <machine/trap.h>

#include <ddb/ddb.h>
#include <ddb/db_sym.h>

extern char _locore[];
extern char _locoreEnd[];
extern char edata[];

/*
 * A function using a stack frame has the following instruction as the first
 * one: addiu sp,sp,-<frame_size>
 *
 * We make use of this to detect starting address of a function. This works
 * better than using 'j ra' instruction to signify end of the previous
 * function (for e.g. functions like boot() or panic() do not actually
 * emit a 'j ra' instruction).
 *
 * XXX the abi does not require that the addiu instruction be the first one.
 */
#define	MIPS_START_OF_FUNCTION(ins)	(((ins) & 0xffff8000) == 0x27bd8000)

/*
 * MIPS ABI 3.0 requires that all functions return using the 'j ra' instruction
 *
 * XXX gcc doesn't do this for functions with __noreturn__ attribute.
 */
#define	MIPS_END_OF_FUNCTION(ins)	((ins) == 0x03e00008)

/*
 * kdbpeekD(addr) - skip one word starting at 'addr', then read the second word
 */
#define	kdbpeekD(addr)	kdbpeek(((int *)(addr)) + 1)

/*
 * Functions ``special'' enough to print by name
 */
#ifdef __STDC__
#define	Name(_fn)  { (void*)_fn, # _fn }
#else
#define	Name(_fn) { _fn, "_fn"}
#endif
static struct {
	void *addr;
	char *name;
}      names[] = {

	Name(trap),
	Name(MipsKernGenException),
	Name(MipsUserGenException),
	Name(MipsKernIntr),
	Name(MipsUserIntr),
	Name(cpu_switch),
	{
		0, 0
	}
};

/*
 * Map a function address to a string name, if known; or a hex string.
 */
static char *
fn_name(uintptr_t addr)
{
	static char buf[17];
	int i = 0;

	db_expr_t diff;
	c_db_sym_t sym;
	char *symname;

	diff = 0;
	symname = NULL;
	sym = db_search_symbol((db_addr_t)addr, DB_STGY_ANY, &diff);
	db_symbol_values(sym, (const char **)&symname, (db_expr_t *)0);
	if (symname && diff == 0)
		return (symname);

	for (i = 0; names[i].name; i++)
		if (names[i].addr == (void *)addr)
			return (names[i].name);
	sprintf(buf, "%jx", (uintmax_t)addr);
	return (buf);
}

void
stacktrace_subr(struct trapframe *regs, int (*printfn) (const char *,...))
{
	InstFmt i;
	uintptr_t a0, a1, a2, a3, pc, sp, fp, ra, va, subr;
	unsigned instr, mask;
	unsigned int frames = 0;
	int more, stksize;

	/* get initial values from the exception frame */
	sp = regs->sp;
	pc = regs->pc;
	fp = regs->s8;
	ra = regs->ra;		/* May be a 'leaf' function */
	a0 = regs->a0;
	a1 = regs->a1;
	a2 = regs->a2;
	a3 = regs->a3;

/* Jump here when done with a frame, to start a new one */
loop:

/* Jump here after a nonstandard (interrupt handler) frame */
	stksize = 0;
	subr = 0;
	if (frames++ > 100) {
		(*printfn) ("\nstackframe count exceeded\n");
		/* return breaks stackframe-size heuristics with gcc -O2 */
		goto finish;	/* XXX */
	}
	/* check for bad SP: could foul up next frame */
	/*XXX MIPS64 bad: this hard-coded SP is lame */
	if (sp & 3 || sp < 0x80000000) {
		(*printfn) ("SP 0x%x: not in kernel\n", sp);
		ra = 0;
		subr = 0;
		goto done;
	}
#define Between(x, y, z) \
		( ((x) <= (y)) && ((y) < (z)) )
#define pcBetween(a,b) \
		Between((uintptr_t)a, pc, (uintptr_t)b)

	/*
	 * Check for current PC in  exception handler code that don't have a
	 * preceding "j ra" at the tail of the preceding function. Depends
	 * on relative ordering of functions in exception.S, swtch.S.
	 */
	if (pcBetween(MipsKernGenException, MipsUserGenException))
		subr = (uintptr_t)MipsKernGenException;
	else if (pcBetween(MipsUserGenException, MipsKernIntr))
		subr = (uintptr_t)MipsUserGenException;
	else if (pcBetween(MipsKernIntr, MipsUserIntr))
		subr = (uintptr_t)MipsKernIntr;
	else if (pcBetween(MipsUserIntr, MipsTLBInvalidException))
		subr = (uintptr_t)MipsUserIntr;
	else if (pcBetween(MipsTLBInvalidException,
	    MipsKernTLBInvalidException))
		subr = (uintptr_t)MipsTLBInvalidException;
	else if (pcBetween(MipsKernTLBInvalidException,
	    MipsUserTLBInvalidException))
		subr = (uintptr_t)MipsKernTLBInvalidException;
	else if (pcBetween(MipsUserTLBInvalidException, MipsTLBMissException))
		subr = (uintptr_t)MipsUserTLBInvalidException;
	else if (pcBetween(cpu_switch, MipsSwitchFPState))
		subr = (uintptr_t)cpu_switch;
	else if (pcBetween(_locore, _locoreEnd)) {
		subr = (uintptr_t)_locore;
		ra = 0;
		goto done;
	}
	/* check for bad PC */
	/*XXX MIPS64 bad: These hard coded constants are lame */
	if (pc & 3 || pc < (uintptr_t)0x80000000 || pc >= (uintptr_t)edata) {
		(*printfn) ("PC 0x%x: not in kernel\n", pc);
		ra = 0;
		goto done;
	}
	/*
	 * Find the beginning of the current subroutine by scanning
	 * backwards from the current PC for the end of the previous
	 * subroutine.
	 */
	if (!subr) {
		va = pc - sizeof(int);
		while (1) {
			instr = kdbpeek((int *)va);

			if (MIPS_START_OF_FUNCTION(instr))
				break;

			if (MIPS_END_OF_FUNCTION(instr)) {
				/* skip over branch-delay slot instruction */
				va += 2 * sizeof(int);
				break;
			}

 			va -= sizeof(int);
		}

		/* skip over nulls which might separate .o files */
		while ((instr = kdbpeek((int *)va)) == 0)
			va += sizeof(int);
		subr = va;
	}
	/* scan forwards to find stack size and any saved registers */
	stksize = 0;
	more = 3;
	mask = 0;
	for (va = subr; more; va += sizeof(int),
	    more = (more == 3) ? 3 : more - 1) {
		/* stop if hit our current position */
		if (va >= pc)
			break;
		instr = kdbpeek((int *)va);
		i.word = instr;
		switch (i.JType.op) {
		case OP_SPECIAL:
			switch (i.RType.func) {
			case OP_JR:
			case OP_JALR:
				more = 2;	/* stop after next instruction */
				break;

			case OP_SYSCALL:
			case OP_BREAK:
				more = 1;	/* stop now */
			};
			break;

		case OP_BCOND:
		case OP_J:
		case OP_JAL:
		case OP_BEQ:
		case OP_BNE:
		case OP_BLEZ:
		case OP_BGTZ:
			more = 2;	/* stop after next instruction */
			break;

		case OP_COP0:
		case OP_COP1:
		case OP_COP2:
		case OP_COP3:
			switch (i.RType.rs) {
			case OP_BCx:
			case OP_BCy:
				more = 2;	/* stop after next instruction */
			};
			break;

		case OP_SW:
			/* look for saved registers on the stack */
			if (i.IType.rs != 29)
				break;
			/* only restore the first one */
			if (mask & (1 << i.IType.rt))
				break;
			mask |= (1 << i.IType.rt);
			switch (i.IType.rt) {
			case 4:/* a0 */
				a0 = kdbpeek((int *)(sp + (short)i.IType.imm));
				break;

			case 5:/* a1 */
				a1 = kdbpeek((int *)(sp + (short)i.IType.imm));
				break;

			case 6:/* a2 */
				a2 = kdbpeek((int *)(sp + (short)i.IType.imm));
				break;

			case 7:/* a3 */
				a3 = kdbpeek((int *)(sp + (short)i.IType.imm));
				break;

			case 30:	/* fp */
				fp = kdbpeek((int *)(sp + (short)i.IType.imm));
				break;

			case 31:	/* ra */
				ra = kdbpeek((int *)(sp + (short)i.IType.imm));
			}
			break;

		case OP_SD:
			/* look for saved registers on the stack */
			if (i.IType.rs != 29)
				break;
			/* only restore the first one */
			if (mask & (1 << i.IType.rt))
				break;
			mask |= (1 << i.IType.rt);
			switch (i.IType.rt) {
			case 4:/* a0 */
				a0 = kdbpeekD((int *)(sp + (short)i.IType.imm));
				break;

			case 5:/* a1 */
				a1 = kdbpeekD((int *)(sp + (short)i.IType.imm));
				break;

			case 6:/* a2 */
				a2 = kdbpeekD((int *)(sp + (short)i.IType.imm));
				break;

			case 7:/* a3 */
				a3 = kdbpeekD((int *)(sp + (short)i.IType.imm));
				break;

			case 30:	/* fp */
				fp = kdbpeekD((int *)(sp + (short)i.IType.imm));
				break;

			case 31:	/* ra */
				ra = kdbpeekD((int *)(sp + (short)i.IType.imm));
			}
			break;

		case OP_ADDI:
		case OP_ADDIU:
			/* look for stack pointer adjustment */
			if (i.IType.rs != 29 || i.IType.rt != 29)
				break;
			stksize = -((short)i.IType.imm);
		}
	}

done:
	(*printfn) ("%s+%x (%x,%x,%x,%x) ra %x sz %d\n",
	    fn_name(subr), pc - subr, a0, a1, a2, a3, ra, stksize);

	if (ra) {
		if (pc == ra && stksize == 0)
			(*printfn) ("stacktrace: loop!\n");
		else {
			pc = ra;
			sp += stksize;
			ra = 0;
			goto loop;
		}
	} else {
finish:
		if (curproc)
			(*printfn) ("pid %d\n", curproc->p_pid);
		else
			(*printfn) ("curproc NULL\n");
	}
}


int
db_md_set_watchpoint(db_expr_t addr, db_expr_t size)
{

	return(0);
}


int
db_md_clr_watchpoint( db_expr_t addr, db_expr_t size)
{

	return(0);
}


void
db_md_list_watchpoints()
{
}

static int
db_backtrace(struct thread *td, db_addr_t frame, int count)
{
	stacktrace_subr((struct trapframe *)frame,
	    (int (*) (const char *, ...))db_printf);
	return (0);
}

void
db_trace_self(void)
{
	db_trace_thread (curthread, -1);
	return;
}

int
db_trace_thread(struct thread *thr, int count)
{
	struct pcb *ctx;

	ctx = kdb_thr_ctx(thr);
	return (db_backtrace(thr, (db_addr_t) &ctx->pcb_regs, count));
}

void
db_show_mdpcpu(struct pcpu *pc)
{

	db_printf("ipis	    = 0x%x\n", pc->pc_pending_ipis);
	db_printf("next ASID    = %d\n", pc->pc_next_asid);
	db_printf("GENID	    = %d\n", pc->pc_asid_generation);
	return;
}
