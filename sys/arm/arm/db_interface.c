/*	$NetBSD: db_interface.c,v 1.33 2003/08/25 04:51:10 mrg Exp $	*/

/* 
 * Copyright (c) 1996 Scott K. Stevens
 *
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
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
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
 *	From: db_interface.c,v 2.4 1991/02/05 17:11:13 mrt (CMU)
 */

/*
 * Interface to new debugger.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/systm.h>	/* just for boothowto */
#include <sys/exec.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <machine/db_machdep.h>
#include <machine/katelib.h>
#include <machine/vmparam.h>
#include <machine/cpu.h>

#include <ddb/ddb.h>
#include <ddb/db_access.h>
#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_variables.h>
#include <ddb/db_sym.h>
#include <sys/cons.h>

static int nil;

db_regs_t ddb_regs;
int db_access_und_sp (struct db_variable *, db_expr_t *, int);
int db_access_abt_sp (struct db_variable *, db_expr_t *, int);
int db_access_irq_sp (struct db_variable *, db_expr_t *, int);
u_int db_fetch_reg (int, db_regs_t *);

int db_trapper __P((u_int, u_int, trapframe_t *, int));

struct db_variable db_regs[] = {
	{ "spsr", (int *)&DDB_REGS->tf_spsr, FCN_NULL, },
	{ "r0", (int *)&DDB_REGS->tf_r0, FCN_NULL, },
	{ "r1", (int *)&DDB_REGS->tf_r1, FCN_NULL, },
	{ "r2", (int *)&DDB_REGS->tf_r2, FCN_NULL, },
	{ "r3", (int *)&DDB_REGS->tf_r3, FCN_NULL, },
	{ "r4", (int *)&DDB_REGS->tf_r4, FCN_NULL, },
	{ "r5", (int *)&DDB_REGS->tf_r5, FCN_NULL, },
	{ "r6", (int *)&DDB_REGS->tf_r6, FCN_NULL, },
	{ "r7", (int *)&DDB_REGS->tf_r7, FCN_NULL, },
	{ "r8", (int *)&DDB_REGS->tf_r8, FCN_NULL, },
	{ "r9", (int *)&DDB_REGS->tf_r9, FCN_NULL, },
	{ "r10", (int *)&DDB_REGS->tf_r10, FCN_NULL, },
	{ "r11", (int *)&DDB_REGS->tf_r11, FCN_NULL, },
	{ "r12", (int *)&DDB_REGS->tf_r12, FCN_NULL, },
	{ "usr_sp", (int *)&DDB_REGS->tf_usr_sp, FCN_NULL, },
	{ "usr_lr", (int *)&DDB_REGS->tf_usr_lr, FCN_NULL, },
	{ "svc_sp", (int *)&DDB_REGS->tf_svc_sp, FCN_NULL, },
	{ "svc_lr", (int *)&DDB_REGS->tf_svc_lr, FCN_NULL, },
	{ "pc", (int *)&DDB_REGS->tf_pc, FCN_NULL, },
	{ "und_sp", &nil, db_access_und_sp, },
	{ "abt_sp", &nil, db_access_abt_sp, },
	{ "irq_sp", &nil, db_access_irq_sp, },
};

struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

int	db_active = 0;

int
db_access_und_sp(struct db_variable *vp, db_expr_t *valp, int rw)
{

	if (rw == DB_VAR_GET)
		*valp = get_stackptr(PSR_UND32_MODE);
	return(0);
}

int
db_access_abt_sp(struct db_variable *vp, db_expr_t *valp, int rw)
{

	if (rw == DB_VAR_GET)
		*valp = get_stackptr(PSR_ABT32_MODE);
	return(0);
}

int
db_access_irq_sp(struct db_variable *vp, db_expr_t *valp, int rw)
{

	if (rw == DB_VAR_GET)
		*valp = get_stackptr(PSR_IRQ32_MODE);
	return(0);
}

#ifdef DDB
/*
 *  kdb_trap - field a TRACE or BPT trap
 */
int
kdb_trap(int type, db_regs_t *regs)
{
	int s;

	switch (type) {
	case T_BREAKPOINT:	/* breakpoint */
	case -1:		/* keyboard interrupt */
		break;
	default:
		break;
	}

	/* Should switch to kdb`s own stack here. */

	ddb_regs = *regs;

	s = splhigh();
	db_active++;
	db_trap(type, 0/*code*/);
	db_active--;
	splx(s);

	*regs = ddb_regs;

	return (1);
}
#endif

