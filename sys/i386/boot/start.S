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
 *	$Id: start.S,v 1.3 1994/06/13 19:27:52 jkh Exp $
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

	.file	"start.s"

BOOTSEG		=	0x9000	# boot will be loaded here (below 640K)
BOOTSTACK	=	0xe000	# boot stack
SIGNATURE	=	0xaa55
LOADSZ		=	15	# size of unix boot
PARTSTART	=	0x1be	# starting address of partition table
NUMPART		=	4	# number of partitions in partition table
PARTSZ		=	16	# each partition table entry is 16 bytes
BSDPART		=	0xA5	# value of boot_ind, means bootable partition
BOOTABLE	=	0x80	# value of boot_ind, means bootable partition

	.text	

ENTRY(boot1)
	# start (aka boot1) is loaded at 0x0:0x7c00 but we want 0x7c0:0
	# ljmp to the next instruction to adjust %cs
	data32
	ljmp $0x7c0, $start

start:
	# set up %ds
	mov	%cs, %ax
	mov	%ax, %ds

	# set up %ss and %esp
	data32
	mov	$BOOTSEG, %eax
	mov	%ax, %ss
	data32
	mov	$BOOTSTACK, %esp

	/*** set up %es, (where we will load boot2 to) ***/
	mov	%ax, %es

#ifdef	DEBUG
	data32
	mov	$one, %esi
	data32
	call	message
#endif

	# bootstrap passes us drive number in %dl
	cmpb	$0x80, %dl
	data32
	jae	hd

fd:
	mov	$0x0, %dl
#	reset the disk system
#ifdef	DEBUG
	data32
	mov	$two, %esi
	data32
	call	message
#endif
	movb	$0x0, %ah
	int	$0x13
	data32
	mov	$0x0001, %ecx	# cyl 0, sector 1
	movb	$0, %dh		# head
#ifdef	DEBUG
	data32
	mov	$three, %esi
	data32
	call	message
#endif
	data32
	jmp	load

hd:	/**** load sector 0 into the BOOTSEG ****/
#ifdef	DEBUG
	data32
	mov	$four, %esi
	data32
	call	message
#endif
	data32
	mov	$0x0201, %eax
	xor	%ebx, %ebx	# %bx = 0
	data32
	mov	$0x0001, %ecx
#ifdef	DEBUG
	data32
	mov	$five, %esi
	data32
	call	message
#endif
	data32
	andl	$0xff, %edx
	/*mov	$0x0080, %edx*/
	int	$0x13
	data32
	jb	read_error

	/***# find the first 386BSD partition *****/
	data32
	mov	$PARTSTART, %ebx
	data32
	mov	$NUMPART, %ecx
again:
	addr32
	movb    %es:4(%ebx), %al
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
# BIOS call "INT 0x13 Function 0x2" to read sectors from disk into memory
#       Call with       %ah = 0x2
#                       %al = number of sectors
#                       %ch = cylinder
#                       %cl = sector
#                       %dh = head
#                       %dl = drive (0x80 for hard disk, 0x0 for floppy disk)
#                       %es:%bx = segment:offset of buffer
#       Return:
#                       %al = 0x0 on success; err code on failure
*/

found:
	addr32
	movb	%es:1(%ebx), %dh /* head */
	addr32
	movl	%es:2(%ebx), %ecx /*sect, cyl (+ 2 bytes junk in top word) */

load:
	movb	$0x2, %ah	/* function 2 */
	movb	$LOADSZ, %al	/* number of blocks */
	xor	%ebx, %ebx	/* %bx = 0, put it at 0 in the BOOTSEG */
	int	$0x13
	data32
	jb	read_error

	# ljmp to the second stage boot loader (boot2).
	# After ljmp, %cs is BOOTSEG and boot1 (512 bytes) will be used
	# as an internal buffer "intbuf".

#ifdef	DEBUG
	data32
	mov	$six, %esi
	data32
	call	message
#endif
	data32
	ljmp	$BOOTSEG, $ EXT(boot2)

#
#	read_error
#

read_error:
	data32
	mov	$eread, %esi
err_stop:
	data32
	call	message
	data32
	jmp	stop

#
#	message: write the error message in %ds:%esi to console
#

message:
/*
	# Use BIOS "int 10H Function 0Eh" to write character in teletype mode
	#	%ah = 0xe	%al = character
	#	%bh = page	%bl = foreground color (graphics modes)
*/

	data32
	push	%eax
	data32
	push	%ebx
	data32
	mov	$0x0001, %ebx
	cld

nextb:
	lodsb			# load a byte into %al
	cmpb	$0x0, %al
	data32
	je	done
	movb	$0xe, %ah
	int	$0x10		# display a byte
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
	jmp	stop		# halt doesnt actually halt forever

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
/* throw in a partition in case we are block0 as well */
/* flag, head, sec, cyl, typ, ehead, esect, ecyl, start, len */
	. = EXT(boot1) + PARTSTART
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
