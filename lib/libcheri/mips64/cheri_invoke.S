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
# Assembly wrapper for CCall on an object-capability.  Its function is to save
# and restore any general-purpose and capability registers needed on either
# side of CCall, but not handled by the compiler.  This is done by creating an
# on-stack frame which will be pointed to by $idc before CCall, and then
# unwrapping it again.  We rely on the compiler and CCall to sort out clearing
# of registers that require it, since they have the information to do so.
#
# Calling conventions into cheri_invoke:
#
# Registers	Description
# $c0		MIPS address space
# $c1, $c2	Invoked capabilities
# $c3..$c10	Argument capabilities
# $c11..$c16	Caller-save capabilities
# $c17..$c24	Callee-save capabilities
# $c26		IDC
#
# Calling conventions implemented around CCall:
#
# $c1		Invoked code capability
# $c2		Invoked data capability
# $c3..$c10	Argument capabilities; $c3 as return capability
# $c11..$c16	n/a
# $c17..$c24	n/a
# $c26		IDC
#
# XXXRW: Worries/TODO:
#
# - Floating-point registers.
# - The compiler needs to handle unused argument/return registers.
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
	# XXXRW: Possibly with the __ccall calling convention, the compiler
	# should be doing this?
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
	# XXXRW: Possibly with the __ccall calling convention, the compiler
	# should be doing this?
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
	# The compiler is responsible for scrubbing unused argument registers
	# (since only it has the information required to do so).  CCall is
	# responsible for scrubbing all other registers.
	#

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
	# XXXRW: Possibly with the __ccall calling convention, the compiler
	# should be doing this?
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
	# CCall has conservatively cleared all non-return-value registers, and
	# so we don't need to.

	#
	# Restore general-purpose registers from the stack.
	#
	# XXXRW: Possibly with the __ccall calling convention, the compiler
	# should be doing this?
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
	# Return to C-language caller.
	#
	jr	$ra
	nop				# Branch-delay slot

	.end	cheri_invoke
