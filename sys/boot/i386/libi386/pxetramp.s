#
# Copyright (c) 2000 Peter Wemm
# All rights reserved.
#
# Redistribution and use in source and binary forms are freely
# permitted provided that the above copyright notice and this
# paragraph and the following disclaimer are duplicated in all
# such forms.
#
# This software is provided "AS IS" and without any express or
# implied warranties, including, without limitation, the implied
# warranties of merchantability and fitness for a particular
# purpose.
#
# $FreeBSD$

# ph33r this

		.globl  __h0h0magic, __pxeseg, __pxeoff

		.code16
		.p2align 4,0x90
__h0h0magic:
		push    %dx			# seg:data
		push    %ax			# off:data
		push    %bx			# int16 func
		.byte   0x9a			# far call
__pxeoff:	.word   0x0000			# offset
__pxeseg:	.word   0x0000			# segment
		add $6, %sp			# restore stack
		.byte 0xcb			# to vm86int
