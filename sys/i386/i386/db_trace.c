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
 *	$Id: db_trace.c,v 1.2 1993/10/16 14:14:56 rgrimes Exp $
 */

#include "param.h"
#include "systm.h"
#include "proc.h"
#include "ddb/ddb.h"

#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>

/*
 * Machine register set.
 */
struct db_variable db_regs[] = {
	"cs",	(int *)&ddb_regs.tf_cs,  FCN_NULL,
	"ds",	(int *)&ddb_regs.tf_ds,  FCN_NULL,
	"es",	(int *)&ddb_regs.tf_es,  FCN_NULL,
#if 0
	"fs",	(int *)&ddb_regs.tf_fs,  FCN_NULL,
	"gs",	(int *)&ddb_regs.tf_gs,  FCN_NULL,
#endif
	"ss",	(int *)&ddb_regs.tf_ss,  FCN_NULL,
	"eax",	(int *)&ddb_regs.tf_eax, FCN_NULL,
	"ecx",	(int *)&ddb_regs.tf_ecx, FCN_NULL,
	"edx",	(int *)&ddb_regs.tf_edx, FCN_NULL,
	"ebx",	(int *)&ddb_regs.tf_ebx, FCN_NULL,
	"esp",	(int *)&ddb_regs.tf_esp,FCN_NULL,
	"ebp",	(int *)&ddb_regs.tf_ebp, FCN_NULL,
	"esi",	(int *)&ddb_regs.tf_esi, FCN_NULL,
	"edi",	(int *)&ddb_regs.tf_edi, FCN_NULL,
	"eip",	(int *)&ddb_regs.tf_eip, FCN_NULL,
	"efl",	(int *)&ddb_regs.tf_eflags, FCN_NULL,
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

/*
 * Stack trace.
 */
#define	INKERNEL(va)	(((vm_offset_t)(va)) >= VM_MIN_KERNEL_ADDRESS)

struct i386_frame {
	struct i386_frame	*f_frame;
	int			f_retaddr;
	int			f_arg0;
};

#define	TRAP		1
#define	INTERRUPT	2

db_addr_t	db_trap_symbol_value = 0;
db_addr_t	db_kdintr_symbol_value = 0;
boolean_t	db_trace_symbols_found = FALSE;

void
db_find_trace_symbols()
{
	db_expr_t	value;
	if (db_value_of_name("_trap", &value))
	    db_trap_symbol_value = (db_addr_t) value;
	if (db_value_of_name("_kdintr", &value))
	    db_kdintr_symbol_value = (db_addr_t) value;
	db_trace_symbols_found = TRUE;
}

/*
 * Figure out how many arguments were passed into the frame at "fp".
 */
int
db_numargs(fp)
	struct i386_frame *fp;
{
	int	*argp;
	int	inst;
	int	args;
	extern char	etext[];

