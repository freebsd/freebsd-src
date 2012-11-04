/*	$NetBSD: db_interface.c,v 1.33 2003/08/25 04:51:10 mrg Exp $	*/

/*-
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
#ifdef KDB
#include <sys/kdb.h>
#endif

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <machine/db_machdep.h>
#include <machine/vmparam.h>
#include <machine/cpu.h>

#include <ddb/ddb.h>
#include <ddb/db_access.h>
#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_variables.h>
#include <ddb/db_sym.h>
#include <sys/cons.h>

static int nil = 0;

int db_access_und_sp (struct db_variable *, db_expr_t *, int);
int db_access_abt_sp (struct db_variable *, db_expr_t *, int);
int db_access_irq_sp (struct db_variable *, db_expr_t *, int);

static db_varfcn_t db_frame;

#define DB_OFFSET(x)	(db_expr_t *)offsetof(struct trapframe, x)
struct db_variable db_regs[] = {
	{ "spsr", DB_OFFSET(tf_spsr),	db_frame },
	{ "r0", DB_OFFSET(tf_r0),	db_frame },
	{ "r1", DB_OFFSET(tf_r1),	db_frame },
	{ "r2", DB_OFFSET(tf_r2),	db_frame },
	{ "r3", DB_OFFSET(tf_r3),	db_frame },
	{ "r4", DB_OFFSET(tf_r4),	db_frame },
	{ "r5", DB_OFFSET(tf_r5),	db_frame },
	{ "r6", DB_OFFSET(tf_r6),	db_frame },
	{ "r7", DB_OFFSET(tf_r7),	db_frame },
	{ "r8", DB_OFFSET(tf_r8),	db_frame },
	{ "r9", DB_OFFSET(tf_r9),	db_frame },
	{ "r10", DB_OFFSET(tf_r10),	db_frame },
	{ "r11", DB_OFFSET(tf_r11),	db_frame },
	{ "r12", DB_OFFSET(tf_r12),	db_frame },
	{ "usr_sp", DB_OFFSET(tf_usr_sp), db_frame },
	{ "usr_lr", DB_OFFSET(tf_usr_lr), db_frame },
	{ "svc_sp", DB_OFFSET(tf_svc_sp), db_frame },
	{ "svc_lr", DB_OFFSET(tf_svc_lr), db_frame },
	{ "pc", DB_OFFSET(tf_pc), 	db_frame },
	{ "und_sp", &nil, db_access_und_sp, },
	{ "abt_sp", &nil, db_access_abt_sp, },
	{ "irq_sp", &nil, db_access_irq_sp, },
};

struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

int
db_access_und_sp(struct db_variable *vp, db_expr_t *valp, int rw)
{

	if (rw == DB_VAR_GET) {
		*valp = get_stackptr(PSR_UND32_MODE);
		return (1);
	}
	return (0);
}

int
db_access_abt_sp(struct db_variable *vp, db_expr_t *valp, int rw)
{

	if (rw == DB_VAR_GET) {
		*valp = get_stackptr(PSR_ABT32_MODE);
		return (1);
	}
	return (0);
}

int
db_access_irq_sp(struct db_variable *vp, db_expr_t *valp, int rw)
{

	if (rw == DB_VAR_GET) {
		*valp = get_stackptr(PSR_IRQ32_MODE);
		return (1);
	}
	return (0);
}

int db_frame(struct db_variable *vp, db_expr_t *valp, int rw)
{
	int *reg;

	if (kdb_frame == NULL)
		return (0);

	reg = (int *)((uintptr_t)kdb_frame + (db_expr_t)vp->valuep);
	if (rw == DB_VAR_GET)
		*valp = *reg;
	else
		*reg = *valp;
	return (1);
}

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
int
db_read_bytes(addr, size, data)
	vm_offset_t	addr;
	size_t	size;
	char	*data;
{
	char	*src = (char *)addr;

	if (db_validate_address((u_int)src)) {
		db_printf("address %p is invalid\n", src);
		return (-1);
	}

	if (size == 4 && (addr & 3) == 0 && ((uintptr_t)data & 3) == 0) {
		*((int*)data) = *((int*)src);
		return (0);
	}

	if (size == 2 && (addr & 1) == 0 && ((uintptr_t)data & 1) == 0) {
		*((short*)data) = *((short*)src);
		return (0);
	}

	while (size-- > 0) {
		if (db_validate_address((u_int)src)) {
			db_printf("address %p is invalid\n", src);
			return (-1);
		}
		*data++ = *src++;
	}
	return (0);
}

/*
 * Write bytes to kernel address space for debugger.
 */
