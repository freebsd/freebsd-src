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
#include	"asm.h"

	.file	"start.S"

SIGNATURE=	0xaa55
LOADSZ=		15	/* size of unix boot */
PARTSTART=	0x1be	/* starting address of partition table */
NUMPART=	4	/* number of partitions in partition table */
PARTSZ=		16	/* each partition table entry is 16 bytes */
BSDPART=	0xA5	/* value of boot_ind, means bootable partition */
BOOTABLE=	0x80	/* value of boot_ind, means bootable partition */
NAMEBLOCKMAGIC=	0xfadefeed /* value of magicnumebr for block2 	*/

/* to debug this, Use the DEBUGMSG(msg) macro defined in asm.S */

	.text	

ENTRY(boot1)
	/*
	 * start (aka boot1) is loaded at 0x0:0x7c00 but we want 0x7c0:0
	 * ljmp to the next instruction to adjust %cs
	 */
	data32
	ljmp $0x7c0, $start

start:
	/* set up %ds */
	mov	%cs, %ax
	mov	%ax, %ds

	/* set up %ss and %esp */
	data32
	mov	$BOOTSEG, %eax
	mov	%ax, %ss
	/*
	 * make a little room on the stack for
	 * us to save the default bootstring we might find..
	 * effectively, we push the bootstring.
	 */
	data32
	mov	$(BOOTSTACK - 64), %esp

	/* set up %es, (where we will load boot2 to) */
	mov	%ax, %es


	/* bootstrap passes us drive number in %dl */
	cmpb	$0x80, %dl
	data32
	jae	hd

fd:
	/*
	 * XXX some bootstraps don't pass the drive number in %dl.
	 * This is a problem mainly when we are block 0 on a floppy.
	 * Force drive 0 for floppies.
	 * XXX %dl was assumed valid in the test that led here.
	 */
	mov	$0x0, %dl

	/* reset the disk system */
	movb	$0x0, %ah
	int	$0x13
	data32
	mov	$0x0001, %ecx	/* cyl 0, sector 1 */
	movb	$0, %dh		/* head */
	data32
	jmp	load

hd:	/**** load sector 0 into the BOOTSEG ****/
	data32
	mov	$0x0201, %eax
	xor	%ebx, %ebx	/* %bx = 0 */
	data32
	mov	$0x0001, %ecx
	data32
	andl	$0xff, %edx
	/*mov	$0x0080, %edx*/
	int	$0x13
	data32
	jb	read_error

	/* find the first 386BSD partition */
	data32
	mov	$PARTSTART, %ebx
	data32
	mov	$NUMPART, %ecx
again:
	addr32
	movb	%es:4(%ebx), %al
	cmpb	$BSDPART, %al
	data32
	je	found
	data32
	add	$PARTSZ, %ebx
	data32
	loop	again
	data32
	mov	$enoboot, %esi
	data32
	jmp	err_stop


/*
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

found:
	addr32
	movb	%es:1(%ebx), %dh /* head */
	addr32
	movl	%es:2(%ebx), %ecx /*sect, cyl (+ 2 bytes junk in top word) */

load:
#ifdef NAMEBLOCK
/*
 * Load the second sector and see if it is a boot instruction block.
 * If it is then scan the contents for the first valid string and copy it to 
 * the location of the default boot string.. then zero it out.
 * Finally write the block back to disk with the zero'd out entry..
 * I hate writing at this stage but we need this to be persistant.
 * If the boot fails, then the next boot will get the next string.
 * /etc/rc will regenerate a complete block2 iff teh boot succeeds.
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
	movl	$(BOOTSTACK-64), %ecx	/* 64 bytes at the top of the stack */
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
	movb	$0x2, %ah	/* function 2 */
	movb	$LOADSZ, %al	/* number of blocks */
	xor	%ebx, %ebx	/* %bx = 0, put it at 0 in the BOOTSEG */
	int	$0x13
	data32
	jb	read_error

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
ENTRY(message)
message:
	/*
	 * Use BIOS "int 10H Function 0Eh" to write character in teletype mode
	 *	%ah = 0xe	%al = character
	 *	%bh = page	%bl = foreground color (graphics modes)
	 */

	data32
	push	%eax
	data32
	push	%ebx
	data32
	mov	$0x0001, %ebx
	cld

nextb:
	lodsb			/* load a byte into %al */
	cmpb	$0x0, %al
	data32
	je	done
	movb	$0xe, %ah
	int	$0x10		/* display a byte */
	data32
	jmp	nextb
done:
	data32
	pop	%ebx
	data32
	pop	%eax
	data32
	ret

stop:	hlt
	data32
	jmp	stop		/* halt doesnt actually halt forever */

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
/*
 * Dummy partition table in case we are block 0.  The ending c/h/s values
 * of the non-null partition are almost arbitary.  The length of this
 * partition is bogus for backwards compatibility and as a signature.
 * A real partition table shouldn't be as weird and broken as this one,
 * and the isa slice initialization routine interprets this table as
 * saying that the whole disk is used for FreeBSD.
 */
/* flag, head, sec, cyl, typ, ehead, esect, ecyl, start, len */
	. = EXT(boot1) + PARTSTART
strttbl:
	.byte 0x0,0,0,0,0,0,0,0
	.long 0,0
	.byte 0x0,0,0,0,0,0,0,0
	.long 0,0
	.byte 0x0,0,0,0,0,0,0,0
	.long 0,0
	.byte BOOTABLE,0,1,0,BSDPART,255,255,255
	.long 0,50000
/* the last 2 bytes in the sector 0 contain the signature */
	. = EXT(boot1) + 0x1fe
	.value	SIGNATURE
ENTRY(disklabel)
	. = EXT(boot1) + 0x400	
