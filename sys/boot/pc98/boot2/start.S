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
 * $FreeBSD$
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

NAMEBLOCKMAGIC=	0xfadefeed /* value of magicnumebr for block2 	*/

/*
 * This DEBUGMSG(msg) macro may be useful for debugging.  Its use is
 * restricted to this file since it only works in real mode.
 */
#define DEBUGMSG(msg)		\
	data32			; \
	mov	$msg, %esi	; \
	data32			; \
	call	message

	.code16
	.text	
	.globl	start

start:
	jmp	main
boot_cyl:
	.word	0
	String	"IPL1   "

main:
	/* set up %ds */
	xor	%ax, %ax
	mov	%ax, %ds

	/* set up %ss and %esp */
	mov	$BOOTSEG, %eax
	mov	%ax, %ss
	/*
	 * make a little room on the stack for
	 * us to save the default bootstring we might find..
	 * effectively, we push the bootstring.
	 */
	mov	$BOOTSTACK-64, %esp

	/* set up %es, (where we will load boot2 to) */
	mov	%ax, %es

	push	%es
	push	%ecx
	push	%dx

	mov	$0xa000, %eax
	mov	%ax, %es

	/* set up graphic screen */
	movb	$0x42, %ah
	movb	$0xc0, %ch
	int	$0x18
	movb	$0x40, %ah
	int	$0x18

	mov	$0x0a00, %eax		/* 80 x 25 mode  */

	int	$0x18
	movb	$0x0c, %ah		/* text on */
	int	$0x18

	/* cursor home and on */
	xor	%dx, %dx
	movb	$0x13, %ah
	int	$0x18
	movb	$0x11, %ah
	int	$0x18

	/* keyboad reset */
	movb	$0x03, %ah
	int	$0x18

	/* transfer PC-9801 system common area to 0xa1000 */
	mov	$0x0000, %esi
	mov	$0x1000, %edi
	mov	$0x0630, %ecx
	cld
	rep
	movsb

	/* transfer EPSON machine type to 0xa1200 */
	push	%ds
	mov	$0xfd00, %eax
	mov	%ax, %ds
	mov	0x804, %eax
	and	$0x00ffffff, %eax
	mov	%eax, %es: (0x1624)

	pop	%ds
	pop	%dx
	pop	%ecx
	pop	%es

	/* bootstrap passes */
	mov	%cs, %bx
	cmp	$0x1fe0, %bx
	jz	fd
	cmp	$0x1fc0, %bx
	jnz	hd
	xor	%cx, %cx
	movb	0x584, %al
	andb	$0xf0, %al
	cmpb	$0x30, %al
	jz	fd
	cmpb	$0x90, %al
	jnz	hd
fd:
	mov	$0x0200, %cx
	mov	$0x0001, %dx
	movb	$0xd6, %ah
	jmp	load
hd:
	and	%cx, %cx
	jnz	1f
	.code32
	addr32
	mov	%cs: (boot_cyl), %ecx	/* actualy %cx in real mode */
	.code16
1:
	xor	%dx, %dx
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
#ifdef NAMEBLOCK
/*
 * Load the second sector and see if it is a boot instruction block.
 * If it is then scan the contents for the first valid string and copy it to 
 * the location of the default boot string.. then zero it out.
 * Finally write the block back to disk with the zero'd out entry..
 * I hate writing at this stage but we need this to be persistant.
 * If the boot fails, then the next boot will get the next string.
 * /etc/rc will regenerate a complete block2 iff the boot succeeds.
 *
 * Format of block 2 is:
 * [NAMEBLOCKMAGIC] <--0xdeafc0de
 * [nulls]
 * [bootstring]NULL  <---e.g. 0:wd(0,a)/kernel.experimental
 * [bootstring]NULL  <---e.g. 0:wd(0,a)/kernel.old
 * ....
 * [bootstring]NULL  <---e.g. 0:wd(0,f)/kernel
 * FF FF FF
 */
where:
	/*
	 * save things we might smash
	 * (that are not smashed immedatly after us anyway.)
	 */
	data32
	push	%ecx	/* preserve 'cyl,sector ' */
	data32
	push	%edx
/* 
 * Load the second sector
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
	data32
	movl	$0x0201, %eax	/function 2 (read) 1 sector */
	xor	%ebx, %ebx	/* %bx = 0 */ /* buffer address (ES:0) */
	data32
	movl	$0x0002, %ecx	/* sector 2, cylinder 0 */
	data32
	andl	$0x00ff, %edx	/* head 0, drive N */
	int	$0x13
	data32
	jb	read_error
	/*
	 * confirm that it is one for us
	 */
	data32
	xorl	%ebx, %ebx	/* magic number at start of buffer */
	data32
	addr32
	movl	%es:(%ebx), %eax
	data32
	cmpl	$NAMEBLOCKMAGIC, %eax
	data32
	jne	notours		/* not ours so return to caller */
	/*
	 * scan for a bootstring
	 * Skip the magic number, and scan till we find a non-null,
	 * or a -1
	 */
	incl	%ebx	/* quicker and smaller */
	incl	%ebx
	incl	%ebx
