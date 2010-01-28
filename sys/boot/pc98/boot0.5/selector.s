# Copyright (c) KATO Takenori, 1999, 2000, 2007.
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
# Display partition table.
#
showpartitions:
	# Clear partition table area
	movw	$16, %cx
clear_part:
	push	%cx
	movw	%cx, %dx
	decw	%dx
	addw	$5, %dx
	movw	$20, %cx
	call	sc_goto
	movw	$msg_spc, %di
	call	sc_puts
	pop	%cx
	loop	clear_part

	# Check `Exit' menu
	movw	curdevice, %ax
	cmpw	ndevice, %ax
	je	no_slice

	# XXX Move this to a suitable place!
	movw	$22, %cx
	movw	$4, %dx
	call	sc_goto
	movw	$msg_slice, %di
	call	sc_puts

	# Check the number of partitions
	movw	npartition, %cx
	orw	%cx, %cx
	jnz	partitionexist
no_slice:
	# Just show the `no slice' message.
	movw	$22, %cx
	movw	$5, %dx
	call	sc_goto
	movw	$msg_noslice, %di
	call	sc_puts
	ret
partitionexist:
	xorw	%si, %si		# %si = partition number
showpart_loop:
	push	%cx			# %cx = number of partitions
	movw	$22, %cx
	movw	%si, %dx
	addw	$5, %dx
	call	sc_goto
	movw	%si, %di
	movb	$5, %cl
	shlw	%cl, %di
	addw	$0x10, %di		# SYSM field
	# SYSM: space filled string.  Don't use sc_puts.
	movw	$16, %cx
showpart_name:
	push	%cx
	movb	parttable(%di), %al
	call	sc_putc
	incw	%di
	pop	%cx
	loop	showpart_name
	incw	%si
	pop	%cx
	loop	showpart_loop
	ret

#
# Show default slice indicator
# If the default boot slice exists, `*' indicator will be showed.
#
showdefaultslicemark:
	cmpb	$0x80, defpartflag
	je	defpartexist
	ret
defpartexist:
	movw	npartition, %cx
defslice_loop:
	movw	%cx, %bx
	decw	%bx
	push	%cx
	push	%bx
	movw	$40, %cx
	movw	%bx, %dx
	addw	$5, %dx
	call	sc_goto

	pop	%bx
	pop	%cx
	movb	defpartnum, %al
	cmpb	partnum(%bx), %al
	jne	nomatch
	movb	$'*', %al
	call	sc_putc
	jmp	defslice_done
nomatch:
	movb	$' ', %al
	call	sc_putc
defslice_done:
	loop	defslice_loop
	ret

#
# Hide default slice indicator
#
hidedefaultslicemark:
	movw	$16, %cx
hidedefslice_loop:
	push	%cx
	movw	%cx, %dx
	addw	$4, %dx
	movw	$40, %cx
	call	sc_goto
	movb	$' ', %al
	call	sc_putc
	pop	%cx
	loop	hidedefslice_loop
	ret

#
# Toggle default slice
#
toggle_default:
	cmpb	$0x80, defpartflag
	jne	set_default
	# Clear default
	movb	$0, defpartflag
	call	write_ipl		# Restore
	call	hidedefaultslicemark
	ret
	# Set default slice
set_default:
	movw	curpartition, %si
	movb	partnum(%si), %al	# %al = real partition number
	movb	$5, %cl
	shlw	%cl, %si
	# Default slice must be bootable
	testb	$0x80, parttable(%si)
	jnz	curpart_bootable
	# Current partition is not bootable.
	ret
curpart_bootable:
	movb	$0x80, defpartflag
	movb	%al, defpartnum
	call	write_ipl		# Restore
	call	showdefaultslicemark
	ret

#
# Show/hide cursor
#
show_devcurs:
	xorw	%cx, %cx
	movw	curdevice, %dx
	addw	$5, %dx
	call	sc_goto
	movb	$'>', %al
	call	sc_putc
	movb	$'>', %al
	call	sc_putc
	ret

hide_devcurs:
	xorw	%cx, %cx
	movw	curdevice, %dx
	addw	$5, %dx
	call	sc_goto
	movb	$' ', %al
	call	sc_putc
	movb	$' ', %al
	call	sc_putc
	ret

show_slicecurs:
	movw	$20, %cx
	movw	curpartition, %dx
	addw	$5, %dx
	call	sc_goto
	movb	$'>', %al
	call	sc_putc
	movb	$'>', %al
	call	sc_putc
	ret

hide_slicecurs:
	movw	$20, %cx
	movw	curpartition, %dx
	addw	$5, %dx
	call	sc_goto
	movb	$' ', %al
	call	sc_putc
	movb	$' ', %al
	call	sc_putc
	ret

isforceboot:
	xorw	%cx, %cx
	movw	$20, %dx
	call	sc_goto
	movw	$msg_force, %di
	call	sc_puts
	call	sc_getc
	push	%ax
	xorw	%cx, %cx
	movw	$20, %dx
	call	sc_goto
	movw	$msg_forceclr, %di
	call	sc_puts
	pop	%ax
	cmpb	$0x15, %ah
	je	force_yes
	xorw	%ax, %ax
	ret
force_yes:
	movw	$1, %ax
	ret

