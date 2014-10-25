#-
# Copyright (c) 2012-2014 Robert N. M. Watson
# All rights reserved.
#
# This software was developed by SRI International and the University of
# Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
# ("CTSRD"), as part of the DARPA CRASH research programme.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

#.set mips64
.set noreorder
#.set nobopt
.set noat

#
# Assembly wrapper for CCall on an object-capability.  Its function is to
# save, clear, and restore general-purpose and capability registers as needed
# on either side of CCall.  $c0/$sp assumed to provide a valid stack, and the
# caller must have set up $c1, $c2, and general-purpose/capability argument
# registers before calling this function.
#
# Calling conventions into cheri_invoke:
#
# Registers	Description			Before, After
# $c0		MIPS address space		Save/Clear, Restore
# $c1, $c2	Invoked capabilities		Pass through, Clear
# $c3..$c10	Argument capabilities		Pass through, Clear
# $c11..$c16	Caller-save capabilities	Clear, Clear
# $c17..$c24	Callee-save capabilities	Save/Clear, Restore
# $c26		IDC				Handled by CCall
#
# Calling conventions implemented around CCall:
#
# $c1		Invoked code capability
# $c2		Invoked data capability
# $c3..$c10	Argument capabilities
# $c11..$c16	n/a
# $c17..$c24	n/a
# $c26		IDC
#
# XXXRW: Worries/TODO:
#
# - Floating-point registers.
# - Unused argument registers (capability and general-purpose) -- only the
#   caller knows which are used, and hence which need to be scrubbed.
# - Check that CCall scrubs $sp in callee.
# - What about $c25?  Should we swap $c25 and $c26 in the ISA definition?
# - Easy to imagine a #ifdef FAST_BUT_LOOSE that does less clearing but trusts
#   the compiler more.  Would also be nice to have a CClearMask that took a
#   bitmask of capability registers to flush -- and likeways ClearMask for
#   MIPS registers.
# - Not yet using $c11 for stack access, so we depend on $c0 and $c11 being
#   the same.  At least we save/restore $c11?
#

	.text
	.global cheri_invoke
	.ent	cheri_invoke
cheri_invoke:

	#
	# Wrap up all caller-save state suitable to be preseved by CCall and
	# restored by CReturn.  This happens in two phases:
	#
	# 1. First, use the conventional stack to save as many caller-save
	# general-purpose and capability registers as possible.
	#
	# 2. Produce a value for $idc that bundles these up suitable to
	# bootstrap trestoration.  This will save the original $idc, $csp,
	# $sp, and $c0.
	#
	# Then after CReturn, reverse these steps by first unwrapping $idc,
	# and then performing a more normal function return.
	#
	# The caller $idc will describe a stack fragment, which gives us a bit
	# of space to store useful things, such as $sp, that are otherwise
	# quite hard to restore (for obvious reasons).
	#
	# Save callee-save general-purpose registers.
	#
	# Caller-save registers are: $s0..$s7, $gp, $sp, $s8 ($fp).
	#
	# Do also save $ra so that we can return properly.
	#
	# Don't save $sp as CCall will do that for us.
	#
	# NB: Use 96 rather than 88 for the stack to ensure 32-byte alignment
	# for capabilities stored and loaded from it later.
	#
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

	#
	# Save capability registers we later need to restore (that won't be
	# handled by CCall for us).
	#
	daddiu	$sp, -256
	csc	$c17, $sp, 0($c11)
	csc	$c18, $sp, 32($c11)
	csc	$c19, $sp, 64($c11)
	csc	$c20, $sp, 96($c11)
	csc	$c21, $sp, 128($c11)
	csc	$c22, $sp, 160($c11)
	csc	$c23, $sp, 192($c11)
	csc	$c24, $sp, 224($c11)

	#
	# Prepare $idc to be saved (and restored) by CCall; don't need to seal
	# as CCall will protect its integrity.  No more stack use after this
	# point until $c11 and $sp are restored.
	#
	daddiu		$sp, -128
	csd		$sp, $sp, 96($c11)	# Save MIPS stack pointer
	csc		$c0, $sp, 64($c11)	# Save CHERI legacy capability
	csc		$c11, $sp, 32($c11)	# Save CHERI stack capability
	csc		$c26, $sp, 0($c11)	# Save original $idc
	cfromptr	$c26, $c11, $sp
	dli		$t0, 128
	csetlen		$c26, $c26, $t0

	#
	# Scrub callee-save non-argument general-purpose registers.
	#
	# Don't scrub $sp as CCall will do that for us ... right?
	#
	li	$s0, 0
	li	$s1, 0
	li	$s2, 0
	li	$s3, 0
	li	$s4, 0
	li	$s5, 0
	li	$s6, 0
	li	$s7, 0
	li	$gp, 0
	li	$fp, 0
	li	$sp, 0
	li	$ra, 0

	#
	# Scrub unpreserved general-purpose registers: $at, $v0, $v1,
	# temporaries, hi/lo, and exception-handling registers.
	#
	li	$at, 0
	li	$v0, 0
	li	$v1, 0
	li	$t0, 0
	li	$t1, 0
	li	$t2, 0
	li	$t3, 0
	li	$t8, 0
	li	$t9, 0
	li	$k0, 0		# Is this really necessary?
	li	$k1, 0		# Is this really necessary?
	mtlo	$0
	mthi    $0

	#
	# Scrub non-{invoked, argument} capabilities; we do this after
	# general-purpose registers as clearing $c0 will disable MIPS memory
	# instructions.
	#
	# XXXRW: For information-leakage reasons, we'd rather install a null
	# capability?
	#
	ccleartag	$c0, $c0
	ccleartag	$c11, $c11
	ccleartag	$c12, $c12
	ccleartag	$c13, $c13
	ccleartag	$c14, $c14
	ccleartag	$c15, $c15
	ccleartag	$c16, $c16
	ccleartag	$c17, $c17
	ccleartag	$c18, $c18
	ccleartag	$c19, $c19
	ccleartag	$c20, $c20
	ccleartag	$c21, $c21
	ccleartag	$c22, $c22
	ccleartag	$c23, $c23
	ccleartag	$c24, $c24

	#
	# Invoke object capability.  CCall/CReturn will save and restore $sp
	# and IDC, so we don't need to.
	#
	ccall	$c1, $c2

	#
	# Restore $idc-saved recovery state; $idc must be restored last.  The
	# stack is usable again after this.
	#
	cld		$sp, $zero, 96($c26)	# Restore MIPS stack pointer
	clc		$c0, $zero, 64($c26)	# Restore CHERI $c0
	clc		$c11, $zero, 32($c26)	# Restore CHERI $c11
	clc		$c26, $zero, 0($c26)	# Restore original $idc
	daddiu		$sp, 128

	#
	# Restore capability registers from stack.
	#
	clc	$c17, $sp, 0($c11)
	clc	$c18, $sp, 32($c11)
	clc	$c19, $sp, 64($c11)
	clc	$c20, $sp, 96($c11)
	clc	$c21, $sp, 128($c11)
	clc	$c22, $sp, 160($c11)
	clc	$c23, $sp, 192($c11)
	clc	$c24, $sp, 224($c11)
	daddiu	$sp, 256

	#
	# Conservative: clear capability registers that aren't return values
	# and haven't been restored, to prevent accidental use.  Could save on
	# cycles by not doing this, but have to trust the compiler more.
	#
	# NB: $c11 intentionally skipped, as we restored it earlier.
	#
	ccleartag	$c1, $c1
	ccleartag	$c2, $c2
	ccleartag	$c3, $c3
	ccleartag	$c4, $c4
	ccleartag	$c5, $c5
	ccleartag	$c6, $c6
	ccleartag	$c7, $c7
	ccleartag	$c8, $c8
	ccleartag	$c9, $c9
	ccleartag	$c10, $c10
	ccleartag	$c12, $c12
	ccleartag	$c13, $c13
	ccleartag	$c14, $c14
	ccleartag	$c15, $c15
	ccleartag	$c16, $c16

	#
	# Restore general-purpose registers from the stack.
	#
	ld	$s0, 0($sp)
	ld	$s1, 8($sp)
	ld	$s2, 16($sp)
	ld	$s3, 24($sp)
	ld	$s4, 32($sp)
	ld	$s5, 40($sp)
	ld	$s6, 48($sp)
	ld	$s7, 56($sp)
	ld	$gp, 64($sp)
	ld	$fp, 72($sp)
	ld	$ra, 80($sp)
	daddiu	$sp, 96

	#
	# Conservative: clear general-purpose registers that aren't return
	# values and haven't been restored, to prevent accidental use.  This
	# includes argument registers, whose values we can no longer trust.
	# Could save on cycles by not doing this, but have to trust the
	# compiler more.
	#
	li	$at, 0
	li	$a0, 0
	li	$a1, 0
	li	$a2, 0
	li	$a3, 0
	li	$a4, 0
	li	$a5, 0
	li	$a6, 0
	li	$a7, 0
	li	$t0, 0
	li	$t1, 0
	li	$t2, 0
	li	$t3, 0
	li	$t8, 0
	li	$t9, 0
	li	$k0, 0		# Is this really necessary?
	li	$k1, 0		# Is this really necessary?
	mtlo	$0
	mthi	$0

	#
	# Return to C-language caller.
	#
	jr	$ra
	nop				# Branch-delay slot

	.end	cheri_invoke
