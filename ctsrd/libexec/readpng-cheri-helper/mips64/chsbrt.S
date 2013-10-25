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
# CHERI sandbox runtime startup code -- begins execution inside a sandbox and
# is responsible for bootstrapping MIPS-compiled C code (without CHERI
# extensions).
#
# Temporary ABI conventions:
#    $sp contains a pointer to the top of the stack; 32-byte aligned
#    $fp contains a pointer to the top of the stack; 32-byte aligned
#
#    $a0-$a3 contain user arguments
#    $a4 contains the heap offset within the sandbox
#    $a5 contains the heap length
#    $v0, $v1 contain user return values
#
#    $c0, $pcc contain access to (100% overlapped) sandbox code and data
#
#    $c1, $c2 contain the invoked object capability
#    $c3-$c10 contain user capability arguments
#
#    $c26 contains the invoked data capability - the complete segment, which
#      we will install into $c0 to allow MIPS code to access it.
#
# For now, assume:
# (1) The caller has not set up the general-purpose register context, that's
#     our job.
# (2) The goal is not to set up the CHERI calling convention .. yet; we will
#     run only native MIPS64 code, or hand-crafted CHERI assembler.  Thus
#     must change before we can run Clang-compiled CHERI code.
#
	.text
	.global __start
	.ent __start
__start:

	#
	# Set up in-sandbox stack.
	#
	cgetlen	$sp, $c26
	move	$fp, $sp

	#
	# Install $idc as $c0.  From here on out, we can do MIPS feteches and
	# stores to memory inside the sandbox, and $c0-relative capability
	# fetches and stores.
	#
	cmove	$c0, $c26

	#
	# XXXRW: Defensively clear all general-purpose and capability
	# registers that aren't explicit or ABI-implied arguments.
	#

	#
	# Set up global pointer, also install heap base and length from
	# caller.
	#
	dla	$gp, _gp
	dla	$at, _sb_heapbase;
	sd	$a4, 0($at)
	dla	$at, _sb_heaplen;
	sd	$a5, 0($at)

	#
	# Invoke MIPS ABI C "invoke" function.
	#
	# XXXRW: $t9 not set properly.
	#
	dla	$25, invoke
	jalr	$25
	nop			# Branch-delay slot

abort_target:
	#
	# Clear our $c0 so that it is not leaked back to caller.
	#
	ccleartag	$c0

	#
	# Return to caller.
	#
	creturn
	.end __start

	.global abort
	.ent abort
abort:
	dli	$v0, -2
	b	abort_target
	nop
	.end abort

	.data
