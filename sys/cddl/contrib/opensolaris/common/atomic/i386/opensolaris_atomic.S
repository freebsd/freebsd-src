/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
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
 * Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

	.file	"atomic.s"

#define	_ASM
#include <sys/asm_linkage.h>

	/*
	 * NOTE: If atomic_dec_64 and atomic_dec_64_nv are ever
	 * separated, it is important to edit the libc i386 platform
	 * specific mapfile and remove the NODYNSORT attribute
	 * from atomic_dec_64_nv.
	 */
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

	/*
	 * NOTE: If atomic_add_64 and atomic_add_64_nv are ever
	 * separated, it is important to edit the libc i386 platform
	 * specific mapfile and remove the NODYNSORT attribute
	 * from atomic_add_64_nv.
	 */
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

	ENTRY(atomic_load_64)
	pushl	%esi
	movl	8(%esp), %esi
	movl	%ebx, %eax	// make old and new values equal, so that
	movl	%ecx, %edx	// destination is never changed
	lock
	cmpxchg8b (%esi)
	popl	%esi
	ret
	SET_SIZE(atomic_load_64)
