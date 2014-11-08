/*-
 * Copyright (c) 2012-2014 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * CHERI system class CCall landing pad code: catches CCalls inbound from
 * sandboxes seeking system services, and bootstraps C code.  A number of
 * differences from sandboxed code, including how $c0 is handled, and not
 * setting up the C heap.
 *
 * Temporary ABI conventions:
 *    $sp contains a pointer to the top of the stack; 32-byte aligned
 *    $fp contains a pointer to the top of the stack; 32-byte aligned
 *
 *    $a0-$a7 contain user arguments
 *    $v0, $v1 contain user return values
 *
 *    $c0, $pcc contain access to (100% overlapped) sandbox code and data
 *
 *    $c1, $c2 contain the invoked object capability
 *    $c3-$c10 contain user capability arguments
 *
 *    $c26 contains the invoked data capability installed by CCall; unlike
 *      sandboxed versions of this code, this points at actual data rather
 *      than being the value to install in $c0.  $c0 is copied from $pcc.
 *
 * Sandbox heap information is extracted from the sandbox metadata structure.
 * $c26 is assumed to have room for a stack at the top, although its length is
 * currently undefined.
 *
 * For now, assume:
 * (1) The caller has not set up the general-purpose register context, that's
 *     our job.
 * (2) That there is no concurrent sandbox use -- we have a single stack on
 *     the inbound path, which can't be the long-term solution.
 */

#define	CHERI_CLASS_ASM(class, function)				\
	.text;								\
	.global __cheri_ ## class ## _entry;				\
	.ent __cheri_ ## class ## _entry;				\
__cheri_ ## class ## _entry:						\
									\
	/*								\
	 * Normally in a CHERI sandbox, we would install $c26 ($idc)	\
	 * into $c0 for MIPS load/store instructions.  For the system	\
	 * class, we instead use $pcc so that we can have an executable	\
	 * version of $c0; as $pcc.offset contains the entry address,	\
	 * we must clear that in $c0.  Also install as the stack	\
	 * capability.							\
	 *								\
	 * XXXRW: Should we clearing CHERI_PERM_EXECUTE on $c0?		\
	 */								\
	cgetpcc $c0;							\
	csetoffset	$c0, $c0, $zero;				\
	cmove	$c11, $c0;						\
									\
	/*								\
	 * Install global invocation stack.  NB: this means we can't	\
	 * support recursion or concurrency.  Further note: this is	\
	 * shared by all classes outside of the sandbox.		\
	 */								\
	dla	$sp, __cheri_enter_stack_top;				\
	ld	$sp, 0($sp);						\
	move	$fp, $sp;						\
									\
	/*								\
	 * Set up global pointer.					\
	 */								\
	dla	$gp, _gp;						\
									\
	/*								\
	 * Invoke MIPS ABI C "enter" function.				\
	 */								\
	dla	$t9, function;						\
	jalr	$t9;							\
	nop;			/* Branch-delay slot */			\
									\
	/*								\
	 * Return to caller.						\
	 */								\
	creturn;							\
	.end __cheri_## class ## _entry;

#define	CHERI_CLASS_DECL(class)						\
	extern void __cheri_## class ## _entry;

#define	CHERI_CLASS_ENTRY(class)					\
	(&__cheri_## class ## _entry)
