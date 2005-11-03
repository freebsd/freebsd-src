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
__FBSDID("$FreeBSD: src/sys/amd64/amd64/db_interface.c,v 1.81 2005/01/05 20:17:20 imp Exp $");

/*
 * Interface to new debugger.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/cons.h>
#include <sys/pcpu.h>
#include <sys/proc.h>

#include <machine/cpu.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <ddb/ddb.h>

/*
 * Read bytes from kernel address space for debugger.
 */
int
db_read_bytes(vm_offset_t addr, size_t size, char *data)
{
	jmp_buf jb;
	void *prev_jb;
	char *src;
	int ret;

	prev_jb = kdb_jmpbuf(jb);
	ret = setjmp(jb);
	if (ret == 0) {
		src = (char *)addr;
		while (size-- > 0)
			*data++ = *src++;
	}
	(void)kdb_jmpbuf(prev_jb);
	return (ret);
}

/*
 * Write bytes to kernel address space for debugger.
 */
int
db_write_bytes(vm_offset_t addr, size_t size, char *data)
{
	jmp_buf jb;
	void *prev_jb;
	char *dst;
	pt_entry_t	*ptep0 = NULL;
	pt_entry_t	oldmap0 = 0;
	vm_offset_t	addr1;
	pt_entry_t	*ptep1 = NULL;
	pt_entry_t	oldmap1 = 0;
	int ret;

	prev_jb = kdb_jmpbuf(jb);
	ret = setjmp(jb);
	if (ret == 0) {
		if (addr > trunc_page((vm_offset_t)btext) - size &&
		    addr < round_page((vm_offset_t)etext)) {

			ptep0 = vtopte(addr);
			oldmap0 = *ptep0;
			*ptep0 |= PG_RW;

			/*
			 * Map another page if the data crosses a page
			 * boundary.
			 */
			if ((*ptep0 & PG_PS) == 0) {
				addr1 = trunc_page(addr + size - 1);
				if (trunc_page(addr) != addr1) {
					ptep1 = vtopte(addr1);
					oldmap1 = *ptep1;
					*ptep1 |= PG_RW;
				}
			} else {
				addr1 = trunc_2mpage(addr + size - 1);
				if (trunc_2mpage(addr) != addr1) {
					ptep1 = vtopte(addr1);
					oldmap1 = *ptep1;
					*ptep1 |= PG_RW;
				}
			}

			invltlb();
		}

		dst = (char *)addr;

		while (size-- > 0)
			*dst++ = *data++;
	}

	(void)kdb_jmpbuf(prev_jb);

	if (ptep0) {
		*ptep0 = oldmap0;

		if (ptep1)
			*ptep1 = oldmap1;

		invltlb();
	}

	return (ret);
}

void
db_show_mdpcpu(struct pcpu *pc)
{

#if 0
	db_printf("currentldt   = 0x%x\n", pc->pc_currentldt);
#endif
}
