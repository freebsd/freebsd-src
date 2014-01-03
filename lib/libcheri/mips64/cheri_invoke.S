#-
# Copyright (c) 2012-2013 Robert N. M. Watson
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
#

	.text
	.global cheri_invoke
	.ent	cheri_invoke
cheri_invoke:

	#
	# Save caller-save general-purpose registers.
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
	# Save $c0 where CCall will preserve it, and CReturn will restore it.
	#
	# XXXRW: Arguably $c0's value should already be there, so we restore
	# it after CCall returns, but shouldn't need to save it here.
	#
	cmove	$c26, $c0

	#
	# Save capability registers we later need to restore (that won't be
	# handled by CCall for us).
	#
	daddiu	$sp, -256
	csc	$c17, $sp, 0($c0)
	csc	$c18, $sp, 32($c0)
	csc	$c19, $sp, 64($c0)
	csc	$c20, $sp, 96($c0)
	csc	$c21, $sp, 128($c0)
	csc	$c22, $sp, 160($c0)
	csc	$c23, $sp, 192($c0)
	csc	$c24, $sp, 224($c0)

	#
	# Scrub caller-save non-argument general-purpose registers.
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
	li	$ra, 0

	#
	# Scrub unpreserved general-purpose registers: $at, $v0, $v1,
	# temporaries, and exception-handling registers.
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

	#
	# Scrub non-{invoked, argument} capabilities; we do this after
	# general-purpose registers as clearing $c0 will disable MIPS memory
	# instructions.
	#
	ccleartag	$c0
	ccleartag	$c11
	ccleartag	$c12
	ccleartag	$c13
	ccleartag	$c14
	ccleartag	$c15
	ccleartag	$c16
	ccleartag	$c17
	ccleartag	$c18
	ccleartag	$c19
	ccleartag	$c20
	ccleartag	$c21
	ccleartag	$c22
	ccleartag	$c23
	ccleartag	$c24

	#
	# Invoke object capability.  CCall/CReturn will save and restore $sp
	# and IDC, so we don't need to.
	#
	ccall	$c1, $c2

	#
	# CReturn restores IDC; propagate to $c0
	#
	cmove	$c0, $c26

	#
	# Restore capability registers from stack.
	#
	clc	$c17, $sp, 0($c0)
	clc	$c18, $sp, 32($c0)
	clc	$c19, $sp, 64($c0)
	clc	$c20, $sp, 96($c0)
	clc	$c21, $sp, 128($c0)
	clc	$c22, $sp, 160($c0)
	clc	$c23, $sp, 192($c0)
	clc	$c24, $sp, 224($c0)
	daddiu	$sp, 256

	#
	# Conservative: clear capability registers that aren't return values
	# and haven't been restored, to prevent accidental use.  Could save on
	# cycles by not doing this, but have to trust the compiler more.
	#
	ccleartag	$c1
	ccleartag	$c2
	ccleartag	$c3
	ccleartag	$c4
	ccleartag	$c5
	ccleartag	$c6
	ccleartag	$c7
	ccleartag	$c8
	ccleartag	$c9
	ccleartag	$c10
	ccleartag	$c11
	ccleartag	$c12
	ccleartag	$c13
	ccleartag	$c14
	ccleartag	$c15
	ccleartag	$c16

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

	#
	# Return to C-language caller.
	#
	jr	$ra
	nop				# Branch-delay slot

	.end	cheri_invoke
