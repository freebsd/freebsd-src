/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)wdbootblk.c	7.1 (Berkeley) 4/28/91
 *	$Id: wdbootblk.c,v 1.2 1993/10/16 18:49:39 rgrimes Exp $
 */

/*
 * wdbootblk.s:
 *	Written 7/6/90 by William F. Jolitz
 *	Initial block boot for AT/386 with typical Western Digital
 *	WD 1002-WA2 (or upwards compatable). Works either as
 *	first and sole partition bootstrap, or as loaded by a
 *	earlier BIOS boot when on an inner partition of the disk.
 *
 *	Goal is to read in sucessive 7.5Kbytes of bootstrap to
 *	execute.
 *
 *	No attempt is made to handle disk errors.
 */
#include "i386/isa/isa.h"
#include "i386/isa/wdreg.h"
#define	NOP	inb $0x84,%al
#define BIOSRELOC	0x7c00
#define start		RELOC+0x400

	/* step 0 force descriptors to bottom of address space */
	
	cli
	.byte 0xb8,0x30,0x00	/* mov $0x30,%ax */
	mov %ax, %ss
	.byte 0xbc,0x00,0x01	/* mov $0x100,%sp */

	xorl	%eax,%eax
	movl	%ax,%ds
	movl	%ax,%es

	/* obtain BIOS parameters for hard disk XXX */
	movb	$0x9f,%ah	 /* write to 0x9ff00  XXX */
	movb	$0xf0,%al
	mov	%ax,%es
	xor	%edi,%edi

	.byte 0xf, 0xb4, 0x36 ; .word  0x41*4	/* lfs 0x41*4, %si */
	xorb	%ch,%ch
	movb	$0x10,%cl
	fs
	rep
	movsb

	.byte 0xf, 0xb4, 0x36 ; .word  0x46*4	/* lfs 0x46*4, %si */
	xorb	%ch,%ch
	movb	$0x10,%cl
	fs
	rep
	movsb

 	xorl	%eax,%eax
	movl	%ax,%es

	/* step 1 load new descriptor table */

	.byte 0x3E,0x0F,1,0x16
	.word	BIOSRELOC+0x6e	#GDTptr
	# word aword cs lgdt GDTptr

	/* step 2 turn on protected mode */

	smsw	%ax
	orb	$1,%al
	lmsw	%ax
	jmp	1f
	nop

	/* step 3  reload segment descriptors */

1:
	xorl	%eax,%eax
	movb	$0x10,%al
	movl	%ax,%ds
	movl	%ax,%es
	movl	%ax,%ss
	word
	ljmp	$0x8,$ BIOSRELOC+0x74	/* would be nice if .-RELOC+0x7c00 worked */

 /* Global Descriptor Table contains three descriptors:
  * 0x00: Null: not used
  * 0x08: Code: code segment starts at 0 and extents for 4 gigabytes
  * 0x10: Data: data segment starts at 0 and extends for 4 gigabytes
  *		(overlays code)
  */
GDT:
NullDesc:	.word	0,0,0,0	# null descriptor - not used
CodeDesc:	.word	0xFFFF	# limit at maximum: (bits 15:0)
	.byte	0,0,0	# base at 0: (bits 23:0)
	.byte	0x9f	# present/priv level 0/code/conforming/readable
	.byte	0xcf	# page granular/default 32-bit/limit(bits 19:16)
	.byte	0	# base at 0: (bits 31:24)
DataDesc:	.word	0xFFFF	# limit at maximum: (bits 15:0)
	.byte	0,0,0	# base at 0: (bits 23:0)
	.byte	0x93	# present/priv level 0/data/expand-up/writeable
	.byte	0xcf	# page granular/default 32-bit/limit(bits 19:16)
	.byte	0	# base at 0: (bits 31:24)

/* Global Descriptor Table pointer
 *  contains 6-byte pointer information for LGDT
 */
GDTptr:	.word	0x17	# limit to three 8 byte selectors(null,code,data)
	.long 	BIOSRELOC+0x56	# GDT -- arrgh, gas again!

	/* step 4 relocate to final bootstrap address. */
reloc:
	movl	$ BIOSRELOC,%esi
	movl	$ RELOC,%edi
	movl	$512,%ecx
	rep
	movsb
 	movl	$0xa0000, %esp
	pushl	$dodisk
	ret

	/* step 5 load remaining 15 sectors off disk */
dodisk:
	movl	$ IO_WD1+wd_seccnt,%edx
	movb	$ 15,%al
	outb	%al,%dx
	NOP
	movl	$ IO_WD1+wd_sector,%edx
	movb	$ 2,%al
	outb	%al,%dx
	NOP
	#outb(wdc+wd_cyl_lo, (cyloffset & 0xff));
	#outb(wdc+wd_cyl_hi, (cyloffset >> 8));
	#outb(wdc+wd_sdh, WDSD_IBM | (unit << 4));

	movl	$ IO_WD1+wd_command,%edx
	movb	$ WDCC_READ,%al
	outb	%al,%dx
	NOP
	cld

	/* check to make sure controller is not busy and we have data ready */
readblk:
	movl	$ IO_WD1+wd_status,%edx
	NOP
	inb	%dx,%al
	testb	$ WDCS_BUSY,%al
	jnz readblk
	testb	$ WDCS_DRQ,%al
	jz readblk

	/* read a block into final position in memory */

	movl	$ IO_WD1+wd_data,%edx
	movl	$ 256,%ecx
	.byte 0x66,0xf2,0x6d	# rep insw
	NOP

	/* need more blocks to be read in? */

	cmpl	$ RELOC+16*512-1,%edi
	jl	readblk

	/* for clever bootstrap, dig out boot unit and cylinder */
	
	movl	$ IO_WD1+wd_cyl_lo,%edx
	inb	%dx,%al
	xorl	%ecx,%ecx
	movb	%al,%cl
	incl	%edx
	NOP
	inb	%dx,%al		/* cyl_hi */
	movb	%al,%ch
	pushl	%ecx		/* cyloffset */

	incl	%edx
	xorl	%eax,%eax
	NOP
	inb	%dx,%al		/* sdh */
	andb	$0x10,%al	/* isolate unit # bit */
	shrb	$4,%al
	pushl	%eax		/* unit */

	/* wd controller is major device 0 */
	xorl	%eax,%eax
	pushl	%eax		/* bootdev */

	/* sorry, no flags at this point! */

	movl	$ start, %eax
	call	%eax /* main (dev, unit, offset) */

ebootblkcode:

	/* remaining space usable for a disk label */
	
	.org	0x1fe
	.word	0xaa55		/* signature -- used by BIOS ROM */

ebootblk: 			/* MUST BE EXACTLY 0x200 BIG FOR SURE */
