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
 * $FreeBSD: src/sys/boot/pc98/boot2/bios.S,v 1.4.30.1 2008/11/25 02:59:29 kensmith Exp $
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

/*
 * Extensions for El Torito CD-ROM booting:
 *
 * Copyright © 1997 Pluto Technologies International, Inc.  Boulder CO
 * Copyright © 1997 interface business GmbH, Dresden.
 *      All rights reserved.
 *
 * This code has been written by Jörg Wunsch, Dresden.
 * Direct comments to <joerg_wunsch@interface-business.de>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

	.file	"bios.s"

#include "asm.h"
	.text

#ifndef CDBOOT

/*
 *  PC-9801/PC-9821 SCSI MO booting
 *    2002/06/05-07/03 Kawanobe Koh <kawanobe@st.rim.or.jp>
 *
 */
scsi_hd:
	.code16
	push	%cx
	push	%ds
	mov	%bl, %cl		/* UA */
	and	$0x0F, %cl
	xor	%ax, %ax
	mov	%ax, %ds
	mov	(0x0482), %al		/* SCSI HD equipment bits */
	shr	%cl, %al
	pop	%ds
	pop	%cx
	test	$1, %al
	ret

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
	.code32
	push	%ebp
	mov	%esp, %ebp

	push	%ebx
	push	%esi
	push	%edi

	mov	0x08(%ebp), %bl		/* (byte) DA/UA */
	mov	0x0C(%ebp), %ecx	/* (word) cylinder */
	mov	0x10(%ebp), %dh		/* (byte) head */
	mov	0x14(%ebp), %dl		/* (byte) sector */
	mov	0x18(%ebp), %esi	/* (byte) number of sectors */
	mov	0x1C(%ebp), %edi	/* (word) destination offset */

	/* prot_to_real will set %es to BOOTSEG */
	call	EXT(prot_to_real)	/* enter real mode */
	.code16
	mov	$0x06, %bh		/* read data function */
	mov	%bl, %al		/* DA */
	and	$0xF0, %al
	cmp	$0x30, %al		/* 1440KB FD */
	jz	read_floppy
	cmp	$0x90, %al		/* 1200KB FD */
	jz	read_floppy
	cmp	$0xA0, %al		/* SCSI HD or MO */
	jnz	read_next
	call	scsi_hd
	jnz	read_next
read_linear:
	mov	%dh, %al		/* change to linear sector */
	shl	$5, %al			/* multiply by 32 sector per track */
	add	%dl, %al
	xor	%dh, %dh		/* higher 16 bits into %dx */
	mov	%ch, %dl
	mov	%cl, %ch		/* lower 16 bits into %cx */
	mov	%al, %cl
	and	$0x7F, %bl		/* linear access DA/UA */
	jmp	read_next
read_floppy:
	inc	%dx			/* sector address begins from one */
	mov	$0x02, %ch		/* 512 bytes sector */
	mov	$0xD6, %bh		/* MT MFM retry seek */
read_next:
	mov	%si, %ax		/* number of sectors */
	shl	$9, %ax			/* multiply by 512 bytes */
	xchg	%bx, %ax
	mov	%di, %bp		/* destination offset */
	int	$0x1B			/* disk bios call */
	jc	read_end
	xor	%ax, %ax
read_end:
	mov	%ax, %bx		/* save return value */

	.code32
	data32
	call	EXT(real_to_prot)	/* back to protected mode */

	xor	%eax, %eax
	mov	%bh, %al		/* return value in %eax */

	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp

	ret

#else /* CDBOOT */


/*
 * int
 * getbootspec(struct specpacket *offset)
 *
 * Read CD-ROM boot specification packet to "offset".
 */
ENTRY(getbootspec)
	push	%ebp
	mov	%esp, %ebp

	push	%esi
	push	%ebx

	movw	0x8(%ebp), %si
	mov	$0x7f, %edx

	/* prot_to_real will set %es to BOOTSEG */
	call	EXT(prot_to_real)	/* enter real mode */
	movw	$0x4b01, %ax		/* (do not) terminate disk emulation */
	movb	$0x7f, %dl		/* any drive */

	sti
	int	$0x13
	cli

	/* save return value (actually movw %ax, %bx) */
	mov	%eax, %ebx

	data32
	call	EXT(real_to_prot)	/* back to protected mode */

	xor	%eax, %eax
	movb	%bh, %al		/* return value in %ax */

	pop	%ebx
	pop	%esi
	pop	%ebp

	ret


