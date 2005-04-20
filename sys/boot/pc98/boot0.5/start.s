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
	.global	start
	.code16

	.text
start:
	jmp	start1

	.org	0x2d4
start1:
	cli
	movw	%cs, %ax
	movw	%ax, %ds
	movw	%ss, iniss
	movw	%sp, inisp
	movw	%ax, %ss
	movw	$0xfffe, %sp
	sti	
	xorw	%ax, %ax
	movw	%ax, %es
	call	main

	cli
	movw	%cs:iniss, %ss
	movw	%cs:inisp, %sp
	sti
	int	$0x1e
	# NOTREACHED
	lret

	.data
	.global	iniss, inisp
iniss:	.word	0
inisp:	.word	0
