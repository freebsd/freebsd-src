#
# Copyright (c) 1999 Robert Nordier
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

# $FreeBSD: src/sys/boot/i386/mbr/mbr.m4,v 1.2 1999/08/28 00:40:20 peter Exp $

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

define(cmpbr0,`.byte 0x38; .byte ($1 << 0x3) | $2')dnl
define(cmpwi2,`.byte 0x81; .byte 0xb8 | $3; .word $2; .word $1')dnl
define(movb0r,`.byte 0x8a; .byte ($2 << 0x3) | $1')dnl
define(movb1r,`.byte 0x8a; .byte 0x40 | ($3 << 0x3) | $2; .byte $1')dnl
define(movw1r,`.byte 0x8b; .byte 0x40 | ($3 << 0x3) | $2; .byte $1')dnl
define(movwir,`.byte 0xb8 | $2; .word $1')dnl
define(jmpnwi,`.byte 0xe9; .word $1 - . - 0x2')dnl