/*
 * int
 * biosreadlba(struct daddrpacket *daddr)
 *	Read sectors using the BIOS "read extended" function
 * BIOS call "INT 0x13 Function 0x42" to read sectors from disk into memory
 *	Call with	%ah = 0x42
 *			%dl = drive (0x0 for floppy disk, or emulated CD)
 *			%ds:%si = ptr to disk address packet
 *	Return:
 *			%ah = 0x0 on success; err code on failure
 */

ENTRY(biosreadlba)
	push	%ebp
	mov	%esp, %ebp

	push	%ebx
	push	%esi

	movw	8(%ebp), %si
	movl	$0, %edx		/* emulated CD is always drive 0 */

	/* prot_to_real will set %es to BOOTSEG */
	call	EXT(prot_to_real)	/* enter real mode */
	movw	$0x4200, %ax		/* subfunction */
	movb	$0, %dl

	sti
	int	$0x13
	cli

	/* save return value (actually movw %ax, %bx) */
	mov	%eax, %ebx

	data32
	call	EXT(real_to_prot)	/* back to protected mode */

	xor	%eax, %eax
	movb	%bh, %al		/* return value in %ax */

	pop	%esi
	pop	%ebx
	pop	%ebp

	ret

#endif /* !CDBOOT */

/*
 * getc()
 * BIOS call "INT 18H Function 00H" to read character from keyboard
 *	Call with	%ah = 0x0
 *	Return:		%ah = keyboard scan code
 *			%al = ASCII character
 */

ENTRY(getc)
	.code32
	push	%ebp
	mov	%esp, %ebp
	push	%ebx			/* save %ebx */
	push    %esi
	push    %edi

	call	EXT(prot_to_real)
	.code16

	movb	$0x0, %ah
	int	$0x18

	movb	%al, %bl		/* real_to_prot uses %eax */

	.code32
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
	.code32
	push	%ebp
	mov	%esp, %ebp
	push	%ebx
	push    %esi
	push    %edi

	call	EXT(prot_to_real)	/* enter real mode */

	xor	%ebx, %ebx
	.code16
	movb	$0x1, %ah
	int	$0x18
	andb	%bh, %bh
	data32
	jz	nochar
	movb	%al, %bl

nochar:
	.code32
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
	.code32
	push	%ebp
	mov	%esp, %ebp
	push	%ebx

	mov	0x08(%ebp), %bl		/* (byte) DA/UA */

	call	EXT(prot_to_real)	/* enter real mode */
	.code16
	mov	%bl, %al		/* DA */
	and	$0xf0, %al
	mov	$18, %dl		/* 1440KB FD sectors per track */
	cmp	$0x30, %al
	jz	floppy
	mov	$15, %dl		/* 1200KB FD sectors per track */
	cmp	$0x90, %al
	jz	floppy
	cmp	$0xA0, %al		/* SCSI HD or MO */
	jnz	sense
	call	scsi_hd
	jnz	sense

	push	%ds			/* SCSI MO or CD ? */
	xor	%ax, %ax
	mov	%ax, %ds
	and	$0x0F, %bx		/* UA */
	shl	$2, %bx			/* parameter offset */
	add	$0x0460, %bx
	mov	(%bx), %al		/* SCSI equipment parameter[0] */
	and	$0x1F, %al		/* peripheral device type */
	cmp	$7, %al			/* SCSI MO */
	jnz	good
	add	$3, %bx
	mov	(%bx), %al		/* SCSI equipment parameter[3] */
	test	$0x30, %al		/* sector length from 256 to 2048 */
	jnz	good
	or	$0x10, %al		/* forced set 512 bytes sector */
	mov	%al, (%bx)
	mov	$0xA100, %dx		/* refered by C language */
	mov	%dx, %ds
	mov	%al, (%bx)
good:
	pop	%ds

	mov	$0xFFFE, %cx		/* virtual 65535 cylinders setting */
	mov	$0x0820, %dx		/* standard 8 heads and 32 sectors */
	jmp	ok
sense:
	mov	$0x84, %ah		/* ask for disk info */
	mov	%bl, %al
	int	$0x1b
	jnc	ok			/* use %cx and %dx after */
	/*
	 * Urk.  Call failed.  It is not supported for floppies by old BIOS's.
	 * Guess it's a 15-sector floppy.
	 */
floppy:
	mov	$79, %cx		/* 80 cylinders 1200K and 1440K FD */
	mov	$2, %dh			/* 2 heads as double side */
ok:
	.code32
	data32
	call	EXT(real_to_prot)	/* back to protected mode */

	/* 
	 * form a longword representing all this gunk:
	 *	16 bit cylinder
	 *	 8 bit head
	 *	 8 bit sector
	 */
	mov	%ecx, %eax
	sal	$16, %eax		/* max cylinder number from zero */
	mov	%dx, %ax		/* number of heads and sectors */

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
	.code32
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
