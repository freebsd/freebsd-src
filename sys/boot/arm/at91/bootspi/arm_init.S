/*******************************************************************************
 *
 * Filename: arm_init.s
 *
 * Initialization for C-environment and basic operation.  Adapted from
 *  ATMEL cstartup.s.
 *
 * Revision information:
 *
 * 20AUG2004	kb_admin	initial creation
 * 12JAN2005	kb_admin	updated for 16KB eeprom
 *				Atmel stack prevents loading full size at once
 *
 * BEGIN_KBDD_BLOCK
 * No warranty, expressed or implied, is included with this software.  It is
 * provided "AS IS" and no warranty of any kind including statutory or aspects
 * relating to merchantability or fitness for any purpose is provided.  All
 * intellectual property rights of others is maintained with the respective
 * owners.  This software is not copyrighted and is intended for reference
 * only.
 * END_BLOCK
 *
 * $FreeBSD$
 ******************************************************************************/

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
		.long	((1056 << 17) | (13 << 13) | (12 * 2))
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

	ldr	r1, = SVC_STACK_USE
	mov	sp, r1		@ ; Move the stack to SDRAM

/* Start execution at main					*/

	.extern	main
_main:
__main:
	bl	main

/* main should not return.  If it does, spin forever		*/

infiniteLoop:
	b	infiniteLoop

/* the following section is used to store boot commands in 	*/
/*  non-volatile memory.					*/

	.global BootCommandSection
BootCommandSection:
#ifdef SUPPORT_LINUX
	.string "Bootloader for KB9202 Evaluation Board."
	.string "c 0x20210000 0x10100000 0x80000        "
	.string "m 0 0 0 0 0 0                          "
	.string "t 0x20000100 console=ttyS0,115200 root=/dev/ram rw initrd=0x20210000,654933"
	.string "e 0x10000000                           "
	.string "                 "
#else
#if 0
	.string "Bootloader for KB9202 Evaluation Board."
	.string "m 42 53 44 0 0 1                       "
	.string "ip 206 168 13 194                      "
	.string "server_ip 206 168 13 207               "
	.string "tftp 0x20000000 kernel.bin             "
	.string "e 0x20000000                           "
#else
	.string "m 42 53 44 0 0 1"
	.string "k 0x20000000"
	.string "e 0x20000000"
#endif
	.word	0
#endif
