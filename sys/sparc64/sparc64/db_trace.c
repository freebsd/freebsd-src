/*-
 * Copyright (c) 2001 Jake Burkholder.
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/linker_set.h>
#include <sys/proc.h>
#include <sys/sysent.h>
#include <sys/user.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>

#include <machine/cpu.h>
#include <machine/trap.h>
#include <machine/vmparam.h>

#include <ddb/ddb.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>
#include <ddb/db_watch.h>

static db_varfcn_t db_show_in0;
static db_varfcn_t db_show_in1;
static db_varfcn_t db_show_in2;
static db_varfcn_t db_show_in3;
static db_varfcn_t db_show_in4;
static db_varfcn_t db_show_in5;
static db_varfcn_t db_show_in6;
static db_varfcn_t db_show_in7;
static db_varfcn_t db_show_local0;
static db_varfcn_t db_show_local1;
static db_varfcn_t db_show_local2;
static db_varfcn_t db_show_local3;
static db_varfcn_t db_show_local4;
static db_varfcn_t db_show_local5;
static db_varfcn_t db_show_local6;
static db_varfcn_t db_show_local7;

static int db_print_trap(struct proc *p, struct trapframe *);

#define	INKERNEL(va) \
	((va) >= VM_MIN_KERNEL_ADDRESS && (va) <= VM_MAX_KERNEL_ADDRESS)

struct	db_variable db_regs[] = {
	{ "g0",	&ddb_regs.tf_global[0], FCN_NULL },
	{ "g1",	&ddb_regs.tf_global[1], FCN_NULL },
	{ "g2",	&ddb_regs.tf_global[2], FCN_NULL },
	{ "g3",	&ddb_regs.tf_global[3], FCN_NULL },
	{ "g4",	&ddb_regs.tf_global[4], FCN_NULL },
	{ "g5",	&ddb_regs.tf_global[5], FCN_NULL },
	{ "g6",	&ddb_regs.tf_global[6], FCN_NULL },
	{ "g7",	&ddb_regs.tf_global[7], FCN_NULL },
	{ "i0", NULL, db_show_in0 },
	{ "i1", NULL, db_show_in1 },
	{ "i2", NULL, db_show_in2 },
	{ "i3", NULL, db_show_in3 },
	{ "i4", NULL, db_show_in4 },
	{ "i5", NULL, db_show_in5 },
	{ "i6", NULL, db_show_in6 },
	{ "i7", NULL, db_show_in7 },
	{ "l0", NULL, db_show_local0 },
	{ "l1", NULL, db_show_local1 },
	{ "l2", NULL, db_show_local2 },
	{ "l3", NULL, db_show_local3 },
	{ "l4", NULL, db_show_local4 },
	{ "l5", NULL, db_show_local5 },
	{ "l6", NULL, db_show_local6 },
	{ "l7", NULL, db_show_local7 },
	{ "tstate", &ddb_regs.tf_tstate, FCN_NULL },
	{ "tpc", &ddb_regs.tf_tpc, FCN_NULL },
	{ "tnpc", &ddb_regs.tf_tnpc, FCN_NULL }
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

void
db_stack_trace_cmd(db_expr_t addr, boolean_t have_addr, db_expr_t count,
		   char *modif)
{
	struct trapframe *tf;
	struct frame *fp;
	struct proc *p;
	struct thread *td;
	const char *name;
	c_db_sym_t sym;
	db_expr_t offset;
	db_expr_t value;
	db_addr_t nfp;
	db_addr_t npc;
	db_addr_t pc;
	int trap;
	int user;
	pid_t pid;

	trap = 0;
	user = 0;
	npc = 0;
	if (count == -1)
		count = 1024;
	if (!have_addr) {
		td = curthread;
		p = td->td_proc;
		addr = DDB_REGS->tf_out[6];
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
		} else {
			/* search for pid */
		}
		db_printf("trace pid not implemented\n");
		return;
	}
	fp = (struct frame *)(addr + SPOFF);

	while (count-- && !user) {
		pc = (db_addr_t)db_get_value((db_addr_t)&fp->f_pc,
		    sizeof(db_addr_t), FALSE);
		if (trap) {
			pc = npc;
			trap = 0;
		}
		if (!INKERNEL((vm_offset_t)pc))
			break;
		sym = db_search_symbol(pc, DB_STGY_ANY, &offset);
		if (sym == C_DB_SYM_NULL) {
			value = 0;
			name = NULL;
		} else
			db_symbol_values(sym, &name, &value);
		if (name == NULL)
			name = "(null)";
		if (bcmp(name, "tl0_", 4) == 0 ||
		    bcmp(name, "tl1_", 4) == 0) {
			nfp = db_get_value((db_addr_t)&fp->f_fp,
			    sizeof(u_long), FALSE) + SPOFF;
			tf = (struct trapframe *)(nfp + sizeof(*fp));
			npc = db_get_value((db_addr_t)&tf->tf_tpc,
			    sizeof(u_long), FALSE);
			user = db_print_trap(curthread->td_proc, tf);
			trap = 1;
		} else {
			db_printf("%s() at ", name);
			db_printsym(pc, DB_STGY_PROC);
			db_printf("\n");
		}
		fp = (struct frame *)(db_get_value((db_addr_t)&fp->f_fp,
		   sizeof(u_long), FALSE) + SPOFF);
	}
}

