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
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
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
# $FreeBSD: src/sys/boot/pc98/boot0/boot0.s,v 1.1.2.1 2000/09/11 09:19:46 kato Exp $

	.globl start
	.code16
start:
	jmp	main

	.org	4
	.ascii	"IPL1"
	.byte	0, 0, 0

	.globl start
main:
	xor	%ax, %ax
	mov	%ax, %ds
	mov	(0x584), %al		# DA/UA
	mov	%al, %ah
	and	$0xf0, %ah
	cmp	$0x90, %ah
	je	fdd

	# hdd
	mov	$6, %ah
	mov	$0x3000, %bx
	mov	%bx, %es
	mov	$0x2000, %bx
	xor	%cx, %cx
	xor	%dx, %dx
	xor	%bp, %bp
	int	$0x1b
	jc	error_hdd

	push	%ax
	mov	%es, %ax
	add	$0x40, %ax
	mov	%ax, %es
	pop	%ax
	push	%es
	push	%bp
	lret

	# fdd
fdd:
	xor	%di, %di
fdd_retry:
	mov	$0xd6, %ah
	mov	$0x3000, %bx
	mov	%bx, %es
	mov	$0x2000, %bx
	mov	$0x0200, %cx
	mov	$0x0001, %dx
	xor	%bp, %bp
	int	$0x1b
	jc	error
	push	%ax
	mov	%es, %ax
	add	$0x40, %ax
	mov	%ax, %es
	pop	%ax
	push	%es
	push	%bp
	lret

error:
	or	%di, %di
	jnz	error_hdd
	and	$0x0f, %al
	or	$0x30, %al
	jmp	fdd_retry
	
error_hdd:
	jmp	error

	.org	0x1fa
	.byte	0			# defflag_off
	.byte	0			# defpart_off
	.byte	1			# menu version
	.byte	0
	.word	0xaa55