scan:
	incl	%ebx
	addr32
	movb	%es:(%ebx), %al	/* load the next byte */
	testb	%al, %al	/* and if it is null */
	data32			/* keep scanning (past deleted entries) */
	jz scan
	incb	%al		/* now look for -1 */
	data32
	jz	notours		/* if we reach the 0xFF then we have finished */

	/*
	 * save our settings.. we need them twice..
	 */
	data32
	push	%ebx
	/*
	 * copy it to the default string location
	 * which is just above the stack for 64 bytes.
	 */
	data32
	movl	$BOOTSTACK-64, %ecx	/* 64 bytes at the top of the stack */
nxtbyte:
	addr32
	movb	%es:(%ebx), %al	/* get the next byte in */
	addr32
	movb	%al, %es:(%ecx)	/* and transfer it to the name buffer */
	incl	%ebx		/* get on with the next byte */
	incl	%ecx		/* get on with the next byte */
	testb	%al, %al	/* if it was 0 then quit this */
	data32
	jnz nxtbyte		/* and looop if more to do */ 
	
	/*
	 * restore the saved settings and
	 * zero it out so next time we don't try it again
	 */
	data32
	pop	%ebx		/* get back our starting location */
#ifdef	NAMEBLOCK_WRITEBACK
nxtbyte2:
	addr32
	movb	%es:(%ebx), %al	/* get the byte */
	addr32
	movb	$0,  %es:(%ebx)	/* zero it out */
	data32
	incl	%ebx		/* point to the next byte */
	testb	%al, %al	/* check if we have finished.. */
	data32
	jne nxtbyte2
/* 
 * Write the second sector back
 * Load the second sector
 * BIOS call "INT 0x13 Function 0x3" to write sectors from memory to disk
 *	Call with	%ah = 0x3
 *			%al = number of sectors
 *			%ch = cylinder
 *			%cl = sector
 *			%dh = head
 *			%dl = drive (0x80 for hard disk, 0x0 for floppy disk)
 *			%es:%bx = segment:offset of buffer
 *	Return:
 *			%al = 0x0 on success; err code on failure
 */
	data32
	movl	$0x0301, %eax	/* write 1 sector */
	xor	%ebx, %ebx	/* buffer is at offset 0 */ 
	data32
	movl	$0x0002, %ecx	/* block 2 */
	data32
	andl	$0xff, %edx	/* head 0 */
	int	$0x13
	data32
	jnb	notours
	data32
	mov	$eread, %esi
	jmp	err_stop
#endif	/* NAMEBLOCK_WRITEBACK */
	/*
	 * return to the main-line
	 */
notours:
	data32
	pop	%edx
	data32
	pop	%ecx
#endif
	mov	$LOADSZ, %ebx
	movb	0x584, %al
	xor	%bp, %bp	/* %bp = 0, put it at 0 in the BOOTSEG */
	int	$0x1b
	jc	read_error

	/*
	 * ljmp to the second stage boot loader (boot2).
	 * After ljmp, %cs is BOOTSEG and boot1 (512 bytes) will be used
	 * as an internal buffer "intbuf".
	 */

	.code32
	data32
	ljmp	$BOOTSEG, $ EXT(boot2)
	.code16

/*
 * read_error
 */
read_error:
	mov	$eread, %esi
err_stop:
	call	message
	jmp	stop

/*
 * message: write the error message in %ds:%esi to console
 */
message:

	push	%eax
	push	%ebx
	push	%ds
	push	%es
	mov	$0xe000, %dx
	mov	0x501, %al
	testb	$0x08, %al
	jnz	1f
	mov	$0xa000, %dx
1:
	mov	%dx, %es
	mov	%cs, %ax
	mov	%ax, %ds
	mov	vram, %di
	mov	$0x00e1, %bx
	mov	$160, %cx
	cld

nextb:
	lodsb			/* load a byte into %al */
	cmpb	$0x0, %al
	je	done
	cmpb	$0x0d, %al
	je	cr_code
	cmpb	$0x0a, %al
	je	lf_code
	movb	%bl, %es:0x2000(%di)
	stosb
	inc	%di
	jmp	move_cursor
lf_code:
	add	%cx, %di
	jmp	move_cursor
cr_code:
	xor	%dx, %dx
	mov	%di, %ax
	div	%cx
	sub	%dx, %di
move_cursor:
	mov	%di, %dx
	movb	$0x13, %ah
	int	$0x18
	jmp	nextb
done:
	mov	%di, vram
	pop	%es
	pop	%ds
	pop	%ebx
	pop	%eax
	ret

stop:	hlt
	jmp	stop		/* halt doesnt actually halt forever */

vram:
	.word	0

/* error messages */


#ifdef	DEBUG
one:	String		"1-\0"
two:	String		"2-\0"
three:	String		"3-\0"
four:	String		"4-\0"
#endif	DEBUG
#ifdef 	NAMEBLOCK_WRITEBACK
ewrite:	String		"Write error\r\n\0"
#endif	/* NAMEBLOCK_WRITEBACK */
eread:	String		"Read error\r\n\0"
enoboot: String		"No bootable partition\r\n\0"
endofcode:

	. = EXT(start) + 0x1be

/* Partition table */

	.fill 0x30,0x1,0x0
	.byte 0x80, 0x00, 0x01, 0x00
	.byte 0xa5, 0xff, 0xff, 0xff
	.byte 0x00, 0x00, 0x00, 0x00
	.byte 0x50, 0xc3, 0x00, 0x00

/* the last 2 bytes in the sector 0 contain the signature */
	.value	SIGNATURE

ENTRY(disklabel)
	. = EXT(start) + 0x400	
