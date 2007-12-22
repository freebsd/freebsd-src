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
#include <sys/stack.h>
#include <sys/sysent.h>

#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/reg.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <ddb/ddb.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>

static db_varfcn_t db_dr0;
static db_varfcn_t db_dr1;
static db_varfcn_t db_dr2;
static db_varfcn_t db_dr3;
static db_varfcn_t db_dr4;
static db_varfcn_t db_dr5;
static db_varfcn_t db_dr6;
static db_varfcn_t db_dr7;
static db_varfcn_t db_frame;
static db_varfcn_t db_rsp;
static db_varfcn_t db_ss;

/*
 * Machine register set.
 */
#define	DB_OFFSET(x)	(db_expr_t *)offsetof(struct trapframe, x)
struct db_variable db_regs[] = {
	{ "cs",		DB_OFFSET(tf_cs),	db_frame },
#if 0
	{ "ds",		DB_OFFSET(tf_ds),	db_frame },
	{ "es",		DB_OFFSET(tf_es),	db_frame },
	{ "fs",		DB_OFFSET(tf_fs),	db_frame },
	{ "gs",		DB_OFFSET(tf_gs),	db_frame },
#endif
	{ "ss",		NULL,			db_ss },
	{ "rax",	DB_OFFSET(tf_rax),	db_frame },
	{ "rcx",        DB_OFFSET(tf_rcx),	db_frame },
	{ "rdx",	DB_OFFSET(tf_rdx),	db_frame },
	{ "rbx",	DB_OFFSET(tf_rbx),	db_frame },
	{ "rsp",	NULL,			db_rsp },
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
#define	DB_N_SHOW_REGS	20	/* Don't show registers after here. */
	{ "dr0",	NULL,			db_dr0 },
	{ "dr1",	NULL,			db_dr1 },
	{ "dr2",	NULL,			db_dr2 },
	{ "dr3",	NULL,			db_dr3 },
	{ "dr4",	NULL,			db_dr4 },
	{ "dr5",	NULL,			db_dr5 },
	{ "dr6",	NULL,			db_dr6 },
	{ "dr7",	NULL,			db_dr7 },
};
struct db_variable *db_eregs = db_regs + DB_N_SHOW_REGS;

#define DB_DRX_FUNC(reg)		\
static int				\
db_ ## reg (vp, valuep, op)		\
	struct db_variable *vp;		\
	db_expr_t * valuep;		\
	int op;				\
{					\
	if (op == DB_VAR_GET)		\
		*valuep = r ## reg ();	\
	else				\
		load_ ## reg (*valuep); \
	return (1);			\
}

DB_DRX_FUNC(dr0)
DB_DRX_FUNC(dr1)
DB_DRX_FUNC(dr2)
DB_DRX_FUNC(dr3)
DB_DRX_FUNC(dr4)
DB_DRX_FUNC(dr5)
DB_DRX_FUNC(dr6)
DB_DRX_FUNC(dr7)

static __inline long
get_rsp(struct trapframe *tf)
{
	return ((ISPL(tf->tf_cs)) ? tf->tf_rsp :
	    (db_expr_t)tf + offsetof(struct trapframe, tf_rsp));
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

static int
db_rsp(struct db_variable *vp, db_expr_t *valuep, int op)
{

	if (kdb_frame == NULL)
		return (0);

	if (op == DB_VAR_GET)
		*valuep = get_rsp(kdb_frame);
	else if (ISPL(kdb_frame->tf_cs))
		kdb_frame->tf_rsp = *valuep;
	return (1);
}

static int
db_ss(struct db_variable *vp, db_expr_t *valuep, int op)
{

	if (kdb_frame == NULL)
		return (0);

	if (op == DB_VAR_GET)
		*valuep = (ISPL(kdb_frame->tf_cs)) ? kdb_frame->tf_ss : rss();
	else if (ISPL(kdb_frame->tf_cs))
		kdb_frame->tf_ss = *valuep;
	return (1);
}

/*
 * Stack trace.
 */
#define	INKERNEL(va) (((va) >= DMAP_MIN_ADDRESS && (va) < DMAP_MAX_ADDRESS) \
	    || ((va) >= KERNBASE && (va) < VM_MAX_KERNEL_ADDRESS))

struct amd64_frame {
	struct amd64_frame	*f_frame;
	long			f_retaddr;
	long			f_arg0;
};

