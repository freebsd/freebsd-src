#
# Copyright (c) 1999 Global Technology Associates, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
# OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#	From: btx.s 1.10 1999/02/25 16:27:41 rnordier
# $FreeBSD$
#

# Screen defaults and assumptions.

		.set SCR_MAT,0xe1		# Mode/attribute
		.set SCR_COL,0x50		# Columns per row
		.set SCR_ROW,0x19		# Rows per screen

# BIOS Data Area locations.

		.set BDA_POS,0x53e		# Cursor position

		.globl crt_putchr

# void crt_putchr(int c)

crt_putchr: 	movb 0x4(%esp,1),%al		# Get character
		pusha				# Save
		xorl %ecx,%ecx			# Zero for loops
		movb $SCR_MAT,%ah		# Mode/attribute
		movl $BDA_POS,%ebx		# BDA pointer
		movw (%ebx),%dx 		# Cursor position
		movl $0xa0000,%edi
crt_putchr.1:	cmpb $0xa,%al			# New line?
		je crt_putchr.2			# Yes
		movw %dx,%cx
		movb %al,(%edi,%ecx,1)		# Write char
		addl $0x2000,%ecx
		movb %ah,(%edi,%ecx,1)		# Write attr
		addw $0x02,%dx
		jmp crt_putchr.3
crt_putchr.2:	movw %dx,%ax
		movb $SCR_COL*2,%dl
		div %dl
		incb %al
		mul %dl
		movw %ax,%dx
crt_putchr.3:	cmpw $SCR_ROW*SCR_COL*2,%dx
		jb crt_putchr.4			# No
		leal 2*SCR_COL(%edi),%esi	# New top line
		movw $(SCR_ROW-1)*SCR_COL/2,%cx # Words to move
		rep				# Scroll
		movsl				#  screen
		movb $' ',%al			# Space
		xorb %ah,%ah
		movb $SCR_COL,%cl		# Columns to clear
		rep				# Clear
		stosw				#  line
		movw $(SCR_ROW-1)*SCR_COL*2,%dx
crt_putchr.4:	movw %dx,(%ebx) 		# Update position
		popa				# Restore
		ret				# To caller
