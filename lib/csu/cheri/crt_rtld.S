#-
# Copyright (c) 2012-2015 Robert N. M. Watson
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
#include "abicall.s"

#.set mips64
.set noreorder
#.set nobopt
.set noat

#
# CHERI sandbox run-time linker entry point, used for constructors and
# destructors.  Identical calling convention to that described in
# crt_invoke.S.
#
# Most code borrowed from crt_invoke.S; notice that exit/abort symbols here
# are not global, and that there is no path to invoke().
#

	.text
	.option pic0
	.global __rtld_vector
	.type __rtld_vector,@function
	.ent __rtld_vector
__rtld_vector:

	#
	# Load $idc-relative offset of the Sandbox Metadata (SBM) area
	#
	dli	$t0, 0x1000

	#
	# Set up the in-sandbox stack: a stack capability has been left for us
	# by the run-time.  For now, we assume that $sp is the length (i.e.,
	# that $c11.offset == 0), but eventually we will presumably use only
	# $c11 and can leave $sp alone.
	#
	clc	$c11, $t0, 32 + 3*CHERICAP_SIZE($c26)
	cgetlen	$sp, $c11
	move	$fp, $sp

	#
	# Install $idc as $c0.  From here on out, we can do MIPS fetches and
	# stores to memory inside the sandbox, and $c0-relative capability
	# fetches and stores.
	#
	csetdefault	$c26

	#
	# The compiler is responsible for scrubbing unused argument registers
	# (since only it has the information required to do so).  CCall is
	# responsible for scrubbing all other registers.
	#

	#
	# Set up global pointer.
	#
	dla	$gp, _gp

	#
	# Method switch for the run-time linker vector.
	#
	dli	$at, -1
	beq	$v0, $at, constructors
	nop

	# dli	$at, 2
	# beq	$v0, $at, destructors
	# nop

	#
	# No matching function; return (-1).
	#
	cfromptr	$c3, $c3, $zero
	dli	$v0, -1
	dli	$v1, 0

creturn_target:
	#
	# The compiler is responsible for scrubbing unused return-value
	# registers (since only it has the information required to do so).
	# CReturn is responsible for scrubbing all other registers.
	#

	#
	# Return to caller.
	#
	creturn

constructors:
	#
	# Extract heap base, heap length, and CHERI system object reference
	# from sandbox metadata.  These must match sandboxasm.h.
	#
	# XXXRW: I'm having trouble using the C preprocessor to provide these
	# values in assembly, so hard-coded for now.  We could also just defer
	# these calculations to C.
	#
	ld	$t1, 0($t0)
	dla	$t2, _sb_heapbase;
	sd	$t1, 0($t2)

	ld	$t1, 8($t0)
	dla	$t2, _sb_heaplen;
	sd	$t1, 0($t2)

	# Code capability
	clc	$c12, $t0, 32($c26)
	dla	$t2, _cheri_system_object;
	csc	$c12, $t2, 0($c26)

	# Data capability
	clc	$c12, $t0, 32+CHERICAP_SIZE($c26)
	dla	$t2, _cheri_system_object;
	csc	$c12, $t2, CHERICAP_SIZE($c26)

	#
	# Perform global capability initialisation from C.
	#
	dla	$t9, crt_init_globals
	abicall	$t9
	#
	# Perform constructor initialisation from C; the C code must be very
	# careful not to depend on the outcome of any constructors!  Here, as
	# above, set $v0 to indicate success, and scrub $v1/$c3 before
	# returning.
	#
	dla	$t9, crt_call_constructors
	abicall	$t9

	#
	# Return with $v0 set to '0'; $v1 and $c3 scrubbed.
	#
	cfromptr	$c3, $c3, $zero
	dli	$v0, 0
	b	creturn_target
	dli	$v1, 0		# Branch-delay slot
	.end __rtld_vector

	.data
