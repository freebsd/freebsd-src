/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994 - 1999 by Ralf Baechle
 * Copyright (C) 1999 Silicon Graphics
 *
 * Low level exception handling
 */
#include <asm/asm.h>
#include <asm/regdef.h>
#include <asm/fpregdef.h>
#include <asm/mipsregs.h>
#include <asm/stackframe.h>

	.macro	__build_clear_none
	.endm

	.macro	__build_clear_sti
	STI
	.endm

	.macro	__build_clear_cli
	CLI
	.endm

	.macro	__build_clear_fpe
	cfc1	a1, fcr31
	li	a2, ~(0x3f << 12)
	and	a2, a1
	ctc1	a2, fcr31
	STI
	.endm

	.macro	__build_clear_ade
	dmfc0	t0, CP0_BADVADDR
	sd	t0, PT_BVADDR(sp)
	KMODE
	.endm

	.macro	__BUILD_silent exception
	.endm

	/* Gas tries to parse the PRINT argument as a string containing
	   string escapes and emits bogus warnings if it believes to
	   recognize an unknown escape code.  So make the arguments
	   start with an n and gas will believe \n is ok ...  */
	.macro	__BUILD_verbose	nexception
	ld	a1, PT_EPC(sp)
	PRINT("Got \nexception at %016lx\012")
	.endm

	.macro	__BUILD_count exception
	.set	reorder
	ld	t0,exception_count_\exception
	daddiu	t0, 1
	sd	t0,exception_count_\exception
	.set	noreorder
	.comm	exception_count\exception, 8, 8
	.endm

	.macro	BUILD_HANDLER exception handler clear verbose
	.align	5
	NESTED(handle_\exception, PT_SIZE, sp)
	.set	noat
	SAVE_ALL
	__BUILD_clear_\clear
	.set	at
	__BUILD_\verbose \exception
	move	a0, sp
	jal	do_\handler
	j	ret_from_exception
	 nop
	END(handle_\exception)
	.endm
