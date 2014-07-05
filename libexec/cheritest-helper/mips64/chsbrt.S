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

#include <machine/cherireg.h>

#.set mips64
.set noreorder
#.set nobopt
.set noat

#
# CHERI sandbox runtime startup code -- begins execution inside a sandbox and
# is responsible for bootstrapping MIPS-compiled C code (without CHERI
# extensions).
#
# Temporary ABI conventions:
#    $sp contains a pointer to the top of the stack; 32-byte aligned
#    $fp contains a pointer to the top of the stack; 32-byte aligned
#
#    $a0-$a7 contain user arguments
#    $v0, $v1 contain user return values
#
#    $c0, $pcc contain access to (100% overlapped) sandbox code and data
#
#    $c1, $c2 contain the invoked object capability
#    $c3-$c10 contain user capability arguments
#    $c11 contains the stack-segment capability -- for now, equal to $c0
#
#    $c26 contains the invoked data capability installed by CCall; the
#      complete segment, which we will install into $c0 to allow MIPS code to
#      access it.
#
# Sandbox heap information is extracted from the sandbox metadata structure.
# $c26 is assumed to have room for a stack at the top, although its length is
# currently undefined.
#
# For now, assume:
# (1) The caller has not set up the general-purpose register context, that's
#     our job.
# (2) The goal is not to set up the CHERI calling convention .. yet; we will
#     run only native MIPS64 code, or hand-crafted CHERI assembler.  Thus
#     must change before we can run Clang-compiled CHERI code.
#
	.text
	.global __ccall_enter
	.ent __ccall_enter
__ccall_enter:

	#
	# Set up in-sandbox stack; assume that suitable space for a stack has
	# been made at the top of the sandbox address space described by IDC.
	#
	# XXXRW: We make the stack capability ephemeral, and disallow storing
	# ephemeral capabilities via $c0 here, rather than in the TCB.  This
	# is to ensure software is ready for proper stack handling.  We can
	# remove these candperms once that is present.
	#
	dli	$t0, (~CHERI_PERM_NON_EPHEMERAL)
	candperm	$c11, $c26, $t0
	cgetlen	$sp, $c26
	move	$fp, $sp

	#
	# Install $idc as $c0.  From here on out, we can do MIPS feteches and
	# stores to memory inside the sandbox, and $c0-relative capability
	# fetches and stores.
	#
	dli	$t0, (~CHERI_PERM_STORE_EPHEM_CAP)
	candperm	$c0, $c26, $t0

	#
	# XXXRW: Defensively clear all general-purpose and capability
	# registers that aren't explicit or ABI-implied arguments.
	#

	#
	# Set up global pointer.
	#
	dla	$gp, _gp

	#
	# Extract heap base and length from sandbox metadata.  These must
	# match sandboxasm.h.
	#
	# XXXRW: I'm having trouble using the C preprocessor to provide these
	# values in assembly, so hard-coded for now.  We could also just defer
	# these calculations to C.
	#
	dli	$t0, 0x1000
	ld	$t1, 0($t0)
	dla	$t2, _sb_heapbase;
	sd	$t1, 0($t2)

	ld	$t1, 8($t0)
	dla	$t2, _sb_heaplen;
	sd	$t1, 0($t2)

	#
	# Invoke MIPS ABI C "invoke" function.
	#
	dla	$t9, invoke
	jalr	$t9
	nop			# Branch-delay slot

abort_target:
	#
	# Clear our $c0 so that it is not leaked back to caller.
	#
	# XXXRW: Arguably we should do this for many other registers too,
	# especially as the compiler starts using them.
	#
	ccleartag	$c0, $c0
	ccleartag	$c11, $c11

	#
	# Return to caller.
	#
	creturn
	.end __ccall_enter

	.global abort
	.ent abort
abort:
	dli	$v0, -2
	b	abort_target
	nop
	.end abort

	.global _exit
	.ent _exit
_exit:
	move	$v0, $a0
	b	abort_target
	nop
	.end _exit

	.data
