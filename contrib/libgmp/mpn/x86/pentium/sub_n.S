/* Pentium __mpn_sub_n -- Subtract two limb vectors of the same length > 0
   and store difference in a third limb vector.

Copyright (C) 1992, 1994, 1995, 1996 Free Software Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of the GNU Library General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

The GNU MP Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
License for more details.

You should have received a copy of the GNU Library General Public License
along with the GNU MP Library; see the file COPYING.LIB.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
MA 02111-1307, USA. */

/*
   INPUT PARAMETERS
   res_ptr	(sp + 4)
   s1_ptr	(sp + 8)
   s2_ptr	(sp + 12)
   size		(sp + 16)
*/

#include "sysdep.h"
#include "asm-syntax.h"

.text
	ALIGN (3)
	.globl C_SYMBOL_NAME(__mpn_sub_n)
C_SYMBOL_NAME(__mpn_sub_n:)
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	pushl	%ebp

	movl	20(%esp),%edi		/* res_ptr */
	movl	24(%esp),%esi		/* s1_ptr */
	movl	28(%esp),%ebp		/* s2_ptr */
	movl	32(%esp),%ecx		/* size */

	movl	(%ebp),%ebx

	decl	%ecx
	movl	%ecx,%edx
	shrl	$3,%ecx
	andl	$7,%edx
	testl	%ecx,%ecx		/* zero carry flag */
	jz	Lend
	pushl	%edx

	ALIGN (3)
Loop:	movl	28(%edi),%eax		/* fetch destination cache line */
	leal	32(%edi),%edi

L1:	movl	(%esi),%eax
	movl	4(%esi),%edx
	sbbl	%ebx,%eax
	movl	4(%ebp),%ebx
	sbbl	%ebx,%edx
	movl	8(%ebp),%ebx
	movl	%eax,-32(%edi)
	movl	%edx,-28(%edi)

L2:	movl	8(%esi),%eax
	movl	12(%esi),%edx
	sbbl	%ebx,%eax
	movl	12(%ebp),%ebx
	sbbl	%ebx,%edx
	movl	16(%ebp),%ebx
	movl	%eax,-24(%edi)
	movl	%edx,-20(%edi)

L3:	movl	16(%esi),%eax
	movl	20(%esi),%edx
	sbbl	%ebx,%eax
	movl	20(%ebp),%ebx
	sbbl	%ebx,%edx
	movl	24(%ebp),%ebx
	movl	%eax,-16(%edi)
	movl	%edx,-12(%edi)

L4:	movl	24(%esi),%eax
	movl	28(%esi),%edx
	sbbl	%ebx,%eax
	movl	28(%ebp),%ebx
	sbbl	%ebx,%edx
	movl	32(%ebp),%ebx
	movl	%eax,-8(%edi)
	movl	%edx,-4(%edi)

	leal	32(%esi),%esi
	leal	32(%ebp),%ebp
	decl	%ecx
	jnz	Loop

	popl	%edx
Lend:
	decl	%edx			/* test %edx w/o clobbering carry */
	js	Lend2
	incl	%edx
Loop2:
	leal	4(%edi),%edi
	movl	(%esi),%eax
	sbbl	%ebx,%eax
	movl	4(%ebp),%ebx
	movl	%eax,-4(%edi)
	leal	4(%esi),%esi
	leal	4(%ebp),%ebp
	decl	%edx
	jnz	Loop2
Lend2:
	movl	(%esi),%eax
	sbbl	%ebx,%eax
	movl	%eax,(%edi)

	sbbl	%eax,%eax
	negl	%eax

	popl	%ebp
	popl	%ebx
	popl	%esi
	popl	%edi
	ret
