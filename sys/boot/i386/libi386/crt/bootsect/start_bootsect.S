/*	$NetBSD: start_bootsect.S,v 1.3 1998/02/19 14:15:38 drochner Exp $	*/
	
/* BIOS bootsector startup
 parts from netbsd:sys/arch/i386/boot/start.S
 and freebsd:sys/i386/boot/biosboot/boot2.S
 */

/*
 * Ported to boot 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
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
/* #include <machine/disklabel.h> */

#include "bbinfo.h"

BOOTSEG		=	0x0100	# boot will be loaded here (below 640K)
BOOTSTACK	=	0xfffc	# boot stack
LOADSZ		=	PRIM_LOADSZ	# size of first loaded chunk

SIGNATURE	=	0xaa55
PARTSTART	=	0x1be	# starting address of partition table
NUMPART		=	4	# number of partitions in partition table
PARTSZ		=	16	# sizeof(struct dos_partition)
BSDPART		=	0xA5
BOOTABLE	=	0x80	# value of dp_flag, means bootable partition

	.text
_boot1:
ENTRY(start)
	# start (aka boot1) is loaded at 0x0:0x7c00 but we want 0x7c0:0
	# ljmp to the next instruction to adjust %cs
	data32
	ljmp $0x7c0, $start1

start1:
	# set up %ds
	movl	%cs, %ax
	movl	%ax, %ds

	# temporary stack while we locate boot2
	# set up %ss and %esp
	data32
	movl	$BOOTSEG, %eax
	movl	%ax, %ss
	data32
	movl	$BOOTSTACK, %esp

	/*** set up %es, (where we will load boot2 to) ***/
	movl	%ax, %es

	# bootstrap passes us drive number in %dl
	cmpb	$0x80, %dl
	data32
	jae	hd

fd:
#	reset the disk system
	movb	$0x00, %ah
	int	$0x13
	data32
	movl	$0x0001, %ecx	# cyl 0, sector 1
	movb	$0x00, %dh	# head
	# XXX Override the drive number.
	movb	$0x00, %dl
	data32
	jmp	load

hd:	/**** load sector 0 (DOSBBSECTOR) into the BOOTSEG ****/
	data32
	movl	$0x0201, %eax
	xorl	%ebx, %ebx	# %bx = 0
	data32
	movl	$0x0001, %ecx
	data32
	andl	$0xff, %edx
	/*mov	$0x0080, %edx*/
	int	$0x13
	data32
	jb	read_error

	/***# find the first BSD partition *****/
	data32
	movl	$PARTSTART, %ebx
	data32
	movl	$NUMPART, %ecx
again:
	addr32
	movb    %es:4(%ebx), %al	/* dp_typ */
	cmpb	$BSDPART, %al
	data32
	je	found
	data32
	addl	$PARTSZ, %ebx
	data32
	loop	again

	/* didn't find either NetBSD or 386BSD partitions */
	data32
	movl	$enoboot, %esi
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
	movb	%es:1(%ebx), %dh /* head: dp_sdh */
	addr32
	movl	%es:2(%ebx), %ecx /*sect, cyl: dp_ssect + (dp_scyl << 8)*/

load:
	movb	$0x2, %ah	/* function 2 */
	movb	$LOADSZ, %al	/* number of blocks */
	xorl	%ebx, %ebx	/* %bx = 0, put it at 0 in the BOOTSEG */
	int	$0x13
	data32
	jb	read_error

	# ljmp to the second stage boot loader (boot2).
	# After ljmp, %cs is BOOTSEG and boot1 (512 bytes) will be used
	# for stack space while we load boot3.

	data32
	ljmp	$BOOTSEG, $CNAME(boot2)

#
#	read_error
#

read_error:
	data32
	movl	$eread, %esi
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
# BIOS call "INT 10H Function 0Eh" to write character to console
#	Call with	%ah = 0x0e
#			%al = character
#			%bh = page
#			%bl = foreground color
*/
	data32
	pushl	%eax
	data32
	pushl	%ebx
	data32
	pushl	%edx

