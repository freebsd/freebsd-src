/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

	.ident	"%Z%%M%	%I%	%E% SMI"

	.file	"%M%"

#define	_ASM
#include <sys/asm_linkage.h>

#if defined(_KERNEL)
	/*
	 * Legacy kernel interfaces; they will go away (eventually).
	 */
	ANSI_PRAGMA_WEAK2(cas8,atomic_cas_8,function)
	ANSI_PRAGMA_WEAK2(cas32,atomic_cas_32,function)
	ANSI_PRAGMA_WEAK2(cas64,atomic_cas_64,function)
	ANSI_PRAGMA_WEAK2(caslong,atomic_cas_ulong,function)
	ANSI_PRAGMA_WEAK2(casptr,atomic_cas_ptr,function)
	ANSI_PRAGMA_WEAK2(atomic_and_long,atomic_and_ulong,function)
	ANSI_PRAGMA_WEAK2(atomic_or_long,atomic_or_ulong,function)
#endif

	ENTRY(atomic_inc_8)
	ALTENTRY(atomic_inc_uchar)
	movl	4(%esp), %eax
	lock
	incb	(%eax)
	ret
	SET_SIZE(atomic_inc_uchar)
	SET_SIZE(atomic_inc_8)

	ENTRY(atomic_inc_16)
	ALTENTRY(atomic_inc_ushort)
	movl	4(%esp), %eax
	lock
	incw	(%eax)
	ret
	SET_SIZE(atomic_inc_ushort)
	SET_SIZE(atomic_inc_16)

	ENTRY(atomic_inc_32)
	ALTENTRY(atomic_inc_uint)
	ALTENTRY(atomic_inc_ulong)
	movl	4(%esp), %eax
	lock
	incl	(%eax)
	ret
	SET_SIZE(atomic_inc_ulong)
	SET_SIZE(atomic_inc_uint)
	SET_SIZE(atomic_inc_32)

	ENTRY(atomic_inc_8_nv)
	ALTENTRY(atomic_inc_uchar_nv)
	movl	4(%esp), %edx	// %edx = target address
	movb	(%edx), %al	// %al = old value
1:
	leal	1(%eax), %ecx	// %cl = new value
	lock
	cmpxchgb %cl, (%edx)	// try to stick it in
	jne	1b
	movzbl	%cl, %eax	// return new value
	ret
	SET_SIZE(atomic_inc_uchar_nv)
	SET_SIZE(atomic_inc_8_nv)

	ENTRY(atomic_inc_16_nv)
	ALTENTRY(atomic_inc_ushort_nv)
	movl	4(%esp), %edx	// %edx = target address
	movw	(%edx), %ax	// %ax = old value
1:
	leal	1(%eax), %ecx	// %cx = new value
	lock
	cmpxchgw %cx, (%edx)	// try to stick it in
	jne	1b
	movzwl	%cx, %eax	// return new value
	ret
	SET_SIZE(atomic_inc_ushort_nv)
	SET_SIZE(atomic_inc_16_nv)

	ENTRY(atomic_inc_32_nv)
	ALTENTRY(atomic_inc_uint_nv)
	ALTENTRY(atomic_inc_ulong_nv)
	movl	4(%esp), %edx	// %edx = target address
	movl	(%edx), %eax	// %eax = old value
1:
	leal	1(%eax), %ecx	// %ecx = new value
	lock
	cmpxchgl %ecx, (%edx)	// try to stick it in
	jne	1b
	movl	%ecx, %eax	// return new value
	ret
	SET_SIZE(atomic_inc_ulong_nv)
	SET_SIZE(atomic_inc_uint_nv)
	SET_SIZE(atomic_inc_32_nv)

	ENTRY(atomic_inc_64)
	ALTENTRY(atomic_inc_64_nv)
	pushl	%edi
	pushl	%ebx
	movl	12(%esp), %edi	// %edi = target address
	movl	(%edi), %eax
	movl	4(%edi), %edx	// %edx:%eax = old value
