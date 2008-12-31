# Copyright (c) KATO Takenori, 2007.
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
# $FreeBSD: src/sys/boot/pc98/boot0.5/putssjis.s,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $
#

	.code16
	.section	.putssjis, "awx", @progbits

	#
	# Display string with Shift-JIS support
	# %si: addres of string, %di: T-VRAM address, %cx: count
	#

	# Absolute address of putssjis_entry must be 0x1243.
putssjis_entry:
	push	%es
	push	%ax
	# Setup the T-VRAM segement address.
	xorw	%ax, %ax
	movw	%ax, %es
	movw	$0xa000, %ax
	testb	$0x08, %es:0x501
	jz	normalmode
	movw	$0xe000, %ax
normalmode:
	movw	%ax, %es

putssjis_loop:
	lodsw
	call	check_sjis
	jc	put_2byte_char

	# 1 byte character
	xorb	%ah, %ah
	testb	$0xe0, %al	# Check control code.
	jnz	put_1byte_char
	movb	$0x20, %al	# Convert control code into the space.
put_1byte_char:
	stosw
	decw	%si
	jmp	putssjis_loop_end

put_2byte_char:
	subb	$0x20, %al

	# Check 2byte "hankaku"
	cmp	$0x09, %al
	je	put_2byte_hankaku
	cmp	$0x0a, %al
	je	put_2byte_hankaku
	cmp	$0x0b, %al
	je	put_2byte_hankaku
	jmp	put_2byte_zenkaku

put_2byte_hankaku:
	stosw
	jmp	putssjis_loop_end

put_2byte_zenkaku:
	stosw
	orb	$0x80, %ah
	stosw
	decw	%cx

putssjis_loop_end:
	loop	putssjis_loop

	pop	%ax
	pop	%es
	ret

	# Check 2-byte code.
check_sjis:
	cmpb	$0x80, %al
	jbe	found_ank_kana
	cmpb	$0xa0, %al
	jb	found_2byte_char
	cmpb	$0xe0, %al
	jb	found_ank_kana
	cmpb	$0xf0, %al
	jae	found_ank_kana
	jmp	found_2byte_char
found_ank_kana:
	clc
	ret

found_2byte_char:
	# Convert Shift-JIS into JIS.
	cmpb	$0x9f, %al
	ja	sjis_h_2		# Upper > 0x9f
	subb	$0x71, %al		# Upper -= 0x71
	jmp	sjis_lower
sjis_h_2:
	subb	$0xb1, %al		# Upper -= 0xb1
sjis_lower:
	salb	%al			# Upper *= 2
	incb	%al			# Upper += 1

	cmpb	$0x7f, %ah
	jbe	sjis_l_2
	decb	%ah			# Lower -= 1 if lower > 0x7f
sjis_l_2:
	cmpb	$0x9e, %ah
	jb	sjis_l_3
	subb	$0x7d, %ah		# Lower -= 0x7d
	incb	%al			# Upper += 1
	jmp	check_2byte_end
sjis_l_3:
	subb	$0x1f, %ah		# Lower -= 0x1f
check_2byte_end:
	stc
	ret
