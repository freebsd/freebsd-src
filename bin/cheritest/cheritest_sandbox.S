/*-
 * Copyright (c) 2013-2014 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

	.set noreorder	/* Preserve nops, allow instructions in b-d slots. */
	.set noat	/* Don't use $at automatically. */

/*
 * CCall path; inline assembly works fine if we don't have compiler support,
 * but with compiler support we want to use the right calling convention,
 * which requires a complete function with safe internal register use.
 */
	.global cheritest_ccall
	.ent cheritest_ccall
cheritest_ccall:

	/* Save callee-save registers. */
	daddiu	$sp, -96
	sd	$s0, 0($sp)
	sd	$s1, 8($sp)
	sd	$s2, 16($sp)
	sd	$s3, 24($sp)
	sd	$s4, 32($sp)
	sd	$s5, 40($sp)
	sd	$s6, 48($sp)
	sd	$s7, 56($sp)
	sd	$gp, 64($sp)
	sd	$fp, 72($sp)
	sd	$ra, 80($sp)

        daddiu  $sp, -256
        csc     $c17, $sp, 0($c11)
        csc     $c18, $sp, 32($c11)
        csc     $c19, $sp, 64($c11)
        csc     $c20, $sp, 96($c11)
        csc     $c21, $sp, 128($c11)
        csc     $c22, $sp, 160($c11)
        csc     $c23, $sp, 192($c11)
        csc     $c24, $sp, 224($c11)

	/* Move $c0, with an offset of $sp, into $idc to be saved by CCall. */
	csetoffset	$c26, $c0, $sp

        /* Invoke object capability. */
        ccall	$c1, $c2

	/* Restore $c0 and $sp from $idc.  Make sure to reset $c26's offset. */
	cgetoffset	$sp, $c26
	csetoffset	$c26, $c26, $zero
	cmove		$c0, $c26

	/* Also use $idc for $c11. */
	cmove	$c11, $c26

	/* Restore callee-save registers. */
        clc     $c17, $sp, 0($c11)
        clc     $c18, $sp, 32($c11)
        clc     $c19, $sp, 64($c11)
        clc     $c20, $sp, 96($c11)
        clc     $c21, $sp, 128($c11)
        clc     $c22, $sp, 160($c11)
        clc     $c23, $sp, 192($c11)
        clc     $c24, $sp, 224($c11)
        daddiu  $sp, 256

        ld      $s0, 0($sp)
        ld      $s1, 8($sp)
        ld      $s2, 16($sp)
        ld      $s3, 24($sp)
        ld      $s4, 32($sp)
        ld      $s5, 40($sp)
        ld      $s6, 48($sp)
        ld      $s7, 56($sp)
        ld      $gp, 64($sp)
        ld      $fp, 72($sp)
        ld      $ra, 80($sp)
        daddiu  $sp, 96

	jalr	$ra
	nop
	.end cheritest_ccall

/*
 * Landing code within a sandbox.  Various versions with varying levels of
 * technical and moral complexity.
 */

	/*
	 * Sandbox code that simply performs creturn immediately.
	 * Position-independent, no memory access.
	 */
	.text
	.global sandbox_creturn
	.ent sandbox_creturn
sandbox_creturn:

	creturn
	.end sandbox_creturn

	.global sandbox_creturn_end
sandbox_creturn_end:


	/*
	 * Sandbox code that performs a series of nops before creturn.
	 * Position-independent, no memory access.
	 */
	.text
	.global sandbox_nop_creturn
	.ent sandbox_nop_creturn
sandbox_nop_creturn:

	nop
	nop
	nop
	creturn
	.end sandbox_nop_creturn

	.global sandbox_nop_creturn_end
sandbox_nop_creturn_end:


	/*
	 * Sandbox code that loads a well-known constant into its return
	 * register before returning.
	 */
	.text
	.global sandbox_dli_creturn
	.ent sandbox_dli_creturn
sandbox_dli_creturn:

	dli	$v0, 0x1234
	creturn
	.end sandbox_dli_creturn

	.global sandbox_dli_creturn_end
sandbox_dli_creturn_end:
