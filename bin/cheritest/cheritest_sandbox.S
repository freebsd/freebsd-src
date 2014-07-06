/*-
 * Copyright (c) 2013 Robert N. M. Watson
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
        /* Move $c0 to $idc so that it will be saved by CCall. */
        cmove $c26, $c0

        /* Clear $c0 so it's not available to sandbox. */
        ccleartag $c0, $c0

        /* Invoke object capability. */
        ccall $c1, $c2

        /* Set $c0 back to stored $idc. */
        cmove $c0, $c26

	jalr $ra
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
