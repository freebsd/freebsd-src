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
 *	$Id: bios.S,v 1.3 1994/10/02 05:18:25 rgrimes Exp $
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
 * Serial bootblock interface routines
 * Copyright (c) 1994, J"org Wunsch
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE AUTHOR ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  THE AUTHOR DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */ 

	.file	"bios.s"

#include "asm.h"
	.text

/*
 * biosread(dev, cyl, head, sec, nsec, offset)
 *	Read "nsec" sectors from disk to offset "offset" in boot segment
 * BIOS call "INT 0x13 Function 0x2" to read sectors from disk into memory
 *	Call with	%ah = 0x2
 *			%al = number of sectors
 *			%ch = cylinder
 *			%cl = sector
 *			%dh = head
 *			%dl = drive (0x80 for hard disk, 0x0 for floppy disk)
 *			%es:%bx = segment:offset of buffer
 *	Return:		
 *			%al = 0x0 on success; err code on failure
 */

ENTRY(biosread)
	push	%ebp
	mov	%esp, %ebp

	push	%ebx
	push	%ecx
	push	%edx
	push	%es

	movb	0x10(%ebp), %dh
	movw	0x0c(%ebp), %cx
	/* cylinder; the highest 2 bits of cyl is in %cl */
	xchgb	%ch, %cl
	rorb	$2, %cl
	movb	0x14(%ebp), %al
	orb	%al, %cl
	incb	%cl			/* sector; sec starts from 1, not 0 */
	movb	0x8(%ebp), %dl		/* device */
	movl	0x1c(%ebp), %ebx	/* offset */

	/* prot_to_real will set %es to BOOTSEG */
	call	EXT(prot_to_real)	/* enter real mode */
	movb	$0x2, %ah		/* subfunction */
	addr32
	movb	0x18(%ebp), %al		/* number of sectors */

	sti
	int	$0x13
	cli

	/* save return value (actually movw %ax, %bx) */
	mov	%eax, %ebx

	data32
	call	EXT(real_to_prot)	/* back to protected mode */

	xor	%eax, %eax
	movb	%bh, %al		/* return value in %ax */

	pop	%es
	pop	%edx
	pop	%ecx
	pop	%ebx
	pop	%ebp

	ret

/*
 * The serial port interface routines implement a simple polled i/o
 * interface to a standard serial port.  Due to the space restrictions
 * for the boot blocks, no BIOS support is used (since BIOS requires
 * expensive real/protected mode switches), instead the rudimentary
 * BIOS support is duplicated here.
 *
 * The base address for the i/o port is passed from the Makefile in
 * the COMCONSOLE preprocessor macro.  Console parameters are currently
 * hard-coded to 9600 Bd, 8 bit.  This can be changed in the
 * init_serial() function.
 */

/*
 * void putc(char ch)
 *	send ch to serial port
 *
 */

ENTRY(putc)
	push	%ebp
	mov	%esp, %ebp
	push	%edx

	mov	$COMCONSOLE + 5, %edx	# line status reg
1:	inb	%dx, %al
	test	$0x20, %al
	jz	1b		# TX buffer not empty

	movb	0x8(%ebp), %al

	sub	$5, %edx	# TX output reg
	outb	%al, %dx	# send this one

	pop	%edx
	pop	%ebp
	ret

/*
 * int getc(void)
 *	read a character from serial port
 */

ENTRY(getc)
	push	%ebp
	mov	%esp, %ebp
	push	%edx

	mov	$COMCONSOLE + 5, %edx	# line status reg
1:
	inb	%dx, %al
	testb	$0x01, %al
	jz	1b		# no RX char available

	xor	%eax, %eax
	sub	$5, %edx	# RX buffer reg
	inb	%dx, %al	# fetch (first) character

	cmp	$0x7F, %eax	# make DEL...
	jne	2f
	mov	$0x08, %eax	# look like BS
2:
	pop	%edx
	pop	%ebp
	ret

/*
 * int ischar(void)
 *       if there is a character pending, return true; otherwise return 0
 */
