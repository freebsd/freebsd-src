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

#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/reg.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <ddb/ddb.h>

#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>

db_varfcn_t db_dr0;
db_varfcn_t db_dr1;
db_varfcn_t db_dr2;
db_varfcn_t db_dr3;
db_varfcn_t db_dr4;
db_varfcn_t db_dr5;
db_varfcn_t db_dr6;
db_varfcn_t db_dr7;

/*
 * Machine register set.
 */
struct db_variable db_regs[] = {
	{ "cs",		&ddb_regs.tf_cs,     FCN_NULL },
	{ "ds",		&ddb_regs.tf_ds,     FCN_NULL },
	{ "es",		&ddb_regs.tf_es,     FCN_NULL },
	{ "fs",		&ddb_regs.tf_fs,     FCN_NULL },
#if 0
	{ "gs",		&ddb_regs.tf_gs,     FCN_NULL },
#endif
	{ "ss",		&ddb_regs.tf_ss,     FCN_NULL },
	{ "eax",	&ddb_regs.tf_eax,    FCN_NULL },
	{ "ecx",	&ddb_regs.tf_ecx,    FCN_NULL },
	{ "edx",	&ddb_regs.tf_edx,    FCN_NULL },
	{ "ebx",	&ddb_regs.tf_ebx,    FCN_NULL },
	{ "esp",	&ddb_regs.tf_esp,    FCN_NULL },
	{ "ebp",	&ddb_regs.tf_ebp,    FCN_NULL },
	{ "esi",	&ddb_regs.tf_esi,    FCN_NULL },
	{ "edi",	&ddb_regs.tf_edi,    FCN_NULL },
	{ "eip",	&ddb_regs.tf_eip,    FCN_NULL },
	{ "efl",	&ddb_regs.tf_eflags, FCN_NULL },
	{ "dr0",	NULL,		     db_dr0 },
	{ "dr1",	NULL,		     db_dr1 },
	{ "dr2",	NULL,		     db_dr2 },
	{ "dr3",	NULL,		     db_dr3 },
	{ "dr4",	NULL,		     db_dr4 },
	{ "dr5",	NULL,		     db_dr5 },
	{ "dr6",	NULL,		     db_dr6 },
	{ "dr7",	NULL,		     db_dr7 },
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

/*
 * Stack trace.
 */
#define	INKERNEL(va)	(((vm_offset_t)(va)) >= USRSTACK)

struct i386_frame {
	struct i386_frame	*f_frame;
	int			f_retaddr;
	int			f_arg0;
};

#define NORMAL		0
#define	TRAP		1
#define	INTERRUPT	2
#define	SYSCALL		3

static void db_nextframe __P((struct i386_frame **, db_addr_t *));
static int db_numargs __P((struct i386_frame *));
static void db_print_stack_entry __P((const char *, int, char **, int *, db_addr_t));


static char * watchtype_str __P((int type));
int  i386_set_watch         __P((int watchnum, unsigned int watchaddr, 
                                 int size, int access, struct dbreg * d));
int  i386_clr_watch         __P((int watchnum, struct dbreg * d));
int  db_md_set_watchpoint   __P((db_expr_t addr, db_expr_t size));
int  db_md_clr_watchpoint   __P((db_expr_t addr, db_expr_t size));
void db_md_list_watchpoints __P((void));


/*
 * Figure out how many arguments were passed into the frame at "fp".
 */
static int
db_numargs(fp)
	struct i386_frame *fp;
{
	int	*argp;
	int	inst;
	int	args;