int
db_write_bytes(vm_offset_t addr, size_t size, char *data)
{
	char *dst;
	size_t loop;

	dst = (char *)addr;
	if (db_validate_address((u_int)dst)) {
		db_printf("address %p is invalid\n", dst);
		return (0);
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
				return (-1);
			}
			*dst++ = *data++;
		}
	}

	/* make sure the caches and memory are in sync */
	cpu_icache_sync_range(addr, size);

	/* In case the current page tables have been modified ... */
	cpu_tlb_flushID();
	cpu_cpwait();
	return (0);
}


static u_int
db_fetch_reg(int reg)
{

	switch (reg) {
	case 0:
		return (kdb_frame->tf_r0);
	case 1:
		return (kdb_frame->tf_r1);
	case 2:
		return (kdb_frame->tf_r2);
	case 3:
		return (kdb_frame->tf_r3);
	case 4:
		return (kdb_frame->tf_r4);
	case 5:
		return (kdb_frame->tf_r5);
	case 6:
		return (kdb_frame->tf_r6);
	case 7:
		return (kdb_frame->tf_r7);
	case 8:
		return (kdb_frame->tf_r8);
	case 9:
		return (kdb_frame->tf_r9);
	case 10:
		return (kdb_frame->tf_r10);
	case 11:
		return (kdb_frame->tf_r11);
	case 12:
		return (kdb_frame->tf_r12);
	case 13:
		return (kdb_frame->tf_svc_sp);
	case 14:
		return (kdb_frame->tf_svc_lr);
	case 15:
		return (kdb_frame->tf_pc);
	default:
		panic("db_fetch_reg: botch");
	}
}

u_int
branch_taken(u_int insn, db_addr_t pc)
{
	u_int addr, nregs, offset = 0;

	switch ((insn >> 24) & 0xf) {
	case 0x2:	/* add pc, reg1, #value */
	case 0x0:	/* add pc, reg1, reg2, lsl #offset */
		addr = db_fetch_reg((insn >> 16) & 0xf);
		if (((insn >> 16) & 0xf) == 15)
			addr += 8;
		if (insn & 0x0200000) {
			offset = (insn >> 7) & 0x1e;
			offset = (insn & 0xff) << (32 - offset) |
			    (insn & 0xff) >> offset;
		} else {

			offset = db_fetch_reg(insn & 0x0f);
			if ((insn & 0x0000ff0) != 0x00000000) {
				if (insn & 0x10)
					nregs = db_fetch_reg((insn >> 8) & 0xf);
				else
					nregs = (insn >> 7) & 0x1f;
				switch ((insn >> 5) & 3) {
				case 0:
					/* lsl */
					offset = offset << nregs;
					break;
				case 1:
					/* lsr */
					offset = offset >> nregs;
					break;
				default:
					break; /* XXX */
				}
					
			}
			return (addr + offset);
				
		}
		
	case 0xa:	/* b ... */
	case 0xb:	/* bl ... */
		addr = ((insn << 2) & 0x03ffffff);
		if (addr & 0x02000000)
			addr |= 0xfc000000;
		return (pc + 8 + addr);
	case 0x7:	/* ldr pc, [pc, reg, lsl #2] */
		addr = db_fetch_reg(insn & 0xf);
		addr = pc + 8 + (addr << 2);
		db_read_bytes(addr, 4, (char *)&addr);
		return (addr);
	case 0x1:	/* mov pc, reg */
		addr = db_fetch_reg(insn & 0xf);
		return (addr);
	case 0x4:
	case 0x5:	/* ldr pc, [reg] */
		addr = db_fetch_reg((insn >> 16) & 0xf);
		/* ldr pc, [reg, #offset] */
		if (insn & (1 << 24))
			offset = insn & 0xfff;
		if (insn & 0x00800000)
			addr += offset;
		else
			addr -= offset;
		db_read_bytes(addr, 4, (char *)&addr);
		return (addr);
	case 0x8:	/* ldmxx reg, {..., pc} */
	case 0x9:
		addr = db_fetch_reg((insn >> 16) & 0xf);
		nregs = (insn  & 0x5555) + ((insn  >> 1) & 0x5555);
		nregs = (nregs & 0x3333) + ((nregs >> 2) & 0x3333);
		nregs = (nregs + (nregs >> 4)) & 0x0f0f;
		nregs = (nregs + (nregs >> 8)) & 0x001f;
		switch ((insn >> 23) & 0x3) {
		case 0x0:	/* ldmda */
			addr = addr - 0;
			break;
		case 0x1:	/* ldmia */
			addr = addr + 0 + ((nregs - 1) << 2);
			break;
		case 0x2:	/* ldmdb */
			addr = addr - 4;
			break;
		case 0x3:	/* ldmib */
			addr = addr + 4 + ((nregs - 1) << 2);
			break;
		}
		db_read_bytes(addr, 4, (char *)&addr);
		return (addr);
	default:
		panic("branch_taken: botch");
	}
}