void
db_show_mdpcpu(struct pcpu *pc)
{
}
int
db_validate_address(vm_offset_t addr)
{
	struct proc *p = curproc;
	struct pmap *pmap;

	if (!p || !p->p_vmspace || !p->p_vmspace->vm_map.pmap ||
#ifndef ARM32_NEW_VM_LAYOUT
	    addr >= VM_MAXUSER_ADDRESS
#else
	    addr >= VM_MIN_KERNEL_ADDRESS
#endif
	   )
		pmap = pmap_kernel();
	else
		pmap = p->p_vmspace->vm_map.pmap;

	return (pmap_extract(pmap, addr) == FALSE);
}

/*
 * Read bytes from kernel address space for debugger.
 */
void
db_read_bytes(addr, size, data)
	vm_offset_t	addr;
	size_t	size;
	char	*data;
{
	char	*src = (char *)addr;

	if (db_validate_address((u_int)src)) {
		db_printf("address %p is invalid\n", src);
		return;
	}

	if (size == 4 && (addr & 3) == 0 && ((uintptr_t)data & 3) == 0) {
		*((int*)data) = *((int*)src);
		return;
	}

	if (size == 2 && (addr & 1) == 0 && ((uintptr_t)data & 1) == 0) {
		*((short*)data) = *((short*)src);
		return;
	}

	while (size-- > 0) {
		if (db_validate_address((u_int)src)) {
			db_printf("address %p is invalid\n", src);
			return;
		}
		*data++ = *src++;
	}
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(vm_offset_t addr, size_t size, char *data)
{
	char *dst;
	size_t loop;

	/* If any part is in kernel text, use db_write_text() */
	if (addr >= (vm_offset_t) btext && addr < (vm_offset_t) etext) {
		return;
	}

	dst = (char *)addr;
	if (db_validate_address((u_int)dst)) {
		db_printf("address %p is invalid\n", dst);
		return;
	}

	if (size == 4 && (addr & 3) == 0 && ((uintptr_t)data & 3) == 0)
		*((int*)dst) = *((int*)data);
	else
	if (size == 2 && (addr & 1) == 0 && ((uintptr_t)data & 1) == 0)
		*((short*)dst) = *((short*)data);
	else {
		loop = size;
		while (loop-- > 0) {
			if (db_validate_address((u_int)dst)) {
				db_printf("address %p is invalid\n", dst);
				return;
			}
			*dst++ = *data++;
		}
	}

	/* make sure the caches and memory are in sync */
	cpu_icache_sync_range(addr, size);

	/* In case the current page tables have been modified ... */
	cpu_tlb_flushID();
	cpu_cpwait();
}

#ifdef DDB
void
Debugger(const char *msg)
{
	db_printf("Debugger(\"%s\")\n", msg);
	__asm(".word	0xe7ffffff");
}

int
db_trapper(u_int addr, u_int inst, trapframe_t *frame, int fault_code)
{

	if (fault_code == 0) {
		if ((inst & ~INSN_COND_MASK) == (BKPT_INST & ~INSN_COND_MASK))
			kdb_trap(T_BREAKPOINT, frame);
		else
			kdb_trap(-1, frame);
	} else
		return (1);
	return (0);
}

extern u_int end;

#endif

u_int
db_fetch_reg(int reg, db_regs_t *db_regs)
{

	switch (reg) {
	case 0:
		return (db_regs->tf_r0);
	case 1:
		return (db_regs->tf_r1);
	case 2:
		return (db_regs->tf_r2);
	case 3:
		return (db_regs->tf_r3);
	case 4:
		return (db_regs->tf_r4);
	case 5:
		return (db_regs->tf_r5);
	case 6:
		return (db_regs->tf_r6);
	case 7:
		return (db_regs->tf_r7);
	case 8:
		return (db_regs->tf_r8);
	case 9:
		return (db_regs->tf_r9);
	case 10:
		return (db_regs->tf_r10);
	case 11:
		return (db_regs->tf_r11);
	case 12:
		return (db_regs->tf_r12);
	case 13:
		return (db_regs->tf_svc_sp);
	case 14:
		return (db_regs->tf_svc_lr);
	case 15:
		return (db_regs->tf_pc);
	default:
		panic("db_fetch_reg: botch");
	}
}

