/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software is derived from code provided by Kwikbyte with the
 * following information:
 *
 * Initialization for C-environment and basic operation.  Adapted from
 * ATMEL cstartup.s.
 *
 * No warranty, expressed or implied, is included with this software.  It is
 * provided "AS IS" and no warranty of any kind including statutory or aspects
 * relating to merchantability or fitness for any purpose is provided.  All
 * intellectual property rights of others is maintained with the respective
 * owners.  This software is not copyrighted and is intended for reference
 * only.
 *
 * $FreeBSD$
 */

	.equ	ARM_MODE_USER,	0x10
	.equ	ARM_MODE_FIQ,	0x11
	.equ	ARM_MODE_IRQ,	0x12
	.equ	ARM_MODE_SVC,	0x13
	.equ	ARM_MODE_ABORT,	0x17
	.equ	ARM_MODE_UNDEF,	0x1B
	.equ	ARM_MODE_SYS,	0x1F

	.equ	I_BIT,	0x80
	.equ	F_BIT,	0x40
	.equ	T_BIT,	0x20

/*
 * Stack definitions
 *
 * Start near top of internal RAM.
 */

	.equ	END_INT_SRAM,		0x4000
	.equ	SVC_STACK_START,	(END_INT_SRAM - 0x4)
	.equ	SVC_STACK_USE,		0x21800000

start:

/* vectors - must reside at address 0			*/
/* the format of this table is defined in the datasheet	*/
                B           InitReset       	@; reset
undefvec:
                B           undefvec        	@; Undefined Instruction
swivec:
                B           swivec          	@; Software Interrupt
pabtvec:
                B           pabtvec         	@; Prefetch Abort
dabtvec:
                B           dabtvec         	@; Data Abort
rsvdvec:
		B	    rsvdvec
irqvec:
                ldr         pc, [pc,#-0xF20]    @; IRQ : read the AIC
fiqvec:
                B           fiqvec          	@; FIQ


InitReset:

/* Set stack and init for SVC				*/
	ldr	r1, = SVC_STACK_START
	mov	sp, r1		@; Init stack SYS

	msr     cpsr_c, #(ARM_MODE_SVC | I_BIT | F_BIT)
	mov     sp, r1		@ ; Init stack SYS

/* Perform system initialization				*/

	.extern	_init
	bl	_init

/* Start execution at main					*/

	.extern	main
	bl	main

/* main should not return.  If it does, spin forever		*/

infiniteLoop:
	b	infiniteLoop
