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
 *	$Id: db_interface.c,v 1.16 1995/12/07 12:45:29 davidg Exp $
 */

/*
 * Interface to new debugger.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/reboot.h>

#include <machine/md_var.h>
#include <machine/segments.h>

#include <machine/cons.h>	/* XXX: import cons_unavail */

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <ddb/ddb.h>
#include <setjmp.h>

static int	db_active = 0;

db_regs_t ddb_regs;

static void kdbprinttrap __P((int type, int code));

#if 0
/*
 * Received keyboard interrupt sequence.
 */
void
kdb_kbd_trap(regs)
	struct i386_saved_state *regs;
{
	if (db_active == 0 && (boothowto & RB_KDB)) {
	    db_printf("\n\nkernel: keyboard interrupt\n");
	    kdb_trap(-1, 0, regs);
	}
}
#endif

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

	/*
	 * XXX try to do nothing if the console is in graphics mode.
	 * Handle trace traps (and hardware breakpoints...) by ignoring
	 * them except for forgetting about them.  Return 0 for other
	 * traps to say that we haven't done anything.  The trap handler
	 * will usually panic.  We should handle breakpoint traps for
	 * our breakpoints by disarming our breakpoints and fixing up
	 * %eip.
	 */
	if (cons_unavail) {
		if (type == T_TRCTRAP) {
			regs->tf_eflags &= ~PSL_T;
			return (1);
		}
		return (0);
	}

	switch (type) {
	    case T_BPTFLT:	/* breakpoint */
	    case T_TRCTRAP:	/* debug exception */
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

	if (ISPL(regs->tf_cs) == 0) {
	    /*
	     * Kernel mode - esp and ss not saved
	     */
	    ddb_regs.tf_esp = (int)&regs->tf_esp;	/* kernel stack pointer */
#ifdef __GNUC__
#define	rss() ({u_short ss; __asm __volatile("movl %%ss,%0" : "=r" (ss)); ss;})
#endif
	    ddb_regs.tf_ss = rss();
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
	if (ISPL(regs->tf_cs)) {
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
static void
kdbprinttrap(type, code)
	int	type, code;
{
	db_printf("kernel: ");
	db_printf("type %d", type);
	db_printf(" trap, code=%x\n", code);
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
	    pmap_update();
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
	    pmap_update();
	}
}

/*
 * XXX move this to machdep.c and allow it to be called iff any debugger is
 * installed.
 */
void
Debugger(msg)
	const char *msg;
{
	static volatile u_char in_Debugger;

	/*
	 * XXX do nothing if the console is in graphics mode.  This is
	 * OK if the call is for the debugger hotkey but not if the call
	 * is a weak form of panicing.
	 */
	if (cons_unavail)
		return;

	if (!in_Debugger) {
		in_Debugger = 1;
		db_printf("Debugger(\"%s\")\n", msg);
#ifdef __GNUC__
		__asm __volatile("int $3");
#else
		int3();
#endif
		in_Debugger = 0;
	}
}
