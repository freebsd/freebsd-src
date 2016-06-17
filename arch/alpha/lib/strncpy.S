/*
 * arch/alpha/lib/strncpy.S
 * Contributed by Richard Henderson (rth@tamu.edu)
 *
 * Copy no more than COUNT bytes of the null-terminated string from
 * SRC to DST.  If SRC does not cover all of COUNT, the balance is
 * zeroed.
 *
 * Or, rather, if the kernel cared about that weird ANSI quirk.  This
 * version has cropped that bit o' nastiness as well as assuming that
 * __stxncpy is in range of a branch.
 */

	.set noat
	.set noreorder

	.text

	.align 3
	.globl strncpy
	.ent strncpy
strncpy:
	.frame $30, 0, $26
	.prologue 0

	mov	$16, $0		# set return value now
	beq	$18, 0f
	mov	$26, $23	# set return address
	br	__stxncpy	# do the work of the copy

0:	ret
	nop
	nop
	nop

	.end strncpy