#define NORMAL		0
#define	TRAP		1
#define	INTERRUPT	2
#define	SYSCALL		3
#define	TRAP_INTERRUPT	5

static void db_nextframe(struct amd64_frame **, db_addr_t *, struct thread *);
static int db_numargs(struct amd64_frame *);
static void db_print_stack_entry(const char *, int, char **, long *, db_addr_t);
static void decode_syscall(int, struct thread *);

static const char * watchtype_str(int type);
int  amd64_set_watch(int watchnum, unsigned long watchaddr, int size,
		    int access, struct dbreg *d);
int  amd64_clr_watch(int watchnum, struct dbreg *d);

/*
 * Figure out how many arguments were passed into the frame at "fp".
 */
static int
db_numargs(fp)
	struct amd64_frame *fp;
{
#if 1
	return (0);	/* regparm, needs dwarf2 info */
#else
	long	*argp;
	int	inst;
	int	args;

	argp = (long *)db_get_value((long)&fp->f_retaddr, 8, FALSE);
	/*
	 * XXX etext is wrong for LKMs.  We should attempt to interpret
	 * the instruction at the return address in all cases.  This
	 * may require better fault handling.
	 */
	if (argp < (long *)btext || argp >= (long *)etext) {
		args = 5;
	} else {
		inst = db_get_value((long)argp, 4, FALSE);
		if ((inst & 0xff) == 0x59)	/* popl %ecx */
			args = 1;
		else if ((inst & 0xffff) == 0xc483)	/* addl $Ibs, %esp */
			args = ((inst >> 16) & 0xff) / 4;
		else
			args = 5;
	}
	return (args);
#endif
}

static void
db_print_stack_entry(name, narg, argnp, argp, callpc)
	const char *name;
	int narg;
	char **argnp;
	long *argp;
	db_addr_t callpc;
{
	db_printf("%s(", name);
#if 0
	while (narg) {
		if (argnp)
			db_printf("%s=", *argnp++);
		db_printf("%lr", (long)db_get_value((long)argp, 8, FALSE));
		argp++;
		if (--narg != 0)
			db_printf(",");
	}
#endif
	db_printf(") at ");
	db_printsym(callpc, DB_STGY_PROC);
	db_printf("\n");
}

