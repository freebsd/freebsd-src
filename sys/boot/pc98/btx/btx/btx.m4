#
# Copyright (c) 1998 Robert Nordier
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

# $FreeBSD: src/sys/boot/pc98/btx/btx/btx.m4,v 1.2 1999/08/28 00:40:29 peter Exp $

define(_al,0x0)dnl
define(_cl,0x1)dnl
define(_dl,0x2)dnl
define(_bl,0x3)dnl
define(_ah,0x4)dnl
define(_ch,0x5)dnl
define(_dh,0x6)dnl
define(_bh,0x7)dnl

define(_ax,0x0)dnl
define(_cx,0x1)dnl
define(_dx,0x2)dnl
define(_bx,0x3)dnl
define(_sp,0x4)dnl
define(_bp,0x5)dnl
define(_si,0x6)dnl
define(_di,0x7)dnl

define(_bx_si,0x0)dnl
define(_bx_di,0x1)dnl
define(_bp_si,0x2)dnl
define(_bp_di,0x3)dnl
define(_si_,0x4)dnl
define(_di_,0x5)dnl
define(_bp_,0x6)dnl
define(_bx_,0x7)dnl

define(o16,`.byte 0x66')dnl

define(addwia,`.byte 0x5; .word $1')dnl
define(lgdtwm,`.byte 0xf; .byte 0x1; .byte 0x16; .word $1')dnl
define(lidtwm,`.byte 0xf; .byte 0x1; .byte 0x1e; .word $1')dnl
define(cmpwmr,`.byte 0x3b; .byte ($2 << 0x3) | 0x6; .word $1')dnl
define(cmpwir,`.byte 0x81; .byte 0xf8 | $2; .word $1')dnl
define(movbr1,`.byte 0x88; .byte 0x40 | ($1 << 0x3) | $3; .byte $2')dnl
define(movwr0,`.byte 0x89; .byte ($1 << 0x3) | $2')dnl
define(leaw1r,`.byte 0x8d; .byte 0x40 | ($3 << 0x3) | $2; .byte $1')dnl
define(movwir,`.byte 0xb8 | $2; .word $1')dnl
define(movbi1,`.byte 0xc6; .byte 0x40 | $3; .byte $2; .byte $1')dnl
define(callwi,`.byte 0xe8; .word $1 - . - 0x2')dnl
define(jmpfwi,`.byte 0xea; .word $2; .word $1')dnl
define(tstbim,`.byte 0xf6; .byte 0x6; .word $2; .byte $1')dnl
