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
# Wait 1ms
#
	.global	wait1ms
wait1ms:
	push	%cx
	movw	$800, %cx
wait_loop:
	outb	%al, $0x5f
	loop	wait_loop
	pop	%cx
	ret

#
# Read one byte from BIOS parameter block
#	%bx	offset
#	%dl	value
#
	.global	read_biosparam
read_biosparam:
	movb	%es:(%bx), %dl
	ret

#
# Write one byte to BIOS paramter block
#	%bx	offset
#	%dl	value
#
	.global	write_biosparam
write_biosparam:
	movb	%dl, %es:(%bx)
	ret

#
# beep
#
	.global	beep_on, beep_off, beep
beep_on:
	movb	$0x17, %ah
	int	$0x18
	ret

beep_off:
	movb	$0x18, %ah
	int	$0x18
	ret

beep:
	push	%cx
	call	beep_on
	movw	$100, %cx
beep_loop1:
	call	wait1ms
	loop	beep_loop1
	call	beep_off
	movw	$50, %cx
beep_loop2:
	call	wait1ms
	loop	beep_loop2
	pop	%cx
	ret