1:
	xorl	%ebx, %ebx
	xorl	%ecx, %ecx
	incl	%ebx		// %ecx:%ebx = 1
	addl	%eax, %ebx
	adcl	%edx, %ecx	// add in the carry from inc
	lock
	cmpxchg8b (%edi)	// try to stick it in
	jne	1b
	movl	%ebx, %eax
	movl	%ecx, %edx	// return new value
	popl	%ebx
	popl	%edi
	ret
	SET_SIZE(atomic_inc_64_nv)
	SET_SIZE(atomic_inc_64)

	ENTRY(atomic_dec_8)
	ALTENTRY(atomic_dec_uchar)
	movl	4(%esp), %eax
	lock
	decb	(%eax)
	ret
	SET_SIZE(atomic_dec_uchar)
	SET_SIZE(atomic_dec_8)

	ENTRY(atomic_dec_16)
	ALTENTRY(atomic_dec_ushort)
	movl	4(%esp), %eax
	lock
	decw	(%eax)
	ret
	SET_SIZE(atomic_dec_ushort)
	SET_SIZE(atomic_dec_16)

	ENTRY(atomic_dec_32)
	ALTENTRY(atomic_dec_uint)
	ALTENTRY(atomic_dec_ulong)
	movl	4(%esp), %eax
	lock
	decl	(%eax)
	ret
	SET_SIZE(atomic_dec_ulong)
	SET_SIZE(atomic_dec_uint)
	SET_SIZE(atomic_dec_32)

	ENTRY(atomic_dec_8_nv)
	ALTENTRY(atomic_dec_uchar_nv)
	movl	4(%esp), %edx	// %edx = target address
	movb	(%edx), %al	// %al = old value
1:
	leal	-1(%eax), %ecx	// %cl = new value
	lock
	cmpxchgb %cl, (%edx)	// try to stick it in
	jne	1b
	movzbl	%cl, %eax	// return new value
	ret
	SET_SIZE(atomic_dec_uchar_nv)
	SET_SIZE(atomic_dec_8_nv)

	ENTRY(atomic_dec_16_nv)
	ALTENTRY(atomic_dec_ushort_nv)
	movl	4(%esp), %edx	// %edx = target address
	movw	(%edx), %ax	// %ax = old value
1:
	leal	-1(%eax), %ecx	// %cx = new value
	lock
	cmpxchgw %cx, (%edx)	// try to stick it in
	jne	1b
	movzwl	%cx, %eax	// return new value
	ret
	SET_SIZE(atomic_dec_ushort_nv)
	SET_SIZE(atomic_dec_16_nv)

	ENTRY(atomic_dec_32_nv)
	ALTENTRY(atomic_dec_uint_nv)
	ALTENTRY(atomic_dec_ulong_nv)
	movl	4(%esp), %edx	// %edx = target address
	movl	(%edx), %eax	// %eax = old value
1:
	leal	-1(%eax), %ecx	// %ecx = new value
	lock
	cmpxchgl %ecx, (%edx)	// try to stick it in
	jne	1b
	movl	%ecx, %eax	// return new value
	ret
	SET_SIZE(atomic_dec_ulong_nv)
	SET_SIZE(atomic_dec_uint_nv)
	SET_SIZE(atomic_dec_32_nv)

	ENTRY(atomic_dec_64)
	ALTENTRY(atomic_dec_64_nv)
	pushl	%edi
	pushl	%ebx
	movl	12(%esp), %edi	// %edi = target address
	movl	(%edi), %eax
	movl	4(%edi), %edx	// %edx:%eax = old value
1:
	xorl	%ebx, %ebx
	xorl	%ecx, %ecx
	not	%ecx
	not	%ebx		// %ecx:%ebx = -1
	addl	%eax, %ebx
	adcl	%edx, %ecx	// add in the carry from inc
	lock
	cmpxchg8b (%edi)	// try to stick it in
	jne	1b
	movl	%ebx, %eax
	movl	%ecx, %edx	// return new value
	popl	%ebx
	popl	%edi
	ret
	SET_SIZE(atomic_dec_64_nv)
	SET_SIZE(atomic_dec_64)

	ENTRY(atomic_or_8)
	ALTENTRY(atomic_or_uchar)
	movl	4(%esp), %eax
	movb	8(%esp), %cl
	lock
	orb	%cl, (%eax)
	ret
	SET_SIZE(atomic_or_uchar)
	SET_SIZE(atomic_or_8)

	ENTRY(atomic_or_16)
	ALTENTRY(atomic_or_ushort)
	movl	4(%esp), %eax
	movw	8(%esp), %cx
	lock
	orw	%cx, (%eax)
	ret
	SET_SIZE(atomic_or_ushort)
	SET_SIZE(atomic_or_16)

	ENTRY(atomic_or_32)
	ALTENTRY(atomic_or_uint)
	ALTENTRY(atomic_or_ulong)
	movl	4(%esp), %eax
	movl	8(%esp), %ecx
	lock
	orl	%ecx, (%eax)
	ret
	SET_SIZE(atomic_or_ulong)
	SET_SIZE(atomic_or_uint)
	SET_SIZE(atomic_or_32)

	ENTRY(atomic_and_8)
	ALTENTRY(atomic_and_uchar)
	movl	4(%esp), %eax
	movb	8(%esp), %cl
	lock
	andb	%cl, (%eax)
	ret
	SET_SIZE(atomic_and_uchar)
	SET_SIZE(atomic_and_8)

	ENTRY(atomic_and_16)
	ALTENTRY(atomic_and_ushort)
	movl	4(%esp), %eax
	movw	8(%esp), %cx
	lock
	andw	%cx, (%eax)
	ret
	SET_SIZE(atomic_and_ushort)
	SET_SIZE(atomic_and_16)

	ENTRY(atomic_and_32)
	ALTENTRY(atomic_and_uint)
	ALTENTRY(atomic_and_ulong)
	movl	4(%esp), %eax
	movl	8(%esp), %ecx
	lock
	andl	%ecx, (%eax)
	ret
	SET_SIZE(atomic_and_ulong)
	SET_SIZE(atomic_and_uint)
	SET_SIZE(atomic_and_32)

	ENTRY(atomic_add_8_nv)
	ALTENTRY(atomic_add_char_nv)
	movl	4(%esp), %edx	// %edx = target address
	movb	(%edx), %al	// %al = old value
1:
	movl	8(%esp), %ecx	// %ecx = delta
	addb	%al, %cl	// %cl = new value
	lock
	cmpxchgb %cl, (%edx)	// try to stick it in
	jne	1b
	movzbl	%cl, %eax	// return new value
	ret
	SET_SIZE(atomic_add_char_nv)
	SET_SIZE(atomic_add_8_nv)

	ENTRY(atomic_add_16_nv)
	ALTENTRY(atomic_add_short_nv)
	movl	4(%esp), %edx	// %edx = target address
	movw	(%edx), %ax	// %ax = old value
1:
	movl	8(%esp), %ecx	// %ecx = delta
	addw	%ax, %cx	// %cx = new value
	lock
	cmpxchgw %cx, (%edx)	// try to stick it in
	jne	1b
	movzwl	%cx, %eax	// return new value
	ret
	SET_SIZE(atomic_add_short_nv)
	SET_SIZE(atomic_add_16_nv)

	ENTRY(atomic_add_32_nv)
	ALTENTRY(atomic_add_int_nv)
	ALTENTRY(atomic_add_ptr_nv)
	ALTENTRY(atomic_add_long_nv)
	movl	4(%esp), %edx	// %edx = target address
	movl	(%edx), %eax	// %eax = old value
1:
	movl	8(%esp), %ecx	// %ecx = delta
	addl	%eax, %ecx	// %ecx = new value
	lock
	cmpxchgl %ecx, (%edx)	// try to stick it in
	jne	1b
	movl	%ecx, %eax	// return new value
	ret
	SET_SIZE(atomic_add_long_nv)
	SET_SIZE(atomic_add_ptr_nv)
	SET_SIZE(atomic_add_int_nv)
	SET_SIZE(atomic_add_32_nv)

	ENTRY(atomic_add_64)
	ALTENTRY(atomic_add_64_nv)
	pushl	%edi
	pushl	%ebx
	movl	12(%esp), %edi	// %edi = target address
	movl	(%edi), %eax
	movl	4(%edi), %edx	// %edx:%eax = old value
1:
	movl	16(%esp), %ebx
	movl	20(%esp), %ecx	// %ecx:%ebx = delta
	addl	%eax, %ebx
	adcl	%edx, %ecx	// %ecx:%ebx = new value
	lock
	cmpxchg8b (%edi)	// try to stick it in
	jne	1b
	movl	%ebx, %eax
	movl	%ecx, %edx	// return new value
	popl	%ebx
	popl	%edi
	ret
	SET_SIZE(atomic_add_64_nv)
	SET_SIZE(atomic_add_64)

	ENTRY(atomic_or_8_nv)
	ALTENTRY(atomic_or_uchar_nv)
	movl	4(%esp), %edx	// %edx = target address
	movb	(%edx), %al	// %al = old value
1:
	movl	8(%esp), %ecx	// %ecx = delta
	orb	%al, %cl	// %cl = new value
	lock
	cmpxchgb %cl, (%edx)	// try to stick it in
	jne	1b
	movzbl	%cl, %eax	// return new value
	ret
	SET_SIZE(atomic_or_uchar_nv)
	SET_SIZE(atomic_or_8_nv)

	ENTRY(atomic_or_16_nv)
	ALTENTRY(atomic_or_ushort_nv)
	movl	4(%esp), %edx	// %edx = target address
	movw	(%edx), %ax	// %ax = old value
