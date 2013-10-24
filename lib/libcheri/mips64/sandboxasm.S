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

.set mips64
.set noreorder
.set nobopt
.set noat

#
# Caller side of the CHERI sandbox runtime startup code -- set up arguments
# for a CCall into a sandbox.
#
# ABI conventions; *'d items implemented here:
#    $c1 contains to-be-invoked sealed code capability *
#    $c2 contains to-be-invoked sealed data capability *
#    $c3-$c10 caller-save argument registers
#    $c3 return value register
#    $c11-$c16 caller-save registers
#    $c17-$c24 callee-save registers
#    $c25 unused *
#    $c26 IDC *
#
#    $a0-$a5 will be filled with general-purpose arguments
#
# XXXRW: No floating-point support.
#

	.text
	.global _chsbrt_invoke
	.ent	_chsbrt_invoke
_chsbrt_invoke:

	#
	# Save caller-save general-purpose registers.
	#
	# XXXRW: Check this -- and also other caller-save registers?
	#
	# XXXRW: Save caller-save capability registers.
	#
	daddiu	$sp, -96
	sd	$gp, 88($sp)
	sd	$ra, 80($sp)
	sd	$s8, 72($sp)
	sd	$s7, 64($sp)
	sd	$s6, 56($sp)
	sd	$s5, 48($sp)
	sd	$s4, 40($sp)
	sd	$s3, 32($sp)
	sd	$s2, 24($sp)
	sd	$s1, 16($sp)
	sd	$s0, 8($sp)
	sd	$fp, 0($sp)

	#
	# XXXRW: When CCall, scrub general-purpose and capability registers
	# here.
	#

	# Save $c0 in IDC for use after return.
	cmove	$c26, $c0

	# Clear $c0 so it is not available to the sanebox.
	ccleartag	$c0

	# Invoke object capability.
	ccall	$c1, $c2

	# Set $c0 back to restored $idc.
	cmove	$c0, $c26

	# Use CCall-restored $sp to restore other registers of note.
	ld	$fp, 0($sp)
	ld	$s0, 8($sp)
	ld	$s1, 16($sp)
	ld	$s2, 24($sp)
	ld	$s3, 32($sp)
	ld	$s4, 40($sp)
	ld	$s5, 48($sp)
	ld	$s6, 56($sp)
	ld	$s7, 64($sp)
	ld	$s8, 72($sp)
	ld	$ra, 80($sp)
	ld	$gp, 88($sp)
	daddiu	$sp, 96

	#
	# XXXRW: Restore caller-save capability registers.
	#
	# XXXRW: Defensively clear other general-purpose, capability registers
	# on return?
	#

	#
	# Return to C-language caller.
	#
	jr	$ra
	nop				# Branch-delay slot

	.end	_chsbrt_invoke
