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
 *	from: Mach, Revision 2.2  92/04/04  11:36:29  rpd
 *	$Id: start.S,v 1.6 1995/09/16 13:51:20 bde Exp $
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

#include	"asm.h"

	.file	"start.S"

SIGNATURE=	0xaa55
LOADSZ=		8192	/* size of unix boot */

	.text	
	.globl	start

ENTRY(boot1)
	jmp	start

boot_cyl:
	.word	0
	String	"IPL1   "

start:
	/* set up %ds */
	xor	%ax, %ax
	mov	%ax, %ds

	/* set up %ss and %esp */
	data32
	mov	$BOOTSEG, %eax
	mov	%ax, %ss
	data32
	mov	$BOOTSTACK, %esp

	/* set up %es, (where we will load boot2 to) */
	mov	%ax, %es

	push	%es
	push	%cx
	push	%dx

	data32
	mov	$0xa000, %eax
	mov	%ax, %es

	addr32
	movb	0x501, %al
	testb	$0x08, %al
	jnz	hireso
normal:
	/* set up graphic screen */
	movb	$0x42, %ah
	movb	$0xc0, %ch
	int	$0x18
	movb	$0x40, %ah
	int	$0x18

	data32
	mov	$0x0a00, %eax		/* 80 x 25 mode  */
	jmp	1f
hireso:
	movb	$0x08, %al		/* set up RAM window */
	outb	%al, $0x91
	movb	$0x0a, %al
	outb	%al, $0x93
	data32
	mov	$0x0a10, %ax		/* 80 x 31 mode */
1:
	int	$0x18
	movb	$0x0c, %ah		/* text on */
	int	$0x18

	/* cursor home and on */
	xor	%edx, %edx
	movb	$0x13, %ah
	int	$0x18
	movb	$0x11, %ah
	int	$0x18

	/* highreso no supported */
	addr32
	movb	0x501, %al
	testb	$0x08, %al
	jz	nothireso

	data32
	mov	$ehireso, %esi
	data32
	call	message
	hlt

nothireso:
	/* keyboad reset */
	movb	$0x03, %ah
	int	$0x18

	/* transfer PC-9801 system common area to 0xa1000 */
	data32
	mov	$0x0000, %esi
	data32
	mov	$0x1000, %edi
	data32
	mov	$0x0630, %ecx
	cld
	rep
	movsb

	/* transfer EPSON machine type to 0xa1200 */
	push	%ds
	data32
	mov	$0xfd00, %eax
	mov	%ax, %ds
	addr32
	data32
	mov	0x804, %eax
	data32
	and	$0x00ffffff, %eax
	addr32
	data32
	.byte	0x26
	mov	%eax, %es: (0x1624)

	pop	%ds
	pop	%dx
	pop	%cx
	pop	%es

	/* bootstrap passes */
	mov	%cs, %bx
	data32
	cmp	$0x1fc0, %ebx
	jnz	hd
	data32
	mov	%ebp, %ecx
	data32
	mov	%ebp, %edx
	addr32
	movb	0x584, %al
	andb	$0xf0, %al
	cmpb	$0x90, %al
	jnz	hd
fd:
	data32
	mov	$0x0200, %ecx
	data32
	mov	$0x0001, %edx
	movb	$0xd6, %ah
	jmp	load
hd:
	data32
	and	%ecx, %ecx
	jnz	1f
	addr32
	data32
	mov	%cs: (boot_cyl), %ecx
1:
	movb	$0x06, %ah	

/*
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
 *			%ah = 0x0 on success; err code on failure
 */

load:
	data32
	mov	$LOADSZ, %ebx
	addr32
	movb	0x584, %al
	xor	%ebp, %ebp	/* %bp = 0, put it at 0 in the BOOTSEG */
	int	$0x1b
	jc	read_error

	/*
	 * ljmp to the second stage boot loader (boot2).
	 * After ljmp, %cs is BOOTSEG and boot1 (512 bytes) will be used
	 * as an internal buffer "intbuf".
	 */

	data32
	ljmp	$BOOTSEG, $ EXT(boot2)

/*
 * read_error
 */
read_error:
	data32
	mov	$eread, %esi
err_stop:
	data32
	call	message
	data32
	jmp	stop

/*
 * message: write the error message in %ds:%esi to console
 */
message:

	data32
	push	%eax
	data32
	push	%ebx
	push	%ds
	push	%es
	data32
	mov	$0xe000, %eax
	mov	%ax, %es
	addr32
	mov	0x501, %al
	testb	$0x08, %al
	jnz	1f
	data32
	mov	$0xa000, %eax
	mov	%ax, %es
1:
	mov	%cs, %ax
	mov	%ax, %ds
	addr32
	data32
	mov	vram, %edi
	data32
	mov	$0x00e1, %ebx
	cld

nextb:
	lodsb			/* load a byte into %al */
	cmpb	$0x0, %al
	je	done
	cmpb	$0x0d, %al
	je	cr_code
	cmpb	$0x0a, %al
	je	lf_code
	addr32
	movb	%al, (%edi)
	addr32
	movb	%bl, 0x2000(%edi)
	data32
	inc	%edi
	data32
	inc	%edi
	jmp	nextb
cr_code:
	data32
	add	$80, %edi
	jmp	nextb
lf_code:
	data32
	mov	%edi, %eax
	data32
	mov	$80, %edx
	data32
	div	%ebx
	data32
	sub	%ebx, %edi
	jmp	nextb
done:
	addr32
	data32
	mov	%edi, vram
	pop	%es
	pop	%ds
	data32
	pop	%ebx
	data32
	pop	%eax
	data32
	ret

stop:	hlt
	data32
	jmp	stop		/* halt doesnt actually halt forever */

vram:
	.long	0

/* error messages */

#ifdef	DEBUG
one:	String		"1\r\n\0"
two:	String		"2\r\n\0"
three:	String		"3\r\n\0"
four:	String		"4\r\n\0"
five:	String		"5\r\n\0"
six:	String		"6\r\n\0"
seven:	String		"7\r\n\0"
#endif	DEBUG
eread:	String		"Read error\r\n\0"
enoboot: String		"No bootable partition\r\n\0"
endofcode:
ehireso: String		"Highreso not supported\r\n\0"
/* the last 2 bytes in the sector 0 contain the signature */
	. = EXT(boot1) + 0x1fe
	.value	SIGNATURE
ENTRY(disklabel)
	. = EXT(boot1) + 0x400	
