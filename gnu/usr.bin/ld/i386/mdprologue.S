/*
 * Copyright (c) 1993 Paul Kranenburg
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: mdprologue.S,v 1.4 1994/06/15 22:40:49 rich Exp $
 */

/*
 * i386 run-time link editor entry points.
 */

#include <sys/syscall.h>
#define LCALL(x,y)	.byte 0x9a ; .long y; .word x

	.text
	.globl	_binder, _binder_entry

/*
 *	_rtl(int version, struct crt_ldso *crtp)
 */
#define FRAME	12			/* Size of stack frame */


_rtl:					# crt0 calls us here
	pushl	%ebp			# Allocate stack frame
	movl	%esp, %ebp
	subl	$FRAME, %esp
	pushl	%ebx
	call	1f			# PIC function prologue
1:
	popl	%ebx
	addl	$_GLOBAL_OFFSET_TABLE_+[.-1b], %ebx

	movl	12(%ebp), %eax		# Extract data from interface structure
	movl	(%eax),%eax		# base address of ld.so (first field)
					# setup arguments for rtld()
	movl	(%ebx), %ecx		# 1st entry in GOT is our __DYNAMIC
	addl	%eax, %ecx		#   add load address
	pushl	%ecx			# 3rd arg
	pushl	12(%ebp)		# 2nd arg == &crt.
	pushl	8(%ebp)			# 1st arg == version
	addl	_rtld@GOT(%ebx), %eax	# relocate address of function
	call	%eax			# _rtld(version, crtp, DYNAMIC)
	addl	$12,%esp		# pop arguments

	movl	(-FRAME-4)(%ebp), %ebx	# restore %ebx
	leave				# remove stack frame,
	ret				# lets rock

 # First call to a procedure generally comes through here for
 # binding.

_binder_entry:
	pushl	%ebp			# setup a stack frame
	movl	%esp, %ebp
	pusha				# save all regs

	movl	$0, %eax		# clear
	movl	4(%ebp), %esi		# return address in PLT
	movw	(%esi), %ax		# get hold of relocation number
	subl	$6, %esi		# make it point to the jmpslot

	pushl	%eax			# pushd arguments
	pushl	%esi			#
	call	_binder@PLT		# _binder(rpc, index)
	addl	$8, %esp		# pop arguments
	movl	%eax, 4(%ebp)		# return value from _binder() == actual
					#  address of function
	popa				# restore regs
	leave				# remove our stack frame
	ret

 # Special system call stubs which return real and effective user and group
 # ids.  Saves overhead of making separate calls for each.
 # !! Relies on compatability option in BSD 4.three-and-a-half

	.globl	_getreuid, _getregid
_getreuid:
	lea	SYS_getuid, %eax
	LCALL(7,0)
	jc	out
	movl	4(%esp), %ecx		# get 1st arg
	movl	%eax, (%ecx)		# put value in it
	movl	8(%esp), %ecx		# same for 2nd arg
	movl	%edx, (%ecx)		#
	ret				# done

_getregid:
	lea	SYS_getgid, %eax
	LCALL(7,0)
	jc	out
	movl	4(%esp), %ecx		# get 1st arg
	movl	%eax, (%ecx)		# put value in it
	movl	8(%esp), %ecx		# same for 2nd arg
	movl	%edx, (%ecx)		#
	ret				# done

out:	jmp	cerror@PLT		# Call common error routine

