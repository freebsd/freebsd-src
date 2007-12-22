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
	SET_SIZE(atomic_or_8_nv)

	ENTRY(atomic_cas_ptr)
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	movl	12(%esp), %ecx
	lock
	cmpxchgl %ecx, (%edx)
	ret
	SET_SIZE(atomic_cas_ptr)

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

	ENTRY(membar_producer)
	lock
	xorl	$0, (%esp)
	ret
	SET_SIZE(membar_producer)
