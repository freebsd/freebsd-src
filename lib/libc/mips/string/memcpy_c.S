#-
# Copyright (c) 2012-2013 David Chisnall
# Copyright (c) 2011 Robert N. M. Watson
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
#ifndef _MIPS_SZCAP
#error Size of capability unknown!
#endif
#define CAP_SIZE (_MIPS_SZCAP/8)
#define CAP_ALIGN_MASK (CAP_SIZE-1)

.macro ret
#ifdef __CHERI_PURE_CAPABILITY__
	cjr	$c17
#else
	jr	$ra
#endif
.endm

#
# Capability Memcpy - copies from one capability to another.
# __capability void *memcpy_c(__capability void *dst,
#                            __capability void *src,
#                            size_t len)
# dst: $c3
# src: $c4
# len: $4
# Copies len bytes from src to dst.  Returns dst.
		.text
		.global memcpy_c
		.ent memcpy_c
memcpy_c:
	beq      $4, $zero, memcpy_c_return  # Only bother if len != 0.  Unlikely to
	                               # be taken, so we make it a forward branch
	                               # to give the predictor a hint.
	# Note: We use v0 to store the base linear address because memcpy() must
	# return that value in v0, allowing memcpy_c() to be tail-called from
	# memcpy().  This is in the delay slot, so it happens even if len == 0.
	CGetBase $v0, $c3            # v0 = linear address of dst
	CGetOffset $at, $c3
	dadd     $v0, $v0, $at
	CGetBase $v1, $c4            # v1 = linear address of src
	CGetOffset $at, $c4
	dadd     $v1, $v1, $at
	andi     $12, $v0, CAP_ALIGN_MASK # t4 = dst % CAP_SIZE
          CToPtr   $v0, $c0, $c3       # Return ptr in integer return
	andi     $13, $v1, CAP_ALIGN_MASK # t5 = src % CAP_SIZE

	# XXX: Work around the fact that CLC/CSC fault rather than loading
	# untagged memory values when capability load/store permissions are
	# missing.
	CGetPerm $a5, $c3
	CGetPerm $a6, $c4
	andi     $a5, (1<<5)
	andi     $a6, (1<<4)
	xor      $a5, $a5, $a6
	addi     $a6, $zero, ((1<<4)+(1<<5))
	bne      $a5, $a6, slow_memcpy_loop

	daddi    $a1, $zero, 0       # Store 0 in $a1 - we'll use that for the
	                             # offset later.

	bne      $12, $13, slow_memcpy_loop
	                             # If src and dst have different alignments, we
	                             # have to copy a byte at a time because we
	                             # don't have any multi-byte load/store
	                             # instruction pairs with different alignments.
	                             # We could do something involving shifts, but
	                             # this is probably a sufficiently uncommon
	                             # case not to be worth optimising.
	andi     $t8, $a0, CAP_ALIGN_MASK      # t8 = len % CAP_SIZE

fast_path:                       # At this point, src and dst are known to have
                                 # the same alignment.  They may not be capability
                                 # aligned, however.
	# FIXME: This logic can be simplified by using the power of algebra
	dsub    $v1, $zero, $12
	daddi   $v1, $v1, CAP_SIZE
	andi    $v1, $v1, CAP_ALIGN_MASK      # v1 = number of bytes we need to copy to
	                            # become aligned
	dsub    $a2, $a0, $v1
	daddi   $a2, $a2, -CAP_SIZE
	bltz    $a2, slow_memcpy_loop# If we are copying more bytes than the number
	                             # required for alignment, plus at least one
	                             # capability more, continue in the fast path
	nop
	beqzl   $v1, aligned_copy    # If we have an aligned copy (which we probably
	                             # do) then skip the slow part

	dsub    $a2, $a0, $a1        # $12 = amount left to copy (delay slot, only
	                             # executed if branch is taken)
unaligned_start:
	clb      $a2, $a1, 0($c4)
	daddi    $a1, $a1, 1
	bne      $v1, $a1, unaligned_start
	csb      $a2, $a1, -1($c3)

	dsub     $a2, $a0, $a1        # $12 = amount left to copy
aligned_copy:
	addi    $at, $zero, -CAP_SIZE
	and     $a2, $a2, $at        # a2 = number of capability-aligned bytes to copy
	dadd    $a2, $a2, $a1        # ...plus the number already copied.

copy_caps:
	clc     $c5, $a1, 0($c4)
	daddi   $a1, $a1, CAP_SIZE
	bne     $a1, $a2, copy_caps
	csc     $c5, $a1, -CAP_SIZE($c3)

	dsub    $v1, $a0, $a2        # Subtract the number of bytes copied from the
	                             # number to copy.  This should give the number
	                             # of unaligned bytes that we still need to copy
	beqzl   $v1, memcpy_c_return # If we have an aligned copy (which we probably
	                             # do) then return
	nop
	dadd    $v1, $a1, $v1
unaligned_end:
	clb      $a2, $a1, 0($c4)
	daddi    $a1, $a1, 1
	bne      $v1, $a1, unaligned_end
	csb      $a2, $a1, -1($c3)

memcpy_c_return:
	ret                          # Return value remains in c1
	nop

slow_memcpy_loop:                # byte-by-byte copy
	clb      $a2, $a1, 0($c4)
	daddi    $a1, $a1, 1
	bne      $a0, $a1, slow_memcpy_loop
	csb      $a2, $a1, -1($c3)
	ret                          # Return value remains in c1
	nop
.end memcpy_c

# Use the capability memcpy for normal memcopies. (Not enabled yet)
#if 0
		.text
		.global memcpy
		.ent memcpy
memcpy:
	CFromPtr $c3, $c0, $a0       # dst ptr -> dst capability
	CFromPtr $c4, $c0, $a1       # src ptr -> src capability
          b        memcpy_c
          move     $a0, $a3            # len -> first integer arg register (in delay slot)
.end memcpy
#endif