static int
db_print_trap(struct proc *p, struct trapframe *tf)
{
	const char *symname;
	struct mmuframe *mf;
	c_db_sym_t sym;
	db_expr_t diff;
	db_addr_t func;
	db_addr_t tpc;
	u_long type;
	u_long va;
	u_long code;

	type = db_get_value((db_addr_t)&tf->tf_type, sizeof(db_addr_t), FALSE);
	db_printf("-- %s", trap_msg[type & ~T_KERNEL]);
	switch (type & ~T_KERNEL) {
	case T_ALIGN:
		mf = (struct mmuframe *)db_get_value((db_addr_t)&tf->tf_arg,
		    sizeof(void *), FALSE);
		va = (u_long)db_get_value((db_addr_t)&mf->mf_sfar,
		    sizeof(u_long), FALSE);
		db_printf(" va=%#lx", va);
		break;
	case T_SYSCALL:
		code = db_get_value((db_addr_t)&tf->tf_global[1],
		    sizeof(u_long), FALSE);
		db_printf(" (%ld", code);
		if (code >= 0 && code < p->p_sysent->sv_size) {
			func = (db_addr_t)p->p_sysent->sv_table[code].sy_call;
			sym = db_search_symbol(func, DB_STGY_ANY, &diff);
			if (sym != DB_SYM_NULL && diff == 0) {
				db_symbol_values(sym, &symname, NULL);
				db_printf(", %s, %s", p->p_sysent->sv_name,
				    symname);
			}
			db_printf(")");
		}
		break;
	default:
		break;
	}
	tpc = db_get_value((db_addr_t)&tf->tf_tpc, sizeof(db_addr_t), FALSE);
	db_printf(" -- at ");
	db_printsym(tpc, DB_STGY_PROC);
	db_printf("\n");
	return ((type & T_KERNEL) == 0);
}

#if 0
DB_COMMAND(down, db_frame_down)
{
	struct kdbframe *kfp;
	struct frame *fp;
	u_long cfp;
	u_long ofp;

	kfp = (struct kdbframe *)DDB_REGS->tf_arg;
	fp = (struct frame *)(kfp->kf_fp + SPOFF);
	cfp = kfp->kf_cfp;
	for (;;) {
		if (!INKERNEL((u_long)fp)) {
			db_printf("already at bottom\n");
			break;
		}
		ofp = db_get_value((db_addr_t)&fp->f_fp, sizeof(u_long),
		    FALSE);
		if (ofp == cfp) {
			kfp->kf_cfp = (u_long)fp - SPOFF;
			break;
		}
		fp = (struct frame *)(ofp + SPOFF);
	}
}

DB_COMMAND(up, db_frame_up)
{
	struct kdbframe *kfp;
	struct frame *cfp;

	kfp = (struct kdbframe *)DDB_REGS->tf_arg;
	cfp = (struct frame *)(kfp->kf_cfp + SPOFF);
	if (!INKERNEL((u_long)cfp)) {
		db_printf("already at top\n");
		return;
	}
	kfp->kf_cfp = db_get_value((db_addr_t)&cfp->f_fp, sizeof(u_long),
	    FALSE);
}
#endif

#define	DB_SHOW_REG(name, num)						\
static int								\
db_show_ ## name ## num(struct db_variable *dp, db_expr_t *vp, int op)	\
{									\
	struct frame *fp;						\
									\
	fp = (struct frame *)(DDB_REGS->tf_out[6] + SPOFF);		\
	if (op == DB_VAR_GET)						\
		*vp = db_get_value((db_addr_t)&fp->f_ ## name ## [num],	\
		    sizeof(u_long), FALSE);				\
	else								\
		db_put_value((db_addr_t)&fp->f_ ## name ## [num],	\
		    sizeof(u_long), *vp);				\
	return (0);							\
}

DB_SHOW_REG(in, 0)
DB_SHOW_REG(in, 1)
DB_SHOW_REG(in, 2)
DB_SHOW_REG(in, 3)
DB_SHOW_REG(in, 4)
DB_SHOW_REG(in, 5)
DB_SHOW_REG(in, 6)
DB_SHOW_REG(in, 7)
DB_SHOW_REG(local, 0)
DB_SHOW_REG(local, 1)
DB_SHOW_REG(local, 2)
DB_SHOW_REG(local, 3)
DB_SHOW_REG(local, 4)
DB_SHOW_REG(local, 5)
DB_SHOW_REG(local, 6)
DB_SHOW_REG(local, 7)
