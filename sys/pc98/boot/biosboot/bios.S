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
 *	$Id: bios.S,v 1.1.1.1.2.3 1997/01/04 16:16:06 kato Exp $
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
/*
 * Ported to PC-9801 by Yoshio Kimura
 */

	.file	"bios.s"

#include "asm.h"
	.text

/*
 * biosread(dev, cyl, head, sec, nsec, offset)
 *	Read "nsec" sectors from disk to offset "offset" in boot segment
 * BIOS call "INT 0x1B Function 0xn6" to read sectors from disk into memory
 *	Call with	%ah = 0xd6(for floppy disk) or 0x06(for hard disk)
 *			%al = DA/UA
 *			%bx = data length
 *			%ch = sector size(for floppy) or cylinder(for hard)
 *			%cl = cylinder
 *			%dh = head
 *			%dl = sector
 *			%es:%bp = segment:offset of buffer
 *	Return:		
 *			%al = 0x0 on success; err code on failure
 */

ENTRY(biosread)
	push	%ebp
	mov	%esp, %ebp

	push	%ebx
	push	%esi
	push	%edi

	movb	0x14(%ebp), %dl		/* sector */
	movb	0x10(%ebp), %dh		/* head */
	movw	0x0c(%ebp), %cx		/* cylinder */
	movb	0x08(%ebp), %al		/* DA/UA */
	movb	$0x06, %ah
	andb	$0xf0, %al
	cmpb	$0x90, %al
	jnz	1f
	incb	%dl
	movb	$0x02, %ch
	movb	$0xd6, %ah
1:
	movb	0x08(%ebp), %al
	movl	%eax, %ebx

	/* prot_to_real will set %es to BOOTSEG */
	call	EXT(prot_to_real)	/* enter real mode */
	mov	%ebx, %eax
	xor	%ebx, %ebx
	addr32
	movb	0x18(%ebp), %bl		/* number of sectors */
	data32
	shl	$9, %ebx
	data32
	push	%ebx
	addr32
	data32
	mov	0x1c(%ebp), %ebx
	data32
	mov	%ebx, %ebp
	data32
	pop	%ebx

	int	$0x1b
	jc	1f
	xor	%eax, %eax
1:
	/* save return value (actually movw %ax, %bx) */
	mov	%eax, %ebx

	data32
	call	EXT(real_to_prot)	/* back to protected mode */

	xor	%eax, %eax
	movb	%bh, %al		/* return value in %ax */

	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp

	ret


/*
 * getc()
 * BIOS call "INT 18H Function 00H" to read character from keyboard
 *	Call with	%ah = 0x0
 *	Return:		%ah = keyboard scan code
 *			%al = ASCII character
 */

ENTRY(getc)
	push	%ebp
	mov	%esp, %ebp
	push	%ebx			/* save %ebx */
	push    %esi
	push    %edi

	call	EXT(prot_to_real)

	movb	$0x0, %ah
	int	$0x18

	movb	%al, %bl		/* real_to_prot uses %eax */

	data32
	call	EXT(real_to_prot)

	xor	%eax, %eax
	movb	%bl, %al

	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret
/*
 * ischar()
 *	if there is a character pending, return it; otherwise return 0
 * BIOS call "INT 18H Function 01H" to check whether a character is pending
 *	Call with	%ah = 0x1
 *	Return:
 *		If key waiting to be input:
 *			%ah = keyboard scan code
 *			%al = ASCII character
 *			%bh = 1
 *		else
 *			%bh = 0
 */
ENTRY(ischar)
	push	%ebp
	mov	%esp, %ebp
	push	%ebx
	push    %esi
	push    %edi

	call	EXT(prot_to_real)	/* enter real mode */

	xor	%ebx, %ebx
	movb	$0x1, %ah
	int	$0x18
	andb	%bh, %bh
	data32
	jz	nochar
	movb	%al, %bl

nochar:
	data32
	call	EXT(real_to_prot)

	xor	%eax, %eax
	movb	%bl, %al

	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret

/*
 *
 * get_diskinfo():  return a word that represents the
 *	max number of sectors and heads and drives for this device
 *
 */

ENTRY(get_diskinfo)
	push	%ebp
	mov	%esp, %ebp
	push	%ebx
	push	%esi
	push    %edi

	movb	0x8(%ebp), %dl		/* diskinfo(drive #) */
	call	EXT(prot_to_real)	/* enter real mode */

	movb	%dl, %al		/* ask for disk info */
	andb	$0xf0, %al
	cmpb	$0x90, %al
	jz	fdd

	movb	%dl, %al
	movb	$0x84, %ah

	int	$0x1b

	jnc	ok
	/*
	 * Urk.  Call failed.  It is not supported for floppies by old BIOS's.
	 * Guess it's a 15-sector floppy.
	 */
fdd:
	subb	%ah, %ah		/* %ax = 0 */
	movb	%al, %al
	movb	%ah, %bh		/* %bh = 0 */
	movb	$2, %bl			/* %bl	bits 0-3 = drive type,
						bit    2 = 1.2M */
	movb	$79, %ch		/* max track */
	movb	$1, %cl			/* # floppy drives installed */
	movb	$2, %dh			/* max head */
	movb	$15, %dl		/* max sector */
	/* es:di = parameter table */
	/* carry = 0 */
ok:

	data32
	call	EXT(real_to_prot)	/* back to protected mode */

	/* 
	 * form a longword representing all this gunk:
	 *	16 bit cylinder
	 *	 8 bit head
	 *	 8 bit sector
	 */
	mov	%ecx, %eax
	sall	$16,%eax		/* << 16 */
	movb	%dh, %ah		/* max head */
	movb	%dl, %al		/* max sector (and # sectors) */

	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret

/*
 *
 * memsize(i) :  return the memory size in KB. i == 0 for conventional memory,
 *		i == 1 for extended memory
 *		Both have the return value in AX.
 *
 */

ENTRY(memsize)
	push	%ebp
	mov	%esp, %ebp
	push	%ebx
	push    %esi
	push    %edi

	mov	8(%ebp), %ebx

	xor	%eax, %eax
	cmpb	$0x01, %bl
	jnz	memcnv
memext:
	movb	0xA1401 - BOOTSEG * 0x10, %al
	shll	$7, %eax
	xorl	%ebx, %ebx
	movw	0xA1594 - BOOTSEG * 0x10, %bx
	shll	$10, %ebx
	addl	%ebx, %eax
	jmp	xdone

memcnv:
	movb	0xA1501 - BOOTSEG * 0x10, %al
	andb	$0x07, %al
	incl	%eax
	shll	$7, %eax

xdone:
	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret
