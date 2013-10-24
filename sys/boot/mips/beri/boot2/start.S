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

.set mips64
.set noreorder
.set nobopt
.set noat

/*
 * Save arguments from the BERI firmware for use in C-land, and jump into
 * main.  Assume that registers/stack/etc are sufficiently initialised to get
 * going.
 */


		.text
		.global start
		.ent start
start:
		/*
		 * Relocate boot2 to DRAM where we can write back global
		 * variable values; jump to it.  Assume all values are 64-bit
		 * aligned.  Use an inline PIC version of memcpy()
		 * pre-relocation.
		 */
		dla	$t0, __boot2_base_vaddr__		/* Dst. */
		dla	$t1, __cheri_flash_boot_loader_vaddr__	/* Src. */
		dla	$t2, __boot_loader_len__		/* Len. */

memcpy_loop:
		beq	$t2, 0, memcopy_done
		nop
		ld	$at, 0($t1)
		sd	$at, 0($t0)
		daddiu	$t0, 8
		daddiu	$t1, 8
		daddi	$t2, -8
		b	memcpy_loop
		nop

memcopy_done:
		dla	$at, beri_argc
		sd	$a0, 0($at)
		dla	$at, beri_argv
		sd	$a1, 0($at)
		dla	$at, beri_envv
		sd	$a2, 0($at)
		dla	$at, beri_memsize
		sd	$a3, 0($at)

		/*
		 * We can now jump into the relocated code, running from
		 * cached DRAM rather than uncached flash.
		 */
		dla	$at, relocated_start
		jr	$at
		nop

relocated_start:
		/*
		 * Zero BSS.  Run from cached memory as this will speed up
		 * code execution noticeably.  Assuming 64-bit alignment of
		 * everything here.
		 */
		dla	$t0, __bss_start
		dla	$t1, __bss_end

bss_loop:
		beq	$t0, $t1, bss_done
		nop
		sd	$zero, 0($t0)
		daddiu	$t0, 8
		b	bss_loop
		nop

bss_done:
		jal	main
		nop

		/*
		 * Ideally we wouldn't get here, but just in case.
		 */
loop:
		b	loop
		nop
		.end start

		/*
		 * Globals exported to boot2.
		 */
		.data
		.global beri_argc
beri_argc:	.dword	0x0000000000000000
		.global beri_argv
beri_argv:	.dword	0x0000000000000000
		.global beri_envv
beri_envv:	.dword	0x0000000000000000
		.global	beri_memsize
beri_memsize:	.dword	0x0000000000000000