	argp = (int *)db_get_value((int)&fp->f_retaddr, 4, FALSE);
	/*
	 * XXX etext is wrong for LKMs.  We should attempt to interpret
	 * the instruction at the return address in all cases.  This
	 * may require better fault handling.
	 */
	if (argp < (int *)btext || argp >= (int *)etext) {
		args = 5;
	} else {
		inst = db_get_value((int)argp, 4, FALSE);
		if ((inst & 0xff) == 0x59)	/* popl %ecx */
			args = 1;
		else if ((inst & 0xffff) == 0xc483)	/* addl $Ibs, %esp */
			args = ((inst >> 16) & 0xff) / 4;
		else
			args = 5;
	}
	return (args);
}

static void
db_print_stack_entry(name, narg, argnp, argp, callpc)
	const char *name;
	int narg;
	char **argnp;
	int *argp;
	db_addr_t callpc;
{
	db_printf("%s(", name);
	while (narg) {
		if (argnp)
			db_printf("%s=", *argnp++);
		db_printf("%r", db_get_value((int)argp, 4, FALSE));
		argp++;
		if (--narg != 0)
			db_printf(",");
  	}
	db_printf(") at ");
	db_printsym(callpc, DB_STGY_PROC);
	db_printf("\n");
}

/*
 * Figure out the next frame up in the call stack.
 */
static void
db_nextframe(fp, ip)
	struct i386_frame **fp;		/* in/out */
	db_addr_t	*ip;		/* out */
{
	struct trapframe *tf;
	int frame_type;
	int eip, esp, ebp;
	db_expr_t offset;
	const char *sym, *name;

	eip = db_get_value((int) &(*fp)->f_retaddr, 4, FALSE);
	ebp = db_get_value((int) &(*fp)->f_frame, 4, FALSE);

	/*
	 * Figure out frame type.
	 */

	frame_type = NORMAL;

	sym = db_search_symbol(eip, DB_STGY_ANY, &offset);
	db_symbol_values(sym, &name, NULL);
	if (name != NULL) {
		if (!strcmp(name, "calltrap")) {
			frame_type = TRAP;
		} else if (!strncmp(name, "Xresume", 7)) {
			frame_type = INTERRUPT;
		} else if (!strcmp(name, "_Xsyscall")) {
			frame_type = SYSCALL;
		}
	}

	/*
	 * Normal frames need no special processing.
	 */
	if (frame_type == NORMAL) {
		*ip = (db_addr_t) eip;
		*fp = (struct i386_frame *) ebp;
		return;
	}

	db_print_stack_entry(name, 0, 0, 0, eip);

	/*
	 * Point to base of trapframe which is just above the
	 * current frame.
	 */
	tf = (struct trapframe *) ((int)*fp + 8);

	esp = (ISPL(tf->tf_cs) == SEL_UPL) ?  tf->tf_esp : (int)&tf->tf_esp;
	switch (frame_type) {
	case TRAP:
		if (INKERNEL((int) tf)) {
			eip = tf->tf_eip;
			ebp = tf->tf_ebp;
			db_printf(
		    "--- trap %#r, eip = %#r, esp = %#r, ebp = %#r ---\n",
			    tf->tf_trapno, eip, esp, ebp);
		}
		break;
	case SYSCALL:
		if (INKERNEL((int) tf)) {
			eip = tf->tf_eip;
			ebp = tf->tf_ebp;
			db_printf(
		    "--- syscall %#r, eip = %#r, esp = %#r, ebp = %#r ---\n",
			    tf->tf_eax, eip, esp, ebp);
		}
		break;
	case INTERRUPT:
		tf = (struct trapframe *)((int)*fp + 16);
		if (INKERNEL((int) tf)) {
			eip = tf->tf_eip;
			ebp = tf->tf_ebp;
			db_printf(
		    "--- interrupt, eip = %#r, esp = %#r, ebp = %#r ---\n",
			    eip, esp, ebp);
		}
		break;
	default:
		break;
	}

	*ip = (db_addr_t) eip;
	*fp = (struct i386_frame *) ebp;
}

void
db_stack_trace_cmd(addr, have_addr, count, modif)
	db_expr_t addr;
	boolean_t have_addr;
	db_expr_t count;
	char *modif;
{
	struct i386_frame *frame;
	int *argp;
	db_addr_t callpc;
	boolean_t first;

	if (count == -1)
		count = 65535;

	if (!have_addr) {
		frame = (struct i386_frame *)ddb_regs.tf_ebp;
		if (frame == NULL)
			frame = (struct i386_frame *)(ddb_regs.tf_esp - 4);
		callpc = (db_addr_t)ddb_regs.tf_eip;
	} else {
		frame = (struct i386_frame *)addr;
		callpc = (db_addr_t)db_get_value((int)&frame->f_retaddr, 4, FALSE);
	}

	first = TRUE;
	while (count--) {
		struct i386_frame *actframe;
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
		if (first && !have_addr) {
			int instr;

			instr = db_get_value(callpc, 4, FALSE);
			if ((instr & 0x00ffffff) == 0x00e58955) {
				/* pushl %ebp; movl %esp, %ebp */
				actframe = (struct i386_frame *)
					   (ddb_regs.tf_esp - 4);
			} else if ((instr & 0x0000ffff) == 0x0000e589) {
				/* movl %esp, %ebp */
				actframe = (struct i386_frame *)
					   ddb_regs.tf_esp;
				if (ddb_regs.tf_ebp == 0) {
					/* Fake the caller's frame better. */
					frame = actframe;
				}
			} else if ((instr & 0x000000ff) == 0x000000c3) {
				/* ret */
				actframe = (struct i386_frame *)
					   (ddb_regs.tf_esp - 4);
			} else if (offset == 0) {
				/* Probably a symbol in assembler code. */
				actframe = (struct i386_frame *)
					   (ddb_regs.tf_esp - 4);
			}
		}
		first = FALSE;

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
			    db_get_value((int)&actframe->f_retaddr, 4, FALSE);
			continue;
		}

		db_nextframe(&frame, &callpc);

		if (INKERNEL((int) callpc) && !INKERNEL((int) frame)) {
			sym = db_search_symbol(callpc, DB_STGY_ANY, &offset);
			db_symbol_values(sym, &name, NULL);
			db_print_stack_entry(name, 0, 0, 0, callpc);
			break;
		}
		if (!INKERNEL((int) frame)) {
			break;
		}
	}
}

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
					\
	return(0);			\
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
i386_set_watch(watchnum, watchaddr, size, access, d)
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
			if ((d->dr7 & mask) == 0)
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
	d->dr7 &= ~((0x3 << (watchnum*2)) | (0x0f << (watchnum*4+16)));

	/* set drN register to the address, N=watchnum */
	DBREG_DRX(d,watchnum) = watchaddr;

	/* enable the watchpoint */
	d->dr7 |= (0x2 << (watchnum*2)) | (mask << (watchnum*4+16));

	return (watchnum);
}