nextb:
	cld
	lodsb			# load a byte into %al
	testb	%al, %al
	data32
	jz	done

	movb	$0x0e, %ah
	data32
	movl	$0x0001, %ebx
	int	$0x10

	data32
	jmp	nextb

done:
	data32
	popl	%edx
	data32
	popl	%ebx
	data32
	popl	%eax
	data32
	ret

_ourseg:
	.long	0

/**************************************************************************
GLOBAL DESCRIPTOR TABLE
**************************************************************************/
	.align	4
gdt:						/* 0x0 */
	.word	0, 0
	.byte	0, 0x00, 0x00, 0

/* additional dummy for Linux support (?) */	/* 0x8 */
	.word	0, 0
	.byte	0, 0x00, 0x00, 0

	/* kernel code segment */
	.globl flatcodeseg
flatcodeseg = . - gdt				/* 0x10 */
	.word	0xffff, 0
	.byte	0, 0x9f, 0xcf, 0

	/* kernel data segment */
	.globl flatdataseg
flatdataseg = . - gdt				/* 0x18 */
	.word	0xffff, 0
	.byte	0, 0x93, 0xcf, 0

	/* boot code segment, will be patched */
bootcodeseg = . - gdt				/* 0x20 */
	.word	0xffff, 0
	.byte	0, 0x9e, 0x40, 0

	/* boot data segment, will be patched */
bootdataseg = . - gdt				/* 0x28 */
	.word	0xffff, 0
	.byte	0, 0x92, 0x0f, 0

	/* 16 bit real mode, will be patched */
bootrealseg = . - gdt				/* 0x30 */
	.word	0xffff, 0
	.byte	0, 0x9e, 0x00, 0
gdtlen = . - gdt

	.align	4
gdtarg:
	.word	gdtlen-1		/* limit */
	.long	0			/* addr, will be inserted */

_boot2:
	data32
	xorl	%eax, %eax
	movl	%cs, %ax
	movl	%ax, %ds
	movl	%ax, %es
	addr32
	data32
	movl	%eax, CNAME(ourseg)
	data32
	shll	$4, %eax

	/* fix up GDT entries for bootstrap */
#define FIXUP(gdt_index) \
	addr32; \
	movl	%eax, gdt+gdt_index+2;	/* actually movw %ax */ \
	addr32; \
	movb	%bl, gdt+gdt_index+4

	data32
	shldl	$16, %eax, %ebx

	FIXUP(bootcodeseg)
	FIXUP(bootrealseg)
	FIXUP(bootdataseg)

	/* fix up GDT pointer */
	data32
	addl	$gdt, %eax
	addr32
	data32
	movl	%eax, gdtarg+2

	/* change to protected mode */
	data32
	call	CNAME(real_to_prot)

	/* move the stack over the top of boot1 while we load */
	movl	$_ourseg - 4, %eax
	movl	%eax, %esp
		
	movzbl	%dl, %edx	/* discard head (%dh) and random high bits */
	pushl	%edx
	call	CNAME(bootsectmain)

	/* Go to internal stack */
	movl	$CNAME(stacktop), %eax
	movl	%eax, %esp
	call	CNAME(main)
	
	
ENTRY(exit)
stop:
	cli
	hlt

eread:	.asciz		"Read error\r\n"
enoboot: .asciz		"No bootable partition\r\n"
endofcode:
/* throw in a partition in case we are block0 as well */
/* flag, head, sec, cyl, typ, ehead, esect, ecyl, start, len */
	. = CNAME(boot1) + PARTSTART
	.byte	0x0,0,0,0,0,0,0,0
	.long	0,0
	.byte	0x0,0,0,0,0,0,0,0
	.long	0,0
	.byte	0x0,0,0,0,0,0,0,0
	.long	0,0
	.byte	BOOTABLE,0,1,0,BSDPART,255,255,255
	.long	0,50000
