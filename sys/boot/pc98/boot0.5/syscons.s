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
#	%al	character code
#	destroyed: %al, %bx
#
put_character:
	movw	$0xe000, %bx
	movb	ishireso, %ah
	orb	%ah, %ah
	jne	hireso_ch
	movw	$0xa000, %bx
hireso_ch:
	movw	%bx, %es
	xorb	%ah, %ah
	movw	curpos, %bx
	movw	%ax, %es:(%bx)
	xorw	%ax, %ax
	movw	%ax, %es
	ret

#
#	%al	attribute
#	destroyed: %ah, %cx
#
set_attribute:
	movw	$0xe200, %bx
	movb	ishireso, %ah
	orb	%ah, %ah
	jne	hireso_ch
	movw	$0xa200, %bx
hireso_attr:
	movw	%bx, %es
	xorb	%ah, %ah
	movw	curpos, %bx
	movw	%ax, %es:(%bx)
	xorw	%bx, %bx
	movw	%bx, %es
	ret

#
# Put a character
#	%al:	character code
#	destroyed: %ah, %bx, %cx
#
	.global	sc_putc
sc_putc:
	call	put_character
	incw	curpos
	incw	curpos
	cmpw	$4000, curpos
	jng	putc_end
	movw	$0, curpos
putc_end:
	ret

#
# Put a null terminated string
#	%di:	pointer to string
#	destroyed: %ah, %cx, %di
#
	.global	sc_puts
sc_puts:
	movb	(%di), %al
	orb	%al, %al
	jz	puts_end
	call	sc_putc
	incw	%di
	jmp	sc_puts
puts_end:
	ret

#
# Change the current cursor position
#	%cx:	X
#	%dx:	Y
#	destroyed: %ax,  %bx
#
	.global	sc_goto
sc_goto:
	movw	%dx, %ax		# AX=Y
	shlw	%ax			# AX=Y*64
	shlw	%ax
	shlw	%ax
	shlw	%ax
	shlw	%ax
	shlw	%ax
	movw	%dx, %bx		# BX=Y
	shlw	%bx			# BX=Y*16
	shlw	%bx
	shlw	%bx
	shlw	%bx
	addw	%bx, %ax		# AX=Y*64+Y*16=Y*80
	addw	%cx, %ax
	shlw	%ax
	movw	%ax, curpos
	ret

#
# Clear screen
#	destroyed: %ax, %bx
#
	.global	sc_clean
sc_clean:
	movb	$0x16, %ah
	movw	$0xe120, %dx
	int	$0x18			# KBD/CRT BIOS
	movw	$0, curpos
	ret

#
# Set sttribute code
#	%al:	attribute
#	%cx:	count
#	destroyed: %ax, %bx, %cx
#
	.global	sc_setattr
sc_setattr:
	call	set_attribute
	incw	curpos
	incw	curpos
	loop	sc_setattr

#
# Sense the state of shift key
#	destroyed: %ax
#
	.global	sc_getshiftkey
sc_getshiftkey:
	movb	$2, %ah			# Sense KB_SHIFT_COD
	int	$0x18			# KBD/CRT BIOS
	xorb	%ah, %ah
	ret

#
# Check KBD buffer
#
	.global	sc_iskeypress
sc_iskeypress:
	mov	$1, %ah
	int	$0x18			# KBD/CRT BIOS
	testb	$1, %bh
	jz	no_key
	movw	$1, %ax
	ret
no_key:
	xorw	%ax, %ax
	ret

#
# Read from KBD
#
	.global	sc_getc
sc_getc:
	xorb	%ah, %ah
	int	$0x18
	ret

#
# Initialize CRT (normal mode)
#
init_screen_normal:
	# Disable graphic screen
	movb	$0x41, %ah
	int	$0x18
	# Init graphic screen
	movb	$0x42, %al
	movb	$0xc0, %ch
	int	$0x18
	# 80x25 mode
	movw	$0x0a00, %ax
	int	$0x18
	ret

#
# Initialize CRT (hireso mode)
#
init_screen_hireso:
	# Init RAM window
	movb	$8, %al
	outb	%al, $0x91
	movb	$0x0a, %al
	outb	%al, $0x93
	# 80x31 mode
	movw	$0x0a00, %ax
	int	$0x18
	ret

#
# Initialize screen (internal)
#
init_screen:
	movb	ishireso, %ah
	orb	%ah, %ah
	jne	hireso_ini
	call	init_screen_normal
	jmp	init_next
hireso_ini:
	call	init_screen_hireso
init_next:
	movb	$0x0c, %ah
	int	$0x18
	# cursor home and off
	xorw	%dx, %dx
	movb	$0x13, %ah
	int	$0x18
	movb	$0x12, %ah
	int	$0x18
	ret

#
# Initialize screeen
#
	.global	sc_init
sc_init:
	call	init_screen
	call	sc_clean
	movw	$0, curpos
	ret

	.data
curpos:		.word	0		# Current cursor position
