# Copyright (c) KATO Takenori, 1999, 2000.
#
# All rights reserved.  Unpublished rights reserved under the copyright
# laws of Japan.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer as
#    the first lines of this file unmodified.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$
#

	.code16

	.text
	.global	boot
#
# Read bootstrap program and jump to it.
#
boot:
	# Step 1: Save parameters
	movw	curdevice, %si
	movb	daua(%si), %al
	movb	%al, b_daua
	shlw	%si
	movw	secsize(%si), %ax
	movw	%ax, b_secsize

	movw	curpartition, %si
	movb	partnum(%si), %al	# %al = real partition number
	xorb	%ah, %ah
	movw	%ax, b_partn		# save real parttion number
	movb	$5, %cl
	shlw	%cl, %si		# %si = offset to parttable
	addw	$4, %si
	movb	parttable(%si), %al	# IPLS
	movb	%al, b_sector
	incw	%si
	movb	parttable(%si), %al	# IPLH
	movb	%al, b_head
	incw	%si			# IPLC
	movw	parttable(%si), %ax
	movw	%ax, b_cylinder

	# Step 2: Calculate egment address of bootstrap routine
	movw	$0x1d00, %ax
	movw	b_secsize, %cx
	shrw	%cx
	shrw	%cx
	subw	%cx, %ax
	subw	$0x100, %ax
	movw	%ax, b_bootseg

	# Step 3: Read bootstrap code
	movb	$6, %ah
	movb	b_daua, %al
	movw	b_secsize, %bx
	shlw	%bx			# 2 sectors
	movw	b_cylinder, %cx
	movb	b_head, %dh
	movb	b_sector, %dl
	movw	b_bootseg, %es
	xorw	%bp, %bp
	int	$0x1b
	jc	boot_error

	# Step 4: Set DA/UA into BIOS work area
	xorw	%ax, %ax
	movw	%ax, %es
	movw	$0x584, %bx		# DISK_BOOT
	movb	b_daua, %dl
	call	write_biosparam

	call	sc_clean
	# Step 5: Set registers
	#	%ah:	00
	#	%al:	DA/UA
	#	%bx:	Sector size * 2
	#	%cx:	cylinder number of boot partition
	#	%si:	pointer to partition table
	movw	b_partn, %ax
	movb	$5, %cl
	shl	%cl, %ax		# %ax = partition number * 32
	addw	b_secsize, %ax
	movw	%ax, %si		# %si = pointer to partition table
	movw	b_cylinder, %cx		# %cx = cylinder
	movb	b_head, %dh		# %dh = head
	movb	b_sector, %dl		# %dl = sector
	movw	b_bootseg, %es		# %es = boot segment
	movb	b_daua, %al		# %al = DA/UA
	movw	b_secsize, %bx
	shlw	%bx			# %bx = sector size * 2
	cli
	movw	%cs:iniss, %ss		# Restore stack pointer
	movw	%cs:inisp, %sp
	push	%es			# Boot segment
	xorw	%bp, %bp
	push	%bp			# 0
	movw	%ax, %di		# Save %ax
	xorw	%ax, %ax
	movw	%ax, %ds		# %ds = 0
	movw	%di, %ax		# Restore %ax
	xorb	%ah, %ah		# %ah = 0
	xorw	%di, %di		# %di = 0
	sti

	# Jump to bootstrap code
	lret
	# NOTREACHED

boot_error:
	ret

#
# Try to boot from default partition.
#
	.global	trydefault
trydefault:
	movw	ndevice, %cx
	xorw	%si, %si
trydefault_loop:
	movw	%si, curdevice
	push	%cx
	push	%si
	call	read_ipl
	pop	%si
	pop	%cx
	cmpb	$0x80, defpartflag
	jne	nodefpart
	# Default partition is defined.
	push	%cx
	movw	npartition, %cx
srch_part:
	movw	%cx, %bx
	decw	%bx
	movb	defpartnum, %al		# %al = real partition number
	cmpb	partnum(%bx), %al
	jne	not_match
	movw	%bx, curpartition	# Store partition number
	call	boot
not_match:
	loop	srch_part
	pop	%cx
nodefpart:
	incw	%si
	loop	trydefault_loop
	ret

	.data
b_daua:		.byte	0		# DA/UA
b_head:		.byte	0		# SYSH
b_sector:	.byte	0		# SYSS
b_cylinder:	.word	0		# SYSC
b_bootseg:	.word	0
b_secsize:	.word	0
b_partn:	.word	0		# Real partition number