ENTRY(ischar)
	push	%ebp
	mov	%esp, %ebp
	push	%edx

	xorl	%eax, %eax
	mov	$COMCONSOLE + 5, %edx	# line status reg
	inb	%dx, %al
	andb	$0x01, %al		# RX char available?

	pop	%edx
	pop	%ebp
	ret

/*
 * void init_serial(void)
 * 	initialize the serial console port to 9600 Bd, 8 bpc
 */
ENTRY(init_serial)
	push	%ebp
	mov	%esp, %ebp
	push	%edx

	mov	$COMCONSOLE + 3, %edx	# line control reg
	movb	$0x80, %al
	outb	%al, %dx	# enable DLAB

	sub	$3, %edx	# divisor latch, low byte
	mov	$12, %ax	# divisor 12: 9600 Bd
	outb	%al, %dx
	inc	%edx		# divisor latch, high byte
	movb	%ah, %al
	outb	%al, %dx

	add	$2, %edx	# line control reg
	movb	$0x13, %al
	outb	%al, %dx	# 8 bit, no parity, 1 stop bit

	inc	%edx		# modem control reg
	mov	$3, %al
	outb	%al, %dx	# enable DTR/RTS

	/* now finally, flush the input buffer */
	inc	%edx		# line status reg
1:
	inb	%dx, %al
	testb	$0x01, %al
	jz	2f		# no more characters buffered
	sub	$5, %edx	# rx buffer reg
	inb	%dx, %al	# throw away
	add	$5, %edx
	jmp	1b
2:
	pop	%edx
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
	push	%es
	push	%ebx
	push	%ecx
	push	%edx

	movb	0x8(%ebp), %dl		/* diskinfo(drive #) */
	call	EXT(prot_to_real)	/* enter real mode */

	movb	$0x8, %ah		/* ask for disk info */

	sti
	int	$0x13
	cli

	jnc	ok
	/*
	 * Urk.  Call failed.  It is not supported for floppies by old BIOS's.
	 * Guess it's a 15-sector floppy.  Initialize all the registers for
	 * documentation, although we only need head and sector counts.
	 */
	subb	%ah, %ah		/* %ax = 0 */
	movb	%al, %al
	movb	%ah, %bh		/* %bh = 0 */
	movb	$2, %bl			/* %bl	bits 0-3 = drive type,
						bit    2 = 1.2M */
	movb	$79, %ch		/* max track */
	movb	$15, %cl		/* max sector */
	movb	$1, %dh			/* max head */
	movb	$1, %dl			/* # floppy drives installed */
	/* es:di = parameter table */
	/* carry = 0 */
ok:

	data32
	call	EXT(real_to_prot)	/* back to protected mode */

	xor	%eax, %eax

	/* form a longword representing all this gunk */
	movb	%dh, %ah		/* max head */
	andb	$0x3f, %cl		/* mask of cylinder gunk */
	movb	%cl, %al		/* max sector (and # sectors) */

	pop	%edx
	pop	%ecx
	pop	%ebx
	pop	%es
	pop	%ebp
	ret

/*
 *
 * memsize(i) :  return the memory size in KB. i == 0 for conventional memory,
 *		i == 1 for extended memory
 *	BIOS call "INT 12H" to get conventional memory size
 *	BIOS call "INT 15H, AH=88H" to get extended memory size
 *		Both have the return value in AX.
 *
 */

ENTRY(memsize)
	push	%ebp
	mov	%esp, %ebp
	push	%ebx

	mov	8(%ebp), %ebx

	call	EXT(prot_to_real)	/* enter real mode */

	cmpb	$0x1, %bl
	data32
	je	xext
	
	sti
	int	$0x12
	cli
	data32
	jmp	xdone

xext:	movb	$0x88, %ah
	sti
	int	$0x15
	cli

xdone:
	mov	%eax, %ebx

	data32
	call	EXT(real_to_prot)

	mov	%ebx, %eax
	pop	%ebx
	pop	%ebp
	ret

