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
 *	$Id: db_interface.c,v 1.3 1993/11/07 17:41:34 wollman Exp $
 */

/*
 * Interface to new debugger.
 */
#include "param.h"
#include "systm.h"
#include "proc.h"
#include "ddb/ddb.h"

#include <sys/reboot.h>
#include <vm/vm_statistics.h>
#include <vm/pmap.h>

#include <setjmp.h>

int	db_active = 0;

db_regs_t ddb_regs;

/*
 * Received keyboard interrupt sequence.
 */
void
kdb_kbd_trap(regs)
	struct i386_saved_state *regs;
{
	if (db_active == 0 && (boothowto & RB_KDB)) {
	    printf("\n\nkernel: keyboard interrupt\n");
	    kdb_trap(-1, 0, regs);
	}
}

/*
 *  kdb_trap - field a TRACE or BPT trap
 */

static jmp_buf *db_nofault = 0;

int
kdb_trap(type, code, regs)
	int	type, code;
	register struct i386_saved_state *regs;
{
#if 0
	if ((boothowto&RB_KDB) == 0)
	    return(0);
#endif

	switch (type) {
	    case T_BPTFLT /* T_INT3 */:	/* breakpoint */
	    case T_KDBTRAP /* T_WATCHPOINT */:	/* watchpoint */
	    case T_PRIVINFLT /* T_DEBUG */:	/* single_step */

	    case -1:	/* keyboard interrupt */
		break;

	    default:
		kdbprinttrap(type, code);

		if (db_nofault) {
		    jmp_buf *no_fault = db_nofault;
		    db_nofault = 0;
		    longjmp(*no_fault, 1);
		}
	}

	/*  Should switch to kdb`s own stack here. */

	ddb_regs = *regs;

	if ((regs->tf_cs & 0x3) == 0) {
	    /*
	     * Kernel mode - esp and ss not saved
	     */
	    ddb_regs.tf_esp = (int)&regs->tf_esp;	/* kernel stack pointer */
#if 0
	    ddb_regs.ss   = KERNEL_DS;
#endif
	    asm(" movw %%ss,%%ax; movl %%eax,%0 " 
		: "=g" (ddb_regs.tf_ss) 
		:
		: "ax");
	}

	db_active++;
	cnpollc(TRUE);
	db_trap(type, code);
	cnpollc(FALSE);
	db_active--;

	regs->tf_eip    = ddb_regs.tf_eip;
	regs->tf_eflags = ddb_regs.tf_eflags;
	regs->tf_eax    = ddb_regs.tf_eax;
	regs->tf_ecx    = ddb_regs.tf_ecx;
	regs->tf_edx    = ddb_regs.tf_edx;
	regs->tf_ebx    = ddb_regs.tf_ebx;
	if (regs->tf_cs & 0x3) {
	    /*
	     * user mode - saved esp and ss valid
	     */
	    regs->tf_esp = ddb_regs.tf_esp;		/* user stack pointer */
	    regs->tf_ss  = ddb_regs.tf_ss & 0xffff;	/* user stack segment */
	}
	regs->tf_ebp    = ddb_regs.tf_ebp;
	regs->tf_esi    = ddb_regs.tf_esi;
	regs->tf_edi    = ddb_regs.tf_edi;
	regs->tf_es     = ddb_regs.tf_es & 0xffff;
	regs->tf_cs     = ddb_regs.tf_cs & 0xffff;
	regs->tf_ds     = ddb_regs.tf_ds & 0xffff;
#if 0
	regs->tf_fs     = ddb_regs.tf_fs & 0xffff;
	regs->tf_gs     = ddb_regs.tf_gs & 0xffff;
#endif

	return (1);
}

/*
 * Print trap reason.
 */
void
kdbprinttrap(type, code)
	int	type, code;
{
	printf("kernel: ");
	printf("type %d", type);
	printf(" trap, code=%x\n", code);
}

/*
 * Read bytes from kernel address space for debugger.
 */

extern jmp_buf	db_jmpbuf;

void
db_read_bytes(addr, size, data)
	vm_offset_t	addr;
	register int	size;
	register char	*data;
{
	register char	*src;

	db_nofault = &db_jmpbuf;

	src = (char *)addr;
	while (--size >= 0)
	    *data++ = *src++;

	db_nofault = 0;
}

struct pte *pmap_pte(pmap_t, vm_offset_t);

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(addr, size, data)
	vm_offset_t	addr;
	register int	size;
	register char	*data;
{
	register char	*dst;

	register pt_entry_t *ptep0 = 0;
	pt_entry_t	oldmap0 = { 0 };
	vm_offset_t	addr1;
	register pt_entry_t *ptep1 = 0;
	pt_entry_t	oldmap1 = { 0 };
	extern char	etext;

	db_nofault = &db_jmpbuf;

	if (addr >= VM_MIN_KERNEL_ADDRESS &&
	    addr <= (vm_offset_t)&etext)
	{
	    ptep0 = pmap_pte(kernel_pmap, addr);
	    oldmap0 = *ptep0;
	    *(int *)ptep0 |= /* INTEL_PTE_WRITE */ PG_RW;

	    addr1 = i386_trunc_page(addr + size - 1);
	    if (i386_trunc_page(addr) != addr1) {
		/* data crosses a page boundary */

		ptep1 = pmap_pte(kernel_pmap, addr1);
		oldmap1 = *ptep1;
		*(int *)ptep1 |= /* INTEL_PTE_WRITE */ PG_RW;
	    }
	    tlbflush();
	}

	dst = (char *)addr;

	while (--size >= 0)
	    *dst++ = *data++;

	db_nofault = 0;

	if (ptep0) {
	    *ptep0 = oldmap0;
	    if (ptep1) {
		*ptep1 = oldmap1;
	    }
	    tlbflush();
	}
}

void
Debugger (msg)
	char *msg;
{
	asm ("int $3");
}