#
# Main loop for device mode
#
devmode:
	call	read_ipl
	call	hidedefaultslicemark
	call	showpartitions
	call	showdefaultslicemark
	call	show_devcurs

	movw	$2, %cx
	movw	$4, %dx
	call	sc_goto
	movb	$0xe5, %al
	movw	$6, %cx
	call	sc_setattr
	movw	$22, %cx
	movw	$4, %dx
	call	sc_goto
	movb	$0xe1, %al
	movw	$5, %cx
	call	sc_setattr
	movw	$44, %cx
	movw	$3, %dx
	call	sc_goto
	movb	$0xe5, %al
	movw	$11, %cx
	call	sc_setattr
	movw	$44, %cx
	movw	$7, %dx
	call	sc_goto
	movb	$0xe1, %al
	movw	$10, %cx
	call	sc_setattr

devmode_loop:
	call	sc_getc
	movw	ndevice, %bx
	cmpb	$0x3a, %ah		# UP
	je	dev_up
	cmpb	$0x3d, %ah		# DOWN
	je	dev_down
	cmpb	$0x3c, %ah		# RIGHT
	je	dev_right
	cmpb	$0x1c, %ah		# RETURN
	jne	devmode_loop
	cmpw	curdevice, %bx
	jne	dev_right
	movw	$3, mode		# N88-BASIC
	ret

	# XXX
	.space	5, 0x90
	ret				# Dummy ret @0x9ab

dev_up:
	cmpw	$0, curdevice
	je	devmode_loop
	call	hide_devcurs
	decw	curdevice
	call	read_ipl
	call	hidedefaultslicemark
	call	showpartitions
	call	showdefaultslicemark
	call	show_devcurs
	jmp	devmode_loop
dev_down:
	cmpw	curdevice, %bx
	je	devmode_loop
	call	hide_devcurs
	incw	curdevice
	call	read_ipl
	call	hidedefaultslicemark
	call	showpartitions
	call	showdefaultslicemark
	call	show_devcurs
	jmp	devmode_loop
dev_right:
	cmpw	curdevice, %bx
	je	devmode_loop
	movw	$1, mode		# Slice mode
	ret

#
# main loop for slice mode
#
slicemode:
	movw	$0, curpartition
	call	show_slicecurs
	movw	$2, %cx
	movw	$4, %dx
	call	sc_goto
	movb	$0xe1, %al
	movw	$6, %cx
	call	sc_setattr
	movw	$22, %cx
	movw	$4, %dx
	call	sc_goto
	movb	$0xe5, %al
	movw	$5, %cx
	call	sc_setattr
	movw	$44, %cx
	movw	$3, %dx
	call	sc_goto
	movb	$0xe1, %al
	movw	$11, %cx
	call	sc_setattr
	movw	$44, %cx
	movw	$7, %dx
	call	sc_goto
	movb	$0xe5, %al
	movw	$10, %cx
	call	sc_setattr

slicemode_loop:
	call	sc_getc
	cmpb	$0x3a, %ah		# UP
	je	slice_up
	cmpb	$0x3d, %ah		# DOWN
	je	slice_down
	cmpb	$0x3b, %ah		# LEFT
	je	slice_esc
	cmpb	$0x00, %ah		# ESC
	je	slice_esc
	cmpb	$0x1c, %ah		# RETURN
	je	slice_ret
	cmpb	$0x34, %ah		# SPC
	je	slice_spc
	cmpb	$0x62, %ah		# f1
	je	slice_spc
	jmp	slicemode_loop
slice_up:
	cmpw	$0, curpartition
	je	slicemode_loop
	call	hide_slicecurs
	decw	curpartition
	call	show_slicecurs
	jmp	slicemode_loop
slice_down:
	movw	curpartition, %bx
	movw	npartition, %ax
	decw	%ax
	cmpw	%bx, %ax
	je	slicemode_loop
	call	hide_slicecurs
	incw	curpartition
	call	show_slicecurs
	jmp	slicemode_loop
slice_esc:
	movw	$0, mode		# Device mode
	ret
slice_spc:
	call	toggle_default
	jmp	slicemode_loop
slice_ret:
	# Test bit 7 of mid
	movw	curpartition, %si
	movb	$5, %cl
	shlw	%cl, %si
	testb	$0x80, parttable(%si)
	jnz	bootable_slice
	call	isforceboot
	orw	%ax, %ax
	jz	slicemode_loop
bootable_slice:
	call	boot
	jmp	slicemode_loop

#
# Main loop
#
	.global	selector
selector:
	movw	$0, curdevice	# trydefault may change the curdevice.
	movw	$0, mode

selector_loop:
	cmpw	$0, mode
	je	status_dev
	cmpw	$1, mode
	je	status_slice
	ret
status_dev:
	call	devmode
	jmp	selector_loop
status_slice:
	call	slicemode
	jmp	selector_loop

	.data
	.global	curpartition
curpartition:	.word	0		# current patition
mode:		.word	0

msg_spc:	.asciz	"                        "
msg_slice:	.asciz	"Slice"
msg_noslice:	.asciz	"no slice"
msg_force:	.asciz	"This slice is not bootable. Continue? (Y / [N])"
msg_forceclr:	.asciz	"                                               "
