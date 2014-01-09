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

# C-compatible memcpy, wrapping the capability version
# Note: This is currently called smemcpy (simple memcpy) so that we don't need
# to remove the old memcpy yet.  It's also important to remember that some of
# the assembly functions here may not respect the ABI in terms of the caller /
# callee-save registers, and so expect memcpy() to clobber fewer registers than
# this does.
# void *memcpy(void *dst,
#              void *src,
#              size_t len)
# dst: $c1
# src: $c2
# len: $4
.text
.global smemcpy
.ent smemcpy
smemcpy:
	CIncBase $c3, $c0, $a0      # Get the destination capability
	CIncBase $c4, $c0, $a1      # Get the source capability
	b        cmemcpy            # Jump to the capability version
	daddi    $a0, $a2, 0        # Move the length to arg0 (delay slot)
.end smemcpy

#
# Capability Memcpy - copies from one capability to another.
# __capability void *cmemcpy(__capability void *dst,
#                            __capability void *src,
#                            size_t len)
# dst: $c3
# src: $c4
# len: $4
# Copies len bytes from src to dst.  Returns dst.
		.text
		.global cmemcpy
		.ent cmemcpy
cmemcpy:
	beq      $4, $zero, cmemcpy_return  # Only bother if len != 0.  Unlikely to
	                               # be taken, so we make it a forward branch
	                               # to give the predictor a hint.
	# Note: We use v0 to store the base linear address because memcpy() must
	# return that value in v0, allowing cmemcpy() to be tail-called from
	# memcpy().  This is in the delay slot, so it happens even if len == 0.
	CGetBase $v0, $c3            # v0 = linear address of dst
	CGetBase $v1, $c4            # v1 = linear address of src
	andi     $12, $v0, 0x1f      # t4 = dst % 32
	andi     $13, $v1, 0x1f      # t5 = src % 32
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
	andi     $t8, $a0, 0x1f      # t8 = len % 32

fast_path:                       # At this point, src and dst are known to have
                                 # the same alignment.  They may not be 32-byte
                                 # aligned, however.
	# FIXME: This logic can be simplified by using the power of algebra
	dsub    $v1, $zero, $12
	daddi   $v1, $v1, 32
	andi    $v1, $v1, 0x1f      # v1 = number of bytes we need to copy to
	                            # become aligned
	dsub    $a2, $a0, $v1
	daddi   $a2, $a2, -32        # (delay slot)
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
	addi    $at, $zero, 0xFFE0
	and     $a2, $a2, $at        # a2 = number of 32-byte aligned bytes to copy
	dadd    $a2, $a2, $a1        # ...plus the number already copied.

copy_caps:
	clc     $c5, $a1, 0($c4)
	daddi   $a1, $a1, 32
	bne     $a1, $a2, copy_caps
	csc     $c5, $a1, -32($c3)

	dsub    $v1, $a0, $a2        # Subtract the number of bytes copied from the
	                             # number to copy.  This should give the number
	                             # of unaligned bytes that we still need to copy
	beqzl   $v1, cmemcpy_return  # If we have an aligned copy (which we probably
	                             # do) then return
	nop
	dadd    $v1, $a1, $v1
unaligned_end:
	clb      $a2, $a1, 0($c4)
	daddi    $a1, $a1, 1
	bne      $v1, $a1, unaligned_end
	csb      $a2, $a1, -1($c3)

cmemcpy_return:
	jr       $ra                 # Return value remains in c1
	nop

slow_memcpy_loop:                # byte-by-byte copy
	clb      $a2, $a1, 0($c4)
	daddi    $a1, $a1, 1
	bne      $a0, $a1, slow_memcpy_loop
	csb      $a2, $a1, -1($c3)
	jr       $ra                 # Return value remains in c1
	nop
.end cmemcpy