int
i386_clr_watch(watchnum, d)
	int watchnum;
	struct dbreg * d;
{

	if (watchnum < 0 || watchnum >= 4)
		return (-1);
	
	d->dr7 = d->dr7 & ~((0x3 << (watchnum*2)) | (0x0f << (watchnum*4+16)));
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
		if ((d.dr7 & (3 << (i*2))) == 0)
			avail++;
	}
	
	if (avail*4 < size)
		return (-1);
	
	for (i=0; i<4 && (size != 0); i++) {
		if ((d.dr7 & (3<<(i*2))) == 0) {
			if (size > 4)
				wsize = 4;
			else
				wsize = size;
			if (wsize == 3)
				wsize++;
			i386_set_watch(i, addr, wsize, 
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
		if (d.dr7 & (3 << (i*2))) {
			if ((DBREG_DRX((&d), i) >= addr) && 
			    (DBREG_DRX((&d), i) < addr+size))
				i386_clr_watch(i, &d);
			
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
	db_printf("  watch    status        type  len     address\n"
		  "  -----  --------  ----------  ---  ----------\n");
	for (i=0; i<4; i++) {
		if (d.dr7 & (0x03 << (i*2))) {
			unsigned type, len;
			type = (d.dr7 >> (16+(i*4))) & 3;
			len =  (d.dr7 >> (16+(i*4)+2)) & 3;
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


