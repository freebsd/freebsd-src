/* $FreeBSD: src/sys/alpha/include/bootinfo.h,v 1.5 1999/08/28 00:38:40 peter Exp $ */
/*
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

struct bootinfo_v1 {
	u_long	ssym;			/* 0: start of kernel sym table	*/
	u_long	esym;			/* 8: end of kernel sym table	*/
	char	boot_flags[64];		/* 16: boot flags		*/
	char	booted_kernel[64];	/* 80: name of booted kernel	*/
	void	*hwrpb;			/* 144: hwrpb pointer (BEVA)	*/
	u_long	hwrpbsize;		/* 152: size of hwrpb data	*/
	int	(*cngetc) __P((void));	/* 160: console getc pointer	*/
	void	(*cnputc) __P((int));	/* 168: console putc pointer	*/
	void	(*cnpollc) __P((int));	/* 176: console pollc pointer	*/
	u_long	pad[6];			/* 184: rsvd for future use	*/
	char	*envp;			/* 232:	start of environment	*/
	u_long	kernend;		/* 240: end of kernel		*/
	u_long	modptr;			/* 248: FreeBSD module base	*/
					/* 256: total size		*/
};

/*
 * Kernel-internal structure used to hold important bits of boot
 * information.  NOT to be used by boot blocks.
 *
 * Note that not all of the fields from the bootinfo struct(s)
 * passed by the boot blocks aren't here (because they're not currently
 * used by the kernel!).  Fields here which aren't supplied by the
 * bootinfo structure passed by the boot blocks are supposed to be
 * filled in at startup with sane contents.
 */
struct bootinfo_kernel {
	u_long	ssym;			/* start of syms */
	u_long	esym;			/* end of syms */
	u_long	modptr;			/* FreeBSD module pointer */
	u_long	kernend;		/* "end of kernel" from boot code */
	char	*envp;			/* "end of kernel" from boot code */
	u_long	hwrpb_phys;		/* hwrpb physical address */
	u_long	hwrpb_size;		/* size of hwrpb data */
	char	boot_flags[64];		/* boot flags */
	char	booted_kernel[64];	/* name of booted kernel */
	char	booted_dev[64];		/* name of booted device */
};

extern struct bootinfo_kernel bootinfo;