/* the last 2 bytes in the sector 0 contain the signature */
	. = CNAME(boot1) + 0x1fe
	.short	SIGNATURE
	. = CNAME(boot1) + 0x200
	.globl	_disklabel
_disklabel:
	. = CNAME(boot1) + 0x400


CR0_PE		=	0x1
/*
 * real_to_prot()
 * 	transfer from real mode to protected mode.
 */
ENTRY(real_to_prot)
	# guarantee that interrupt is disabled when in prot mode
	cli

	# load the gdtr
	addr32
	data32
	lgdt	gdtarg

	# set the PE bit of CR0
	movl	%cr0, %eax

	data32
	orl	$CR0_PE, %eax
	movl	%eax, %cr0 

	# make intrasegment jump to flush the processor pipeline and
	# reload CS register
	data32
	ljmp	$bootcodeseg, $xprot

xprot:
	# we are in USE32 mode now
	# set up the protected mode segment registers : DS, SS, ES
	movl	$bootdataseg, %eax
	movl	%ax, %ds
	movl	%ax, %ss
	movl	%ax, %es

	ret

/*
 * prot_to_real()
 * 	transfer from protected mode to real mode
 */
ENTRY(prot_to_real)
	# set up a dummy stack frame for the second seg change.
	# Adjust the intersegment jump instruction following 
	# the clearing of protected mode bit.
	# This is self-modifying code, but we need a writable
	# code segment, and an intersegment return does not give us that.

	movl	_ourseg, %eax
	movw	%ax, xreal-2

	# Change to use16 mode.
	ljmp	$bootrealseg, $x16

x16:
	# clear the PE bit of CR0
	movl	%cr0, %eax
	data32
	andl 	$~CR0_PE, %eax
	movl	%eax, %cr0
	# Here we have an 16 bits intersegment jump.
	.byte 0xea
	.word xreal
	.word 0

xreal:
	# we are in real mode now
	# set up the real mode segment registers : DS, SS, ES
	movl	%cs, %ax
	movl	%ax, %ds
	movl	%ax, %ss
	movl	%ax, %es

	sti
	data32
	ret

/*
 * pbzero(dst, cnt)
 *	where dst is a physical address and cnt is the length
 */
ENTRY(pbzero)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%es
	pushl	%edi

	cld

	# set %es to point at the flat segment
	movl	$flatdataseg, %eax
	movl	%ax, %es

	movl	8(%ebp), %edi		# destination
	movl	12(%ebp), %ecx		# count
	xorl	%eax, %eax		# value

	rep
	stosb

	popl	%edi
	popl	%es
	popl	%ebp
	ret

/*
 * vpbcopy(src, dst, cnt)
 *	where src is a virtual address and dst is a physical address
 */
ENTRY(vpbcopy)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%es
	pushl	%esi
	pushl	%edi

	cld

	# set %es to point at the flat segment
	movl	$flatdataseg, %eax
	movl	%ax, %es

	movl	8(%ebp), %esi		# source
	movl	12(%ebp), %edi		# destination
	movl	16(%ebp), %ecx		# count

	rep
	movsb

	popl	%edi
	popl	%esi
	popl	%es
	popl	%ebp
	ret

/*
 * pvbcopy(src, dst, cnt)
 *	where src is a physical address and dst is a virtual address
 */
ENTRY(pvbcopy)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ds
	pushl	%esi
	pushl	%edi

	cld

	# set %ds to point at the flat segment
	movl	$flatdataseg, %eax
	movl	%ax, %ds

	movl	8(%ebp), %esi		# source
	movl	12(%ebp), %edi		# destination
	movl	16(%ebp), %ecx		# count

	rep
	movsb

	popl	%edi
	popl	%esi
	popl	%ds
	popl	%ebp
	ret

ENTRY(vtophys)
	movl _ourseg, %eax
	shll	$4, %eax
	addl 4(%esp), %eax
	ret
