/*
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
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
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 *	from: Mach, Revision 2.2  92/04/04  11:34:26  rpd
 *	$Id: bios.S,v 1.12 1997/07/31 08:07:53 phk Exp $
 */

/*
  Copyright 1988, 1989, 1990, 1991, 1992 
   by Intel Corporation, Santa Clara, California.

                All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <machine/asm.h>

#define	addr32	.byte 0x67
#define	data32	.byte 0x66

/*
 *
 * get_diskinfo():  return a word that represents the
 *	max number of sectors and heads and drives for this device
 *
 */

ENTRY(bd_int13fn8)
	push	%ebp
	mov	%esp, %ebp
	push	%ebx
	push	%esi
	push	%edi

	movb	0x8(%ebp), %dl		/* diskinfo(drive #) */
	call	CNAME(prot_to_real)	/* enter real mode */

	movb	$0x8, %ah		/* ask for disk info */

	sti
	int	$0x13
	cli

	jnc	ok
	/* call failed, return 0 */
	subb	%cl, %cl
	subb	%ch, %ch
	subb	%dl, %dl
	subb	%dh, %dh
ok:

	data32
	call	CNAME(real_to_prot)	/* back to protected mode */

	/* 
	 * form a longword representing all this gunk:
	 *       6 bit zero
	 *	10 bit max cylinder (0 based)
	 *	 8 bit max head (0 based)
	 *	 2 bit zero
	 *	 6 bit max sector (1 based) = # sectors
	 */
	movb	%cl, %al		/* Upper two bits of cylinder count */
	andl	$0xc0,%eax	
	leal	0(,%eax,4),%eax		/* << 2 */
	movb	%ch, %al		/* Lower 8 bits */
	sall	$16,%eax		/* << 16 */
	movb	%dh, %ah		/* max head */
	andb	$0x3f, %cl		/* mask of cylinder gunk */
	movb	%cl, %al		/* max sector (and # sectors) */

	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret

