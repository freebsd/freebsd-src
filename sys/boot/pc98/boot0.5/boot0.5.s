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
	.global	main
	.code16

	.text
main:
	# Check hireso mode
	movw	$0x501, %bx		# BIOS_FLAG
	call	read_biosparam
	testb	$0x08, %dl
	jz	normalmode
	movb	$1, ishireso
normalmode:
	call	sc_init

	# Display title and copyright.
	movw	$title, %di
	call	sc_puts
	xorw	%cx, %cx
	movw	$1, %dx
	call	sc_goto
	movw	$copyright, %di
	call	sc_puts

	# Scan hard drives
	xorw	%si, %si		# number of partition
	call	scan_sasi		# SASI/IDE
	call	scan_scsi		# SCSI
	movw	%si, ndevice
	orw	%si, %si
	jnz	drives_found
	jmp	exit			# No hard drives

drives_found:
	# Setup sector size dependent parameters
	movw	%si, %cx		# %cx = number of devices
setup_loop:
	movw	%cx, %di
	decw	%di
	shlw	%di
	movw	secsize(%di), %ax
	cmpw	$1024, %ax
	je	setup_1024
	cmpw	$512, %ax
	je	setup_512
	# 256 bytes/sector
	movw	$0x100, partoff(%di)
	movw	$0x0fa, defflagoff(%di)
	movw	$0x0fb, defpartoff(%di)
	movw	$8, maxpart(%di)
	jmp	setup_secsize_end
	# 1024 bytes/sector
setup_1024:
	# XXX Fix me!
	movw	$0x400, partoff(%di)
	movw	$0x3fa, defflagoff(%di)
	movw	$0x3fb, defpartoff(%di)
	movb	$32, maxpart(%di)
	jmp	setup_secsize_end
	# 512 bytes/sector
setup_512:
	movw	$0x200, partoff(%di)
	movw	$0x1fa, defflagoff(%di)
	movw	$0x1fb, defpartoff(%di)
	movb	$16, maxpart(%di)
setup_secsize_end:
	loop	setup_loop

	# For debug with floppy, fake the parameter.
	movw	$0x584, %bx		# DISK_BOOT
	call	read_biosparam
	andb	$0xf0, %dl
	cmpb	$0x90, %ah
	jne	boot_from_hdd
	movb	daua, %dl
	call	write_biosparam

boot_from_hdd:
	movw	$500, %cx
wait_0_5:
	call	wait1ms
	loop	wait_0_5

	# If the TAB is pressed, don't try to boot from default partition
	xorw	%di, %di		# flag
wait_key_release:
	call	sc_iskeypress
	orw	%ax, %ax
	jz	key_release		# KBD buffer empty.
	call	sc_getc
	cmpb	$0x0f, %ah		# TAB
	jne	wait_key_release
	# TAB pressed
	movw	$1, %di
	jmp	wait_key_release
key_release:
	orw	%di, %di
	jnz	dont_try_default	# TAB pressed.
	call	trydefault
	# Default partition not found.
dont_try_default:
	call	show_usage
	call	showdevices
	call	selector
exit:
	ret
#
# Display usage
#
show_usage:
	movw	$44, %cx
	movw	$3, %dx
	call	sc_goto
	movw	$msg_usage1, %di
	call	sc_puts
	movw	$44, %cx
	movw	$4, %dx
	call	sc_goto
	movw	$msg_usage2, %di
	call	sc_puts
	movw	$44, %cx
	movw	$5, %dx
	call	sc_goto
	movw	$msg_usage3, %di
	call	sc_puts
	movw	$44, %cx
	movw	$7, %dx
	call	sc_goto
	movw	$msg_usage4, %di
	call	sc_puts
	movw	$44, %cx
	movw	$8, %dx
	call	sc_goto
	movw	$msg_usage5, %di
	call	sc_puts
	movw	$44, %cx
	movw	$9, %dx
	call	sc_goto
	movw	$msg_usage6, %di
	call	sc_puts
	movw	$44, %cx
	movw	$10, %dx
	call	sc_goto
	movw	$msg_usage7, %di
	call	sc_puts
	movw	$44, %cx
	movw	$11, %dx
	call	sc_goto
	movw	$msg_usage8, %di
	call	sc_puts
	movw	$44, %cx
	movw	$16, %dx
	call	sc_goto
	movw	$msg_usage9, %di
	call	sc_puts
	movw	$44, %cx
	movw	$17, %dx
	call	sc_goto
	movw	$msg_usage10, %di
	call	sc_puts
	movw	$44, %cx
	movw	$18, %dx
	call	sc_goto
	movw	$msg_usage11, %di
	call	sc_puts
	movw	$44, %cx
	movw	$19, %dx
	call	sc_goto
	movw	$msg_usage12, %di
	call	sc_puts
	ret

#
# Display device list
#
showdevices:
	movw	$2, %cx
	movw	$4, %dx
	call	sc_goto
	movw	$msg_device, %di
	call	sc_puts
	xorw	%si, %si		# %si = device number
	movw	ndevice, %cx		# %cx = number of devices
showdev_loop:
	push	%cx
	movw	$2, %cx
	movw	$5, %dx
	addw	%si, %dx
	call	sc_goto
	# Check DA
	movb	daua(%si), %al
	push	%ax
	andb	$0xf0, %al
	cmpb	$0x80, %al
	je	show_sasi
	cmpb	$0xa0, %al
	je	show_scsi
	# unknown device
	movw	$msg_unknown, %di
	call	sc_puts
	jmp	showunit
	# SASI
show_sasi:
	movw	$msg_sasi, %di
	call	sc_puts
	jmp	showunit
	# SCSI
show_scsi:
	movw	$msg_scsi, %di
	call	sc_puts
	# Display unit number.
showunit:
	pop	%ax
	andb	$0x0f, %al
	addb	$'0', %al
	call	sc_putc
	incw	%si
	pop	%cx
	loop	showdev_loop
	movw	ndevice, %dx
	addw	$5, %dx
	movw	$2, %cx
	call	sc_goto
	movw	$msg_exitmenu, %di
	call	sc_puts
	ret

	.data
	.global	curdevice, ndevice
ndevice:	.word	0		# number of device
curdevice:	.word	0		# current device

	.global	ishireso
ishireso:	.byte	0

title:		.asciz	"PC98 Boot Selector Version 1.1"
copyright:	.ascii	"(C)Copyright 1999, 2000 KATO Takenori. "
		.asciz	"All rights reserved."
msg_device:	.asciz	"Device"
msg_sasi:	.asciz	"SASI/IDE unit "
msg_scsi:	.asciz	"SCSI ID "
msg_unknown:	.asciz	"unknown unit "
msg_exitmenu:	.asciz	"Exit this menu"
msg_usage1:	.asciz	"Device list"
msg_usage2:	.asciz	"UP, DOWN: select boot device"
msg_usage3:	.asciz	"RETURN: move to slice list"
msg_usage4:	.asciz	"Slice list"
msg_usage5:	.asciz	"UP, DOWN: select boot slice"
msg_usage6:	.asciz	"RETURN: boot"
msg_usage7:	.asciz	"SPACE: toggle default"
msg_usage8:	.asciz	"ESC: move to device list"
msg_usage9:	.asciz	"LEGEND"
msg_usage10:	.asciz	">>: selected device/slice"
msg_usage11:	.asciz	"*: default slice to boot"
msg_usage12:	.asciz	"!: unbootable slice"

	.bss
	.global	daua, secsize, defflagoff, defpartoff
	.global	maxpart, partoff
daua:		.space	12		# DA/DU list
secsize:	.space	12 * 2		# Sector soize
defflagoff:	.space	12 * 2
defpartoff:	.space	12 * 2
maxpart:	.space	12 * 2
partoff:	.space	12 * 2
