/*
 * Written by Mateusz Guzik <mjg@freebsd.org>
 * Public domain.
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

/*
 * Note: this routine was written with kernel use in mind (read: no simd),
 * it is only present in userspace as a temporary measure until something
 * better gets imported.
 */

#define ALIGN_TEXT      .p2align 4,0x90 /* 16-byte alignment, nop filled */

/*
 * strlen(string)
 *	  %rdi
 *
 * Uses the ((x - 0x01....01) & ~x & 0x80....80) trick.
 *
 * 0x01....01 is replaced with 0x0 - 0x01....01 so that it can be added
 * with leaq.
 *
 * For a description see either:
 * - "Hacker's Delight" by Henry S. Warren, Jr.
 * - "Optimizing subroutines in assembly language: An optimization guide for x86 platforms"
 *   by Agner Fog
 *
 * The latter contains a 32-bit variant of the same algorithm coded in assembly for i386.
 */
ENTRY(strlen)
	movabsq	$0xfefefefefefefeff,%r8
	movabsq	$0x8080808080808080,%r9

	movq	%rdi,%r10
	movq	%rdi,%rcx
	testb	$7,%dil
	jz	2f

	/*
	 * Handle misaligned reads: align to 8 and fill
	 * the spurious bytes.
	 */
	andq	$~7,%rdi
	movq	(%rdi),%r11
	shlq	$3,%rcx
	movq	$-1,%rdx
	shlq	%cl,%rdx
	notq	%rdx
	orq	%rdx,%r11

	leaq	(%r11,%r8),%rcx
	notq	%r11
	andq	%r11,%rcx
	andq	%r9,%rcx
	jnz	3f

	/*
	 * Main loop.
	 */
	ALIGN_TEXT
1:
	leaq	8(%rdi),%rdi
2:
	movq	(%rdi),%r11
	leaq	(%r11,%r8),%rcx
	notq	%r11
	andq	%r11,%rcx
	andq	%r9,%rcx
	jz	1b
3:
	bsfq	%rcx,%rcx
	shrq	$3,%rcx
	leaq	(%rcx,%rdi),%rax
	subq	%r10,%rax
	ret
END(strlen)

	.section .note.GNU-stack,"",%progbits