1:
	movl	8(%esp), %ecx	// %ecx = delta
	orw	%ax, %cx	// %cx = new value
	lock
	cmpxchgw %cx, (%edx)	// try to stick it in
	jne	1b
	movzwl	%cx, %eax	// return new value
	ret
	SET_SIZE(atomic_or_ushort_nv)
	SET_SIZE(atomic_or_16_nv)

	ENTRY(atomic_or_32_nv)
	ALTENTRY(atomic_or_uint_nv)
	ALTENTRY(atomic_or_ulong_nv)
	movl	4(%esp), %edx	// %edx = target address
	movl	(%edx), %eax	// %eax = old value
1:
	movl	8(%esp), %ecx	// %ecx = delta
	orl	%eax, %ecx	// %ecx = new value
	lock
	cmpxchgl %ecx, (%edx)	// try to stick it in
	jne	1b
	movl	%ecx, %eax	// return new value
	ret
	SET_SIZE(atomic_or_ulong_nv)
	SET_SIZE(atomic_or_uint_nv)
	SET_SIZE(atomic_or_32_nv)

	ENTRY(atomic_or_64)
	ALTENTRY(atomic_or_64_nv)
	pushl	%edi
	pushl	%ebx
	movl	12(%esp), %edi	// %edi = target address
	movl	(%edi), %eax
	movl	4(%edi), %edx	// %edx:%eax = old value
1:
	movl	16(%esp), %ebx
	movl	20(%esp), %ecx	// %ecx:%ebx = delta
	orl	%eax, %ebx
	orl	%edx, %ecx	// %ecx:%ebx = new value
	lock
	cmpxchg8b (%edi)	// try to stick it in
	jne	1b
	movl	%ebx, %eax
	movl	%ecx, %edx	// return new value
	popl	%ebx
	popl	%edi
	ret
	SET_SIZE(atomic_or_64_nv)
	SET_SIZE(atomic_or_64)

	ENTRY(atomic_and_8_nv)
	ALTENTRY(atomic_and_uchar_nv)
	movl	4(%esp), %edx	// %edx = target address
	movb	(%edx), %al	// %al = old value
1:
	movl	8(%esp), %ecx	// %ecx = delta
	andb	%al, %cl	// %cl = new value
	lock
	cmpxchgb %cl, (%edx)	// try to stick it in
	jne	1b
	movzbl	%cl, %eax	// return new value
	ret
	SET_SIZE(atomic_and_uchar_nv)
	SET_SIZE(atomic_and_8_nv)

	ENTRY(atomic_and_16_nv)
	ALTENTRY(atomic_and_ushort_nv)
	movl	4(%esp), %edx	// %edx = target address
	movw	(%edx), %ax	// %ax = old value
1:
	movl	8(%esp), %ecx	// %ecx = delta
	andw	%ax, %cx	// %cx = new value
	lock
	cmpxchgw %cx, (%edx)	// try to stick it in
	jne	1b
	movzwl	%cx, %eax	// return new value
	ret
	SET_SIZE(atomic_and_ushort_nv)
	SET_SIZE(atomic_and_16_nv)

	ENTRY(atomic_and_32_nv)
	ALTENTRY(atomic_and_uint_nv)
	ALTENTRY(atomic_and_ulong_nv)
	movl	4(%esp), %edx	// %edx = target address
	movl	(%edx), %eax	// %eax = old value
1:
	movl	8(%esp), %ecx	// %ecx = delta
	andl	%eax, %ecx	// %ecx = new value
	lock
	cmpxchgl %ecx, (%edx)	// try to stick it in
	jne	1b
	movl	%ecx, %eax	// return new value
	ret
	SET_SIZE(atomic_and_ulong_nv)
	SET_SIZE(atomic_and_uint_nv)
	SET_SIZE(atomic_and_32_nv)

	ENTRY(atomic_and_64)
	ALTENTRY(atomic_and_64_nv)
	pushl	%edi
	pushl	%ebx
	movl	12(%esp), %edi	// %edi = target address
	movl	(%edi), %eax
	movl	4(%edi), %edx	// %edx:%eax = old value