	argp = (int *)db_get_value((int)&fp->f_retaddr, 4, FALSE);
	if (argp < (int *)VM_MIN_KERNEL_ADDRESS || argp > (int *)etext)
	    args = 5;
	else {
	    inst = db_get_value((int)argp, 4, FALSE);
	    if ((inst & 0xff) == 0x59)	/* popl %ecx */
		args = 1;
	    else if ((inst & 0xffff) == 0xc483)	/* addl %n, %esp */
		args = ((inst >> 16) & 0xff) / 4;
	    else
		args = 5;
	}
	return (args);
}

/* 
 * Figure out the next frame up in the call stack.  
 * For trap(), we print the address of the faulting instruction and 
 *   proceed with the calling frame.  We return the ip that faulted.
 *   If the trap was caused by jumping through a bogus pointer, then
 *   the next line in the backtrace will list some random function as 
 *   being called.  It should get the argument list correct, though.  
 *   It might be possible to dig out from the next frame up the name
 *   of the function that faulted, but that could get hairy.
 */
void
db_nextframe(fp, ip, argp, is_trap)
	struct i386_frame **fp;		/* in/out */
	db_addr_t	*ip;		/* out */
	int *argp;			/* in */
	int is_trap;			/* in */
{
	struct i386_saved_state *saved_regs;

	if (is_trap == 0) {
	    *ip = (db_addr_t)
			db_get_value((int) &(*fp)->f_retaddr, 4, FALSE);
	    *fp = (struct i386_frame *)
			db_get_value((int) &(*fp)->f_frame, 4, FALSE);
	} else {
	    /*
	     * We know that trap() has 1 argument and we know that
	     * it is an (int *).
	     */
	    saved_regs = (struct i386_saved_state *)
			db_get_value((int)argp, 4, FALSE);
	    db_printf("--- trap (number %d) ---\n",
		      saved_regs->tf_trapno & 0xffff);
	    db_printsym(saved_regs->tf_eip, DB_STGY_XTRN);
	    db_printf(":\n");
	    *fp = (struct i386_frame *)saved_regs->tf_ebp;
	    *ip = (db_addr_t)saved_regs->tf_eip;
	}

}

void
db_stack_trace_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	boolean_t	have_addr;
	db_expr_t	count;
	char		*modif;
{
	struct i386_frame *frame, *lastframe;
	int		*argp;
	db_addr_t	callpc;
	int		is_trap;
	boolean_t	kernel_only = TRUE;
	boolean_t	trace_thread = FALSE;

	if (!db_trace_symbols_found)
	    db_find_trace_symbols();

	{
	    register char *cp = modif;
	    register char c;

	    while ((c = *cp++) != 0) {
		if (c == 't')
		    trace_thread = TRUE;
		if (c == 'u')
		    kernel_only = FALSE;
	    }
	}

	if (count == -1)
	    count = 65535;

	if (!have_addr) {
	    frame = (struct i386_frame *)ddb_regs.tf_ebp;
	    callpc = (db_addr_t)ddb_regs.tf_eip;
	}
	else if (trace_thread) {
		printf ("db_trace.c: can't trace thread\n");
	}
	else {
	    frame = (struct i386_frame *)addr;
	    callpc = (db_addr_t)db_get_value((int)&frame->f_retaddr, 4, FALSE);
	}

	lastframe = 0;
	while (count-- && frame != 0) {
	    register int narg;
	    char *	name;
	    db_expr_t	offset;

	    if (INKERNEL((int)frame) && callpc == db_trap_symbol_value) {
		narg = 1;
		is_trap = TRAP;
	    }
	    else
	    if (INKERNEL((int)frame) && callpc == db_kdintr_symbol_value) {
		is_trap = INTERRUPT;
		narg = 0;
	    }
	    else {
		is_trap = 0;
		narg = db_numargs(frame);
	    }

	    db_find_sym_and_offset(callpc, &name, &offset);
	    db_printf("%s(", name);

	    argp = &frame->f_arg0;
	    while (narg) {
		db_printf("%x", db_get_value((int)argp, 4, FALSE));
		argp++;
		if (--narg != 0)
		    db_printf(",");
	    }
	    db_printf(") at ");
	    db_printsym(callpc, DB_STGY_XTRN);
	    db_printf("\n");

	    lastframe = frame;
	    db_nextframe(&frame, &callpc, &frame->f_arg0, is_trap);

	    if (frame == 0) {
		/* end of chain */
		break;
	    }
	    if (INKERNEL((int)frame)) {
		/* staying in kernel */
		if (frame <= lastframe) {
		    db_printf("Bad frame pointer: 0x%x\n", frame);
		    break;
		}
	    }
	    else if (INKERNEL((int)lastframe)) {
		/* switch from user to kernel */
		if (kernel_only)
		    break;	/* kernel stack only */
	    }
	    else {
		/* in user */
		if (frame <= lastframe) {
		    db_printf("Bad frame pointer: 0x%x\n", frame);
		    break;
		}
	    }
	}
}
