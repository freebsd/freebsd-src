/*
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
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

#if 0
db_varfcn_t db_dr0;
db_varfcn_t db_dr1;
db_varfcn_t db_dr2;
db_varfcn_t db_dr3;
db_varfcn_t db_dr4;
db_varfcn_t db_dr5;
db_varfcn_t db_dr6;
db_varfcn_t db_dr7;
#endif

/*
 * Machine register set.
 */
struct db_variable db_regs[] = {
	{ "cs",		&ddb_regs.tf_cs,     FCN_NULL },
#if 0
	{ "ds",		&ddb_regs.tf_ds,     FCN_NULL },
	{ "es",		&ddb_regs.tf_es,     FCN_NULL },
	{ "fs",		&ddb_regs.tf_fs,     FCN_NULL },
	{ "gs",		&ddb_regs.tf_gs,     FCN_NULL },
#endif
	{ "ss",		&ddb_regs.tf_ss,     FCN_NULL },
	{ "rax",	&ddb_regs.tf_rax,    FCN_NULL },
	{ "rcx",	&ddb_regs.tf_rcx,    FCN_NULL },
	{ "rdx",	&ddb_regs.tf_rdx,    FCN_NULL },
	{ "rbx",	&ddb_regs.tf_rbx,    FCN_NULL },
	{ "rsp",	&ddb_regs.tf_rsp,    FCN_NULL },
	{ "rbp",	&ddb_regs.tf_rbp,    FCN_NULL },
	{ "rsi",	&ddb_regs.tf_rsi,    FCN_NULL },
	{ "rdi",	&ddb_regs.tf_rdi,    FCN_NULL },
	{ "r8",		&ddb_regs.tf_r8,     FCN_NULL },
	{ "r9",		&ddb_regs.tf_r9,     FCN_NULL },
	{ "r10",	&ddb_regs.tf_r10,    FCN_NULL },
	{ "r11",	&ddb_regs.tf_r11,    FCN_NULL },
	{ "r12",	&ddb_regs.tf_r12,    FCN_NULL },
	{ "r13",	&ddb_regs.tf_r13,    FCN_NULL },
	{ "r14",	&ddb_regs.tf_r14,    FCN_NULL },
	{ "r15",	&ddb_regs.tf_r15,    FCN_NULL },
	{ "rip",	&ddb_regs.tf_rip,    FCN_NULL },
	{ "rflags",	&ddb_regs.tf_rflags, FCN_NULL },
#if 0
	{ "dr0",	NULL,		     db_dr0 },
	{ "dr1",	NULL,		     db_dr1 },
	{ "dr2",	NULL,		     db_dr2 },
	{ "dr3",	NULL,		     db_dr3 },
	{ "dr4",	NULL,		     db_dr4 },
	{ "dr5",	NULL,		     db_dr5 },
	{ "dr6",	NULL,		     db_dr6 },
	{ "dr7",	NULL,		     db_dr7 },
#endif
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

/*
 * Stack trace.
 */
#define	INKERNEL(va)	(((vm_offset_t)(va)) >= USRSTACK)

struct amd64_frame {
	struct amd64_frame	*f_frame;
	long			f_retaddr;
	long			f_arg0;
};

#define NORMAL		0
#define	TRAP		1
#define	INTERRUPT	2
#define	SYSCALL		3

static void db_nextframe(struct amd64_frame **, db_addr_t *, struct proc *);
static int db_numargs(struct amd64_frame *);
static void db_print_stack_entry(const char *, int, char **, long *, db_addr_t);
static void decode_syscall(int, struct proc *);
static void db_trace_one_stack(int count, boolean_t have_addr,
		struct proc *p, struct amd64_frame *frame, db_addr_t callpc);


#if 0
static char * watchtype_str(int type);
int  amd64_set_watch(int watchnum, unsigned int watchaddr, int size, int access,
		    struct dbreg * d);
int  amd64_clr_watch(int watchnum, struct dbreg * d);
#endif
int  db_md_set_watchpoint(db_expr_t addr, db_expr_t size);
int  db_md_clr_watchpoint(db_expr_t addr, db_expr_t size);
void db_md_list_watchpoints(void);


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
decode_syscall(number, p)
	int number;
	struct proc *p;
{
	c_db_sym_t sym;
	db_expr_t diff;
	sy_call_t *f;
	const char *symname;

