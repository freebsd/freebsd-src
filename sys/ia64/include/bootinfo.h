/* $FreeBSD$ */
/*-
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
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

/*
 * The boot program passes a pointer (in the boot environment virtual
 * address address space; "BEVA") to a bootinfo to the kernel using
 * the following convention:
 *
 *	a0 contains first free page frame number
 *	a1 contains page number of current level 1 page table
 *	if a2 contains BOOTINFO_MAGIC and a4 is nonzero:
 *		a3 contains pointer (BEVA) to bootinfo
 *		a4 contains bootinfo version number
 *	if a2 contains BOOTINFO_MAGIC and a4 contains 0 (backward compat):
 *		a3 contains pointer (BEVA) to bootinfo version
 *		    (u_long), then the bootinfo
 */

#define	BOOTINFO_MAGIC			0xdeadbeeffeedface

struct bootinfo {
	u_int64_t	bi_magic;		/* BOOTINFO_MAGIC */
	u_int64_t	bi_version;		/* version 1 */
	u_int64_t	bi_spare[6];		/* was: name of booted kernel */
	u_int64_t	bi_hcdp;		/* DIG64 HCDP table */
	u_int64_t	bi_fpswa;		/* FPSWA interface */
	u_int64_t	bi_boothowto;		/* value for boothowto */
	u_int64_t	bi_systab;		/* pa of EFI system table */
	u_int64_t	bi_memmap;		/* pa of EFI memory map */
	u_int64_t	bi_memmap_size;		/* size of EFI memory map */
	u_int64_t	bi_memdesc_size;	/* sizeof EFI memory desc */
	u_int32_t	bi_memdesc_version;	/* EFI memory desc version */
	u_int64_t	bi_symtab;		/* start of kernel sym table */
	u_int64_t	bi_esymtab;		/* end of kernel sym table */
	u_int64_t	bi_kernend;		/* end of kernel space */
	u_int64_t	bi_envp;		/* environment */
	u_int64_t	bi_modulep;		/* preloaded modules */
};

extern struct bootinfo bootinfo;