1:
	movl	16(%esp), %ebx
	movl	20(%esp), %ecx	// %ecx:%ebx = delta
	andl	%eax, %ebx
	andl	%edx, %ecx	// %ecx:%ebx = new value
	lock
	cmpxchg8b (%edi)	// try to stick it in
	jne	1b
	movl	%ebx, %eax
	movl	%ecx, %edx	// return new value
	popl	%ebx
	popl	%edi
	ret
	SET_SIZE(atomic_and_64_nv)
	SET_SIZE(atomic_and_64)

	ENTRY(atomic_cas_8)
	ALTENTRY(atomic_cas_uchar)
	movl	4(%esp), %edx
	movzbl	8(%esp), %eax
	movb	12(%esp), %cl
	lock
	cmpxchgb %cl, (%edx)
	ret
	SET_SIZE(atomic_cas_uchar)
	SET_SIZE(atomic_cas_8)

	ENTRY(atomic_cas_16)
	ALTENTRY(atomic_cas_ushort)
	movl	4(%esp), %edx
	movzwl	8(%esp), %eax
	movw	12(%esp), %cx
	lock
	cmpxchgw %cx, (%edx)
	ret
	SET_SIZE(atomic_cas_ushort)
	SET_SIZE(atomic_cas_16)

	ENTRY(atomic_cas_32)
	ALTENTRY(atomic_cas_uint)
	ALTENTRY(atomic_cas_ulong)
	ALTENTRY(atomic_cas_ptr)
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	movl	12(%esp), %ecx
	lock
	cmpxchgl %ecx, (%edx)
	ret
	SET_SIZE(atomic_cas_ptr)
	SET_SIZE(atomic_cas_ulong)
	SET_SIZE(atomic_cas_uint)
	SET_SIZE(atomic_cas_32)

	ENTRY(atomic_cas_64)
	pushl	%ebx
	pushl	%esi
	movl	12(%esp), %esi
	movl	16(%esp), %eax
	movl	20(%esp), %edx
	movl	24(%esp), %ebx
	movl	28(%esp), %ecx
	lock
	cmpxchg8b (%esi)
	popl	%esi
	popl	%ebx
	ret
	SET_SIZE(atomic_cas_64)

	ENTRY(atomic_swap_8)
	ALTENTRY(atomic_swap_uchar)
	movl	4(%esp), %edx
	movzbl	8(%esp), %eax
	lock
	xchgb	%al, (%edx)
	ret
	SET_SIZE(atomic_swap_uchar)
	SET_SIZE(atomic_swap_8)

	ENTRY(atomic_swap_16)
	ALTENTRY(atomic_swap_ushort)
	movl	4(%esp), %edx
	movzwl	8(%esp), %eax
	lock
	xchgw	%ax, (%edx)
	ret
	SET_SIZE(atomic_swap_ushort)
	SET_SIZE(atomic_swap_16)

	ENTRY(atomic_swap_32)
	ALTENTRY(atomic_swap_uint)
	ALTENTRY(atomic_swap_ptr)
	ALTENTRY(atomic_swap_ulong)
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	lock
	xchgl	%eax, (%edx)
	ret
	SET_SIZE(atomic_swap_ulong)
	SET_SIZE(atomic_swap_ptr)
	SET_SIZE(atomic_swap_uint)
	SET_SIZE(atomic_swap_32)

	ENTRY(atomic_swap_64)
	pushl	%esi
	pushl	%ebx
	movl	12(%esp), %esi
	movl	16(%esp), %ebx
	movl	20(%esp), %ecx
	movl	(%esi), %eax
	movl	4(%esi), %edx	// %edx:%eax = old value
1:
	lock
	cmpxchg8b (%esi)
	jne	1b
	popl	%ebx
	popl	%esi
	ret
	SET_SIZE(atomic_swap_64)

	ENTRY(atomic_set_long_excl)
	movl	4(%esp), %edx	// %edx = target address
	movl	8(%esp), %ecx	// %ecx = bit id
	xorl	%eax, %eax
	lock
	btsl	%ecx, (%edx)
	jnc	1f
	decl	%eax		// return -1
1:
	ret
	SET_SIZE(atomic_set_long_excl)

	ENTRY(atomic_clear_long_excl)
	movl	4(%esp), %edx	// %edx = target address
	movl	8(%esp), %ecx	// %ecx = bit id
	xorl	%eax, %eax
	lock
	btrl	%ecx, (%edx)
	jc	1f
	decl	%eax		// return -1
1:
	ret
	SET_SIZE(atomic_clear_long_excl)

	ENTRY(membar_enter)
	ALTENTRY(membar_exit)
	ALTENTRY(membar_producer)
	ALTENTRY(membar_consumer)
	lock
	xorl	$0, (%esp)
	ret
	SET_SIZE(membar_consumer)
	SET_SIZE(membar_producer)
	SET_SIZE(membar_exit)
	SET_SIZE(membar_enter)
