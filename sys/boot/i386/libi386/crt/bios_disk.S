/*
 * $Id$
 * From:	$NetBSD: bios_disk.S,v 1.1.1.1 1997/03/14 02:40:32 perry Exp $	
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

/* extracted from netbsd:sys/arch/i386/boot/bios.S */

#include <machine/asm.h>

#define	addr32	.byte 0x67
#define	data32	.byte 0x66

/*
# BIOS call "INT 0x13 Function 0x2" to read sectors from disk into memory
#	Call with	%ah = 0x2
#			%al = number of sectors
#			%ch = cylinder
#			%cl = sector
#			%dh = head
#			%dl = drive (0x80 for hard disk, 0x0 for floppy disk)
#			%es:%bx = segment:offset of buffer
#	Return:		
#			%al = 0x0 on success; err code on failure
*/
ENTRY(biosread)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	push	%ecx
	push	%edx
	push	%esi
	push	%edi

	movb	16(%ebp), %dh
	movw	12(%ebp), %cx
	xchgb	%ch, %cl	# cylinder; the highest 2 bits of cyl is in %cl
	rorb	$2, %cl
	movb	20(%ebp), %al
	orb	%al, %cl
	incb	%cl		# sector; sec starts from 1, not 0
	movb	8(%ebp), %dl	# device
	movl	28(%ebp), %ebx	# offset
				# prot_to_real will set %es to BOOTSEG

	call	CNAME(prot_to_real)	# enter real mode

	movb	$0x2, %ah	# subfunction
	addr32
	movb	24(%ebp), %al	# number of sectors
	int	$0x13
	setc	%bl

	data32
	call	CNAME(real_to_prot) # back to protected mode

	xorl	%eax, %eax
	movb	%bl, %al	# return value in %ax

	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx
	popl	%ebx
	popl	%ebp
	ret

/*
#
# get_diskinfo():  return a word that represents the
#	max number of sectors and  heads and drives for this device
#
*/

ENTRY(get_diskinfo)
	pushl	%ebp
	movl	%esp, %ebp
	push	%es
	pushl	%ebx
	push	%ecx
	push	%edx
	push	%esi
	push	%edi

	movb	8(%ebp), %dl		# diskinfo(drive #)

	call	CNAME(prot_to_real)	# enter real mode

	movb	$0x08, %ah		# ask for disk info
	int	$0x13
	jnc	ok

	/*
	 * Urk.  Call failed.  It is not supported for floppies by old BIOS's.
	 * Guess it's a 15-sector floppy.  Initialize all the registers for
	 * documentation, although we only need head and sector counts.
	 */
#	subb	%ah, %ah		# %ax = 0
#	movb	%ah, %bh		# %bh = 0
#	movb	$2, %bl			# %bl bits 0-3 = drive type, 2 = 1.2M
#	movb	$79, %ch		# max track
	movb	$15, %cl		# max sector
	movb	$1, %dh			# max head
#	movb	$1, %dl			# # floppy drives installed
	# es:di = parameter table
	# carry = 0

ok:
	data32
	call	CNAME(real_to_prot)	# back to protected mode

	xorl	%eax, %eax

	/*form a longword representing all this gunk*/
	movb	%dh, %ah		# max head
	andb	$0x3f, %cl		# mask of cylinder gunk
	movb	%cl, %al		# max sector (and # sectors)

	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx
	popl	%ebx
	pop	%es
	popl	%ebp
	ret
