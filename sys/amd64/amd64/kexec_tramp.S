/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Juniper Networks, Inc.
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

#include <machine/asmacros.h>
#include <machine/specialreg.h>
#include "assym.inc"

/*
 * Take a pointer to the image, copy each segment, and jump to the trampoline.
 *
 * Assumptions:
 * - image is in safe memory
 * - We're already running out of the new "identity" map.
 * - All registers are free game, so go nuts
 * - Interrupts are disabled
 * - All APs are disabled
 */
ENTRY(kexec_do_reboot)
	/*
		r9:	image pointer
		r10:	segment pointer
		r11:	segment counter
	 */
	leaq	kexec_stack(%rip), %rsp
	/* Get the saved kexec_image. */
	leaq	kexec_saved_image(%rip), %r9
	leaq	KEXEC_SEGMENTS(%r9), %r10
	movq	$KEXEC_SEGMENT_MAX, %r11
copy_segment:
	movq	KEXEC_SEGMENT_SIZE(%r10), %rcx
	cmpq	$0, %rcx
	je	done
	shrq	$3, %rcx
	movq	KEXEC_SEGMENT_TARGET(%r10), %rdi
	movq	KEXEC_SEGMENT_MAP(%r10), %rsi
	rep
	movsq
	addq	$KEXEC_STAGED_SEGMENT_SIZE, %r10
	decq	%r11
	jg	copy_segment

done:
	pushq	KEXEC_ENTRY(%r9)
	ret
fail:
	jmp	fail
END(kexec_do_reboot)
ENTRY(kexec_do_reboot_trampoline)
	/* Set new page table, clears most of TLB. */
	movq	%rdi, %cr3

	/* Now flush the rest of the TLB, including global pages. */
	movq	%cr4, %rax
	andq	$~CR4_PGE, %rax
	movq	%rax, %cr4
	jmp	*%rsi
END(kexec_do_reboot_trampoline)
CNAME(kexec_saved_image):
	.globl	kexec_saved_image
	.space	KEXEC_IMAGE_SIZE
	.quad	0
	/* We don't need more than quad, so just fill out the page. */
	.p2align PAGE_SHIFT
	kexec_stack:
CNAME(kexec_do_reboot_size):
	.globl	kexec_do_reboot_size
	.quad . - kexec_do_reboot