static void
decode_syscall(int number, struct thread *td)
{
	struct proc *p;
	c_db_sym_t sym;
	db_expr_t diff;
	sy_call_t *f;
	const char *symname;

	db_printf(" (%d", number);
	p = (td != NULL) ? td->td_proc : NULL;
	if (p != NULL && 0 <= number && number < p->p_sysent->sv_size) {
		f = p->p_sysent->sv_table[number].sy_call;
		sym = db_search_symbol((db_addr_t)f, DB_STGY_ANY, &diff);
		if (sym != DB_SYM_NULL && diff == 0) {
			db_symbol_values(sym, &symname, NULL);
			db_printf(", %s, %s", p->p_sysent->sv_name, symname);
		}
	}
	db_printf(")");
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
		    strcmp(name, "nmi_calltrap") == 0 ||
		    strcmp(name, "Xdblfault") == 0)
			frame_type = TRAP;
		else if (strncmp(name, "Xatpic_intr", 11) == 0 ||
		    strncmp(name, "Xapic_isr", 9) == 0 ||
		    strcmp(name, "Xtimerint") == 0 ||
		    strcmp(name, "Xipi_intr_bitmap_handler") == 0 ||
		    strcmp(name, "Xcpustop") == 0 ||
		    strcmp(name, "Xrendezvous") == 0)
			frame_type = INTERRUPT;
		else if (strcmp(name, "Xfast_syscall") == 0)
			frame_type = SYSCALL;
		/* XXX: These are interrupts with trap frames. */
		else if (strcmp(name, "Xtimerint") == 0 ||
		    strcmp(name, "Xcpustop") == 0 ||
		    strcmp(name, "Xrendezvous") == 0 ||
		    strcmp(name, "Xipi_intr_bitmap_handler") == 0)
			frame_type = TRAP_INTERRUPT;
	}

	/*
	 * Normal frames need no special processing.
	 */
	if (frame_type == NORMAL) {
		*ip = (db_addr_t) rip;
		*fp = (struct amd64_frame *) rbp;
		return;
	}

	db_print_stack_entry(name, 0, 0, 0, rip);

	/*
	 * Point to base of trapframe which is just above the
	 * current frame.
	 */
	tf = (struct trapframe *)((long)*fp + 16);

	if (INKERNEL((long) tf)) {
		rsp = get_rsp(tf);
		rip = tf->tf_rip;
		rbp = tf->tf_rbp;
		switch (frame_type) {
		case TRAP:
			db_printf("--- trap %#lr", tf->tf_trapno);
			break;
		case SYSCALL:
			db_printf("--- syscall");
			decode_syscall(tf->tf_rax, td);
			break;
		case TRAP_INTERRUPT:
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

static int
db_backtrace(struct thread *td, struct trapframe *tf,
    struct amd64_frame *frame, db_addr_t pc, int count)
{
	struct amd64_frame *actframe;
#define MAXNARG	16
	char *argnames[MAXNARG], **argnp = NULL;
	const char *name;
	long *argp;
	db_expr_t offset;
	c_db_sym_t sym;
	int narg;
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
			if (tf != NULL) {
				int instr;

				instr = db_get_value(pc, 4, FALSE);
				if ((instr & 0xffffffff) == 0xe5894855) {
					/* pushq %rbp; movq %rsp, %rbp */
					actframe = (void *)(get_rsp(tf) - 8);
				} else if ((instr & 0xffffff) == 0xe58948) {
					/* movq %rsp, %rbp */
					actframe = (void *)get_rsp(tf);
					if (tf->tf_rbp == 0) {
						/* Fake frame better. */
						frame = actframe;
					}
				} else if ((instr & 0xff) == 0xc3) {
					/* ret */
					actframe = (void *)(get_rsp(tf) - 8);
				} else if (offset == 0) {
					/* Probably an assembler symbol. */
					actframe = (void *)(get_rsp(tf) - 8);
				}
			} else if (strcmp(name, "fork_trampoline") == 0) {
				/*
				 * Don't try to walk back on a stack for a
				 * process that hasn't actually been run yet.
				 */
				db_print_stack_entry(name, 0, 0, 0, pc);
				break;
			}
			first = FALSE;
		}

		argp = &actframe->f_arg0;
		narg = MAXNARG;
		if (sym != NULL && db_sym_numargs(sym, &narg, argnames)) {
			argnp = argnames;
		} else {
			narg = db_numargs(frame);
		}

		db_print_stack_entry(name, narg, argnp, argp, pc);

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
			db_print_stack_entry(name, 0, 0, 0, pc);
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
	db_backtrace(curthread, NULL, frame, callpc, -1);
}

int
db_trace_thread(struct thread *thr, int count)
{
	struct pcb *ctx;

	ctx = kdb_thr_ctx(thr);
	return (db_backtrace(thr, NULL, (struct amd64_frame *)ctx->pcb_rbp,
		    ctx->pcb_rip, count));
}

void
stack_save(struct stack *st)
{
	struct amd64_frame *frame;
	vm_offset_t callpc;
	register_t rbp;

	stack_zero(st);
	__asm __volatile("movq %%rbp,%0" : "=r" (rbp));
	frame = (struct amd64_frame *)rbp;
	while (1) {
		if (!INKERNEL((long)frame))
			break;
		callpc = frame->f_retaddr;
		if (!INKERNEL(callpc))
			break;
		if (stack_put(st, callpc) == -1)
			break;
		if (frame->f_frame <= frame ||
		    (vm_offset_t)frame->f_frame >=
		    (vm_offset_t)rbp + KSTACK_PAGES * PAGE_SIZE)
			break;
		frame = frame->f_frame;
	}
}

int
amd64_set_watch(watchnum, watchaddr, size, access, d)
	int watchnum;
	unsigned long watchaddr;
	int size;
	int access;
	struct dbreg *d;
{
	int i, len;

	if (watchnum == -1) {
		for (i = 0; i < 4; i++)
			if (!DBREG_DR7_ENABLED(d->dr[7], i))
				break;
		if (i < 4)
			watchnum = i;
		else
			return (-1);
	}

	switch (access) {
	case DBREG_DR7_EXEC:
		size = 1; /* size must be 1 for an execution breakpoint */
		/* fall through */
	case DBREG_DR7_WRONLY:
	case DBREG_DR7_RDWR:
		break;
	default:
		return (-1);
	}

	/*
	 * we can watch a 1, 2, 4, or 8 byte sized location
	 */
	switch (size) {
	case 1:
		len = DBREG_DR7_LEN_1;
		break;
	case 2:
		len = DBREG_DR7_LEN_2;
		break;
	case 4:
		len = DBREG_DR7_LEN_4;
		break;
	case 8:
		len = DBREG_DR7_LEN_8;
		break;
	default:
		return (-1);
	}

	/* clear the bits we are about to affect */
	d->dr[7] &= ~DBREG_DR7_MASK(watchnum);

	/* set drN register to the address, N=watchnum */
	DBREG_DRX(d, watchnum) = watchaddr;

	/* enable the watchpoint */
	d->dr[7] |= DBREG_DR7_SET(watchnum, len, access,
	    DBREG_DR7_GLOBAL_ENABLE);

	return (watchnum);
}


int
amd64_clr_watch(watchnum, d)
	int watchnum;
	struct dbreg *d;
{

	if (watchnum < 0 || watchnum >= 4)
		return (-1);

	d->dr[7] &= ~DBREG_DR7_MASK(watchnum);
	DBREG_DRX(d, watchnum) = 0;

	return (0);
}


int
db_md_set_watchpoint(addr, size)
	db_expr_t addr;
	db_expr_t size;
{
	struct dbreg d;
	int avail, i, wsize;

	fill_dbregs(NULL, &d);

	avail = 0;
	for(i = 0; i < 4; i++) {
		if (!DBREG_DR7_ENABLED(d.dr[7], i))
			avail++;
	}

	if (avail * 8 < size)
		return (-1);

	for (i = 0; i < 4 && (size > 0); i++) {
		if (!DBREG_DR7_ENABLED(d.dr[7], i)) {
			if (size >= 8 || (avail == 1 && size > 4))
				wsize = 8;
			else if (size > 2)
				wsize = 4;
			else
				wsize = size;
			amd64_set_watch(i, addr, wsize,
				       DBREG_DR7_WRONLY, &d);
			addr += wsize;
			size -= wsize;
			avail--;
		}
	}

	set_dbregs(NULL, &d);

	return(0);
}


int
db_md_clr_watchpoint(addr, size)
	db_expr_t addr;
	db_expr_t size;
{
	struct dbreg d;
	int i;

	fill_dbregs(NULL, &d);

	for(i = 0; i < 4; i++) {
		if (DBREG_DR7_ENABLED(d.dr[7], i)) {
			if ((DBREG_DRX((&d), i) >= addr) &&
			    (DBREG_DRX((&d), i) < addr+size))
				amd64_clr_watch(i, &d);

		}
	}

	set_dbregs(NULL, &d);

	return(0);
}


static const char *
watchtype_str(type)
	int type;
{
	switch (type) {
		case DBREG_DR7_EXEC   : return "execute";    break;
		case DBREG_DR7_RDWR   : return "read/write"; break;
		case DBREG_DR7_WRONLY : return "write";	     break;
		default		      : return "invalid";    break;
	}
}


void
db_md_list_watchpoints()
{
	struct dbreg d;
	int i, len, type;

	fill_dbregs(NULL, &d);

	db_printf("\nhardware watchpoints:\n");
	db_printf("  watch    status        type  len             address\n");
	db_printf("  -----  --------  ----------  ---  ------------------\n");
	for (i = 0; i < 4; i++) {
		if (DBREG_DR7_ENABLED(d.dr[7], i)) {
			type = DBREG_DR7_ACCESS(d.dr[7], i);
			len = DBREG_DR7_LEN(d.dr[7], i);
			if (len == DBREG_DR7_LEN_8)
				len = 8;
			else
				len++;
			db_printf("  %-5d  %-8s  %10s  %3d  ",
			    i, "enabled", watchtype_str(type), len);
			db_printsym((db_addr_t)DBREG_DRX((&d), i), DB_STGY_ANY);
			db_printf("\n");
		} else {
			db_printf("  %-5d  disabled\n", i);
		}
	}

	db_printf("\ndebug register values:\n");
	for (i = 0; i < 8; i++) {
		db_printf("  dr%d 0x%016lx\n", i, DBREG_DRX((&d), i));
	}
	db_printf("\n");
}