	db_printf(" (%d", number);
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
db_nextframe(fp, ip, p)
	struct amd64_frame **fp;	/* in/out */
	db_addr_t	*ip;		/* out */
	struct proc	*p;		/* in */
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
	 * Figure out frame type.
	 */
	frame_type = NORMAL;
	sym = db_search_symbol(rip, DB_STGY_ANY, &offset);
	db_symbol_values(sym, &name, NULL);
	if (name != NULL) {
		if (strcmp(name, "calltrap") == 0 ||
		    strcmp(name, "fork_trampoline") == 0)
			frame_type = TRAP;
		else if (strncmp(name, "Xintr", 5) == 0 ||
		    strncmp(name, "Xfastintr", 9) == 0)
			frame_type = INTERRUPT;
		else if (strcmp(name, "Xfast_syscall") == 0)
			frame_type = SYSCALL;
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
		rsp = (ISPL(tf->tf_cs) == SEL_UPL) ?
		    tf->tf_rsp : (long)&tf->tf_rsp;
		rip = tf->tf_rip;
		rbp = tf->tf_rbp;
		switch (frame_type) {
		case TRAP:
			db_printf("--- trap %#lr", tf->tf_trapno);
			break;
		case SYSCALL:
			db_printf("--- syscall");
			decode_syscall(tf->tf_rax, p);
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

void
db_stack_trace_cmd(addr, have_addr, count, modif)
	db_expr_t addr;
	boolean_t have_addr;
	db_expr_t count;
	char *modif;
{
	struct amd64_frame *frame;
	struct proc *p;
	struct pcb *pcb;
	struct thread *td;
	db_addr_t callpc;
	pid_t pid;

	if (count == -1)
		count = 1024;

	if (!have_addr) {
		td = curthread;
		p = td->td_proc;
		frame = (struct amd64_frame *)ddb_regs.tf_rbp;
		if (frame == NULL)
			frame = (struct amd64_frame *)(ddb_regs.tf_rsp - 8);
		callpc = (db_addr_t)ddb_regs.tf_rip;
	} else if (!INKERNEL(addr)) {
		pid = (addr % 16) + ((addr >> 4) % 16) * 10 +
		    ((addr >> 8) % 16) * 100 + ((addr >> 12) % 16) * 1000 +
		    ((addr >> 16) % 16) * 10000;
		/*
		 * The pcb for curproc is not valid at this point,
		 * so fall back to the default case.
		 */
		if (pid == curthread->td_proc->p_pid) {
			td = curthread;
			p = td->td_proc;
			frame = (struct amd64_frame *)ddb_regs.tf_rbp;
			if (frame == NULL)
				frame = (struct amd64_frame *)
				    (ddb_regs.tf_rsp - 8);
			callpc = (db_addr_t)ddb_regs.tf_rip;
		} else {

			/* sx_slock(&allproc_lock); */
			LIST_FOREACH(p, &allproc, p_list) {
				if (p->p_pid == pid)
					break;
			}
			/* sx_sunlock(&allproc_lock); */
			if (p == NULL) {
				db_printf("pid %d not found\n", pid);
				return;
			}
			if ((p->p_sflag & PS_INMEM) == 0) {
				db_printf("pid %d swapped out\n", pid);
				return;
			}
			pcb = FIRST_THREAD_IN_PROC(p)->td_pcb;	/* XXXKSE */
			frame = (struct amd64_frame *)pcb->pcb_rbp;
			if (frame == NULL)
				frame = (struct amd64_frame *)
				    (pcb->pcb_rsp - 8);
			callpc = (db_addr_t)pcb->pcb_rip;
		}
	} else {
		p = NULL;
		frame = (struct amd64_frame *)addr;
		callpc = (db_addr_t)db_get_value((long)&frame->f_retaddr, 8, FALSE);
		frame = frame->f_frame;
	}
	db_trace_one_stack(count, have_addr, p, frame, callpc);
}

void
db_stack_thread(db_expr_t addr, boolean_t have_addr,
		db_expr_t count, char *modif)
{
	struct amd64_frame *frame;
	struct thread *td;
	struct proc *p;
	struct pcb *pcb;
	db_addr_t callpc;

	if (!have_addr)
		return;
	if (!INKERNEL(addr)) {
		printf("bad thread address");
		return;
	}
	td = (struct thread *)addr;
	/* quick sanity check */
	if ((p = td->td_proc) != td->td_ksegrp->kg_proc)
		return;
	if (TD_IS_SWAPPED(td)) {
		db_printf("thread at %p swapped out\n", td);
		return;
	}
	if (td == curthread) {
		frame = (struct amd64_frame *)ddb_regs.tf_rbp;
		if (frame == NULL)
			frame = (struct amd64_frame *)(ddb_regs.tf_rsp - 8);
		callpc = (db_addr_t)ddb_regs.tf_rip;
	} else {
		pcb = td->td_pcb;
		frame = (struct amd64_frame *)pcb->pcb_rbp;
		if (frame == NULL)
			frame = (struct amd64_frame *) (pcb->pcb_rsp - 8);
		callpc = (db_addr_t)pcb->pcb_rip;
	}
	db_trace_one_stack(count, have_addr, p, frame, callpc);
}

static void
db_trace_one_stack(int count, boolean_t have_addr,
		struct proc *p, struct amd64_frame *frame, db_addr_t callpc)
{
	long *argp;
	boolean_t first;

	first = TRUE;
	while (count--) {
		struct amd64_frame *actframe;
		int		narg;
		const char *	name;
		db_expr_t	offset;
		c_db_sym_t	sym;
#define MAXNARG	16
		char	*argnames[MAXNARG], **argnp = NULL;

		sym = db_search_symbol(callpc, DB_STGY_ANY, &offset);
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
			if (!have_addr) {
				int instr;

				instr = db_get_value(callpc, 4, FALSE);
				if ((instr & 0xffffffff) == 0xe5894855) {
					/* pushq %rbp; movq %rsp, %rbp */
					actframe = (struct amd64_frame *)
					    (ddb_regs.tf_rsp - 8);
				} else if ((instr & 0x00ffffff) == 0x00e58948) {
					/* movq %rsp, %rbp */
					actframe = (struct amd64_frame *)
					    ddb_regs.tf_rsp;
					if (ddb_regs.tf_rbp == 0) {
						/* Fake caller's frame better. */
						frame = actframe;
					}
				} else if ((instr & 0x000000ff) == 0x000000c3) {
					/* ret */
					actframe = (struct amd64_frame *)
					    (ddb_regs.tf_rsp - 8);
				} else if (offset == 0) {
					/* Probably a symbol in assembler code. */
					actframe = (struct amd64_frame *)
					    (ddb_regs.tf_rsp - 8);
				}
			} else if (strcmp(name, "fork_trampoline") == 0) {
				/*
				 * Don't try to walk back on a stack for a
				 * process that hasn't actually been run yet.
				 */
				db_print_stack_entry(name, 0, 0, 0, callpc);
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

		db_print_stack_entry(name, narg, argnp, argp, callpc);

		if (actframe != frame) {
			/* `frame' belongs to caller. */
			callpc = (db_addr_t)
			    db_get_value((long)&actframe->f_retaddr, 8, FALSE);
			continue;
		}

		db_nextframe(&frame, &callpc, p);

		if (INKERNEL((long) callpc) && !INKERNEL((long) frame)) {
			sym = db_search_symbol(callpc, DB_STGY_ANY, &offset);
			db_symbol_values(sym, &name, NULL);
			db_print_stack_entry(name, 0, 0, 0, callpc);
			break;
		}
		if (!INKERNEL((long) frame)) {
			break;
		}
	}
}

void
db_print_backtrace(void)
{
	register_t ebp;

	__asm __volatile("movq %%rbp,%0" : "=r" (ebp));
	db_stack_trace_cmd(ebp, 1, -1, NULL);
}

#if 0
#define DB_DRX_FUNC(reg)		\
int					\
db_ ## reg (vp, valuep, op)		\
	struct db_variable *vp;		\
	db_expr_t * valuep;		\
	int op;				\
{					\
	if (op == DB_VAR_GET)		\
		*valuep = r ## reg ();	\
	else				\
		load_ ## reg (*valuep); \
	return (0);			\
} 

DB_DRX_FUNC(dr0)
DB_DRX_FUNC(dr1)
DB_DRX_FUNC(dr2)
DB_DRX_FUNC(dr3)
DB_DRX_FUNC(dr4)
DB_DRX_FUNC(dr5)
DB_DRX_FUNC(dr6)
DB_DRX_FUNC(dr7)

int
amd64_set_watch(watchnum, watchaddr, size, access, d)
	int watchnum;
	unsigned int watchaddr;
	int size;
	int access;
	struct dbreg * d;
{
	int i;
	unsigned int mask;
	
	if (watchnum == -1) {
		for (i = 0, mask = 0x3; i < 4; i++, mask <<= 2)
			if ((d->dr[7] & mask) == 0)
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
	default : return (-1); break;
	}
	
	/*
	 * we can watch a 1, 2, or 4 byte sized location
	 */
	switch (size) {
	case 1	: mask = 0x00; break;
	case 2	: mask = 0x01 << 2; break;
	case 4	: mask = 0x03 << 2; break;
	default : return (-1); break;
	}

	mask |= access;

	/* clear the bits we are about to affect */
	d->dr[7] &= ~((0x3 << (watchnum*2)) | (0x0f << (watchnum*4+16)));

	/* set drN register to the address, N=watchnum */
	DBREG_DRX(d,watchnum) = watchaddr;

	/* enable the watchpoint */
	d->dr[7] |= (0x2 << (watchnum*2)) | (mask << (watchnum*4+16));

	return (watchnum);
}


int
amd64_clr_watch(watchnum, d)
	int watchnum;
	struct dbreg * d;
{

	if (watchnum < 0 || watchnum >= 4)
		return (-1);
	
	d->dr[7] = d->dr[7] & ~((0x3 << (watchnum*2)) | (0x0f << (watchnum*4+16)));
	DBREG_DRX(d,watchnum) = 0;
	
	return (0);
}


int
db_md_set_watchpoint(addr, size)
	db_expr_t addr;
	db_expr_t size;
{
	int avail, wsize;
	int i;
	struct dbreg d;
	
	fill_dbregs(NULL, &d);
	
	avail = 0;
	for(i=0; i<4; i++) {
		if ((d.dr[7] & (3 << (i*2))) == 0)
			avail++;
	}
	
	if (avail*4 < size)
		return (-1);
	
	for (i=0; i<4 && (size != 0); i++) {
		if ((d.dr[7] & (3<<(i*2))) == 0) {
			if (size > 4)
				wsize = 4;
			else
				wsize = size;
			if (wsize == 3)
				wsize++;
			amd64_set_watch(i, addr, wsize, 
				       DBREG_DR7_WRONLY, &d);
			addr += wsize;
			size -= wsize;
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
	int i;
	struct dbreg d;

	fill_dbregs(NULL, &d);

	for(i=0; i<4; i++) {
		if (d.dr[7] & (3 << (i*2))) {
			if ((DBREG_DRX((&d), i) >= addr) && 
			    (DBREG_DRX((&d), i) < addr+size))
				amd64_clr_watch(i, &d);
			
		}
	}
	
	set_dbregs(NULL, &d);
	
	return(0);
}


static 
char *
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
	int i;
	struct dbreg d;

	fill_dbregs(NULL, &d);

	db_printf("\nhardware watchpoints:\n");
	db_printf("  watch    status        type  len     address\n");
	db_printf("  -----  --------  ----------  ---  ----------\n");
	for (i=0; i<4; i++) {
		if (d.dr[7] & (0x03 << (i*2))) {
			unsigned type, len;
			type = (d.dr[7] >> (16+(i*4))) & 3;
			len =  (d.dr[7] >> (16+(i*4)+2)) & 3;
			db_printf("  %-5d  %-8s  %10s  %3d  0x%08x\n",
				  i, "enabled", watchtype_str(type), 
				  len+1, DBREG_DRX((&d),i));
		}
		else {
			db_printf("  %-5d  disabled\n", i);
		}
	}
	
	db_printf("\ndebug register values:\n");
	for (i=0; i<8; i++) {
		db_printf("  dr%d 0x%08x\n", i, DBREG_DRX((&d),i));
	}
	db_printf("\n");
}

#else
int
db_md_set_watchpoint(addr, size)
	db_expr_t addr;
	db_expr_t size;
{
	return (-1);
}

int
db_md_clr_watchpoint(addr, size)
	db_expr_t addr;
	db_expr_t size;
{
	return (-1);
}

void
db_md_list_watchpoints()
{
}
#endif
