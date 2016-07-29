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
#
# Check magic number at the end of the sector 0
#
check_magic:
	movw	curdevice, %si
	shlw	%si
	movw	secsize(%si), %bx
	decw	%bx
	decw	%bx
	movw	iplbuf(%bx), %ax
	cmpw	$0xaa55, %ax
	je	magic_ok
	movw	$1, %ax
	ret
magic_ok:
	xorw	%ax, %ax
	ret

#
# Copy partition table from buffer to parttable.
#
setup_partition:
	push	%cs
	pop	%es
	movw	curdevice, %bx
	shlw	%bx
	movw	maxpart(%bx), %cx	# %cx = max num of partitions
	movw	partoff(%bx), %di
	movw	%di, %bx		# %bx = offset to partition table
	xorw	%dx, %dx		# %dx = partition number
setup_partition_loop:
	push	%cx
	movw	%dx, %si
	movb	$5, %cl
	shlw	%cl, %si
	addw	%bx, %si
	movb	iplbuf(%si), %al
	orb	%al, %al
	jz	unused_partition
	addw	$iplbuf, %si
	movw	npartition, %ax
	movw	%ax, %di
	movb	$5, %cl
	shlw	%cl, %di
	addw	$parttable, %di
	movw	$32, %cx
	rep
	movsb
	movw	%ax, %di
	addw	$partnum, %di
	movb	%dl, (%di)
	incw	npartition
unused_partition:
	incw	%dx
	pop	%cx
	loop	setup_partition_loop
	ret

#
# Read IPL and partition table in the current device.
#
	.global	read_ipl
read_ipl:
	movw	curdevice, %ax
	movw	%ax, %si		# %si = device number
	movw	%ax, %di
	shlw	%di

	movw	%cs, %ax
	movw	%ax, %es
	movb	$6, %ah
	movb	daua(%si), %al
	movw	$0x400, %bx
	xorw	%cx, %cx
	xorw	%dx, %dx
	movw	$iplbuf, %bp
	int	$0x1b
	jc	read_ipl_error
	movw	defflagoff(%di), %bx
	movb	iplbuf(%bx), %al
	movb	%al, defpartflag
	incw	%bx
	movb	iplbuf(%bx), %al
	movb	%al, defpartnum
	movw	$0, npartition
	call	check_magic
	orw	%ax, %ax
	jnz	no_magic
	call	setup_partition
no_magic:
	xorw	%ax, %ax
read_ipl_error:
	xorw	%bx, %bx
	movw	%bx, %es
	ret

#
# Restore IPL from the buffer
#
	.global	write_ipl
write_ipl:
	movw	curdevice, %ax
	movw	%ax, %si
	movw	%ax, %di
	shlw	%di

	# Restore default boot partition info.
	movw	defflagoff(%di), %bx
	movb	defpartflag, %al
	movb	%al, iplbuf(%bx)
	incw	%bx
	movb	defpartnum, %al
	movb	%al, iplbuf(%bx)

	movw	%cs, %ax
	movw	%ax, %es
	movb	$5, %ah
	movb	daua(%si), %al
	movw	secsize(%di), %bx
	xorw	%cx, %cx
	xorw	%dx, %dx
	movw	$iplbuf, %bp
	int	$0x1b
	jc	write_ipl_error
	xorw	%ax, %ax
write_ipl_error:
	xorw	%bx, %bx
	movw	%bx, %es
	ret

#
# Scan HDD devices
#
	.global	scan_sasi, scan_scsi
	# Scan SASI disk
scan_sasi:
	# SASI Disk
	movw	$4, %cx
	movw	$0x0001, %ax	# %ah =  unit number, %al = for bit operation

sasi_loop:
	movw	%si, %di
	shlw	%di
	movw	$0x55d, %bx		# DISK_EQUIP
	call	read_biosparam
	testb	%al, %dl
	jz	no_sasi_unit
	movb	$0x80, %dh
	addb	%ah, %dh		# %dh = DA/UA
	movb	%dh, daua(%si)		# Store DA/UA

	# Try new sense command
	push	%ax
	push	%cx
	movb	%dh, %al
	movb	$0x84, %ah
	int	$0x1b
	pop	%cx
	pop	%ax
	jc	err_newsense
	movw	%bx, %dx
	jmp	found_sasi_unit

err_newsense:
	movw	$0x457, %bx		# capacity & sector size of IDE HDD
	call	read_biosparam
	orb	%ah, %ah
	jz	sasi_1
	cmpb	$1, %ah
	jz	sasi_2

	# SASI #3/#4
	movw	$512, %dx		# XXX
	jmp	found_sasi_unit

sasi_1:
	# SASI #1
	testb	$0x80, %dl
	jz	sasi_256
	jmp	sasi_512
sasi_2:
	# SASI #2
	testb	$0x40, %dl
	jz	sasi_256
	jmp	sasi_512

sasi_256:
	movw	$256, %dx
	jmp	found_sasi_unit
sasi_512:
	movw	$512, %dx
found_sasi_unit:
	movw	%dx, secsize(%di)
	incw	%si
no_sasi_unit:
	incb	%ah
	shlb	%al
	loop	sasi_loop
	ret

#
# Scan SCSI disk
#	SI	number of disks
#	destroyed: %ax, %bx, %cx, %dx
scan_scsi:
	movw	$8, %cx
	movw	$0x0001, %ax	# %ah = ID number, %al = for bit operation
scsi_loop:
	# Check whether drive exist.
	movw	%si, %di
	shlw	%di
	movw	$0x482, %bx		# DISK_EQUIPS
	call	read_biosparam
	testb	%al, %dl
	jz	no_scsi_unit
	xorw	%bx, %bx
	movb	%ah, %bl
	shlw	%bx
	shlw	%bx
	addw	$0x460, %bx		# SCSI parameter block
	call	read_biosparam
	orb	%dl, %dl
	jz	no_scsi_unit

	# SCSI harddrive found.
	movb	$0xa0, %dh
	addb	%ah, %dh
	movb	%dh, daua(%si)

	# Check sector size.
	addw	$3, %bx
	call	read_biosparam
	andb	$0x30, %dl
	cmpb	$0x20, %dl
	je	scsi_1024
	cmpb	$0x10, %dl
	je	scsi_512
	movw	$256, %dx
	jmp	found_scsi
scsi_1024:
	movw	$1024, %dx
	jmp	found_scsi
scsi_512:
	movw	$512, %dx
found_scsi:
	movw	%dx, secsize(%di)
	incw	%si
no_scsi_unit:
	incb	%ah
	shlb	%al
	loop	scsi_loop
	ret

	.data
	.global	defpartflag, defpartnum, npartition
defpartflag:	.byte	0
defpartnum:	.byte	0
npartition:	.word	0		# number of partitions

	.bss
	.global	partnum, parttable
iplbuf:		.space	0x400		# Read buffer for IPL
partnum:	.space	32		# Index of parttable
parttable:	.space	1024		# Copy of valid partition table
