/* Pentium optimized __mpn_lshift -- 

Copyright (C) 1992, 1994, 1995 Free Software Foundation, Inc.

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
  s_ptr		(sp + 8)
  size		(sp + 12)
  cnt		(sp + 16)
*/

#include "sysdep.h"
#include "asm-syntax.h"

.text
	ALIGN (3)
	.globl C_SYMBOL_NAME(__mpn_lshift)
C_SYMBOL_NAME(__mpn_lshift:)
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	pushl	%ebp

	movl	20(%esp),%edi		/* res_ptr */
	movl	24(%esp),%esi		/* s_ptr */
	movl	28(%esp),%ebp		/* size */
	movl	32(%esp),%ecx		/* cnt */

/* We can use faster code for shift-by-1 under certain conditions.  */
	cmp	$1,%ecx
	jne	Lnormal
	leal	4(%esi),%eax
	cmpl	%edi,%eax
	jnc	Lspecial		/* jump if s_ptr + 1 >= res_ptr */
	leal	(%esi,%ebp,4),%eax
	cmpl	%eax,%edi
	jnc	Lspecial		/* jump if res_ptr >= s_ptr + size */

Lnormal:
	leal	-4(%edi,%ebp,4),%edi
	leal	-4(%esi,%ebp,4),%esi

	movl	(%esi),%edx
	subl	$4,%esi
	xorl	%eax,%eax
	shldl	%cl,%edx,%eax		/* compute carry limb */
	pushl	%eax			/* push carry limb onto stack */

	decl	%ebp
	pushl	%ebp
	shrl	$3,%ebp
	jz	Lend

	movl	(%edi),%eax		/* fetch destination cache line */

	ALIGN	(2)
Loop:	movl	-28(%edi),%eax		/* fetch destination cache line */
	movl	%edx,%ebx

	movl	(%esi),%eax
	movl	-4(%esi),%edx
	shldl	%cl,%eax,%ebx
	shldl	%cl,%edx,%eax
	movl	%ebx,(%edi)
	movl	%eax,-4(%edi)

	movl	-8(%esi),%ebx
	movl	-12(%esi),%eax
	shldl	%cl,%ebx,%edx
	shldl	%cl,%eax,%ebx
	movl	%edx,-8(%edi)
	movl	%ebx,-12(%edi)

	movl	-16(%esi),%edx
	movl	-20(%esi),%ebx
	shldl	%cl,%edx,%eax
	shldl	%cl,%ebx,%edx
	movl	%eax,-16(%edi)
	movl	%edx,-20(%edi)

	movl	-24(%esi),%eax
	movl	-28(%esi),%edx
	shldl	%cl,%eax,%ebx
	shldl	%cl,%edx,%eax
	movl	%ebx,-24(%edi)
	movl	%eax,-28(%edi)

	subl	$32,%esi
	subl	$32,%edi
	decl	%ebp
	jnz	Loop

Lend:	popl	%ebp
	andl	$7,%ebp
	jz	Lend2
Loop2:	movl	(%esi),%eax
	shldl	%cl,%eax,%edx
	movl	%edx,(%edi)
	movl	%eax,%edx
	subl	$4,%esi
	subl	$4,%edi
	decl	%ebp
	jnz	Loop2

Lend2:	shll	%cl,%edx		/* compute least significant limb */
	movl	%edx,(%edi)		/* store it */

	popl	%eax			/* pop carry limb */

	popl	%ebp
	popl	%ebx
	popl	%esi
	popl	%edi
	ret

/* We loop from least significant end of the arrays, which is only
   permissable if the source and destination don't overlap, since the
   function is documented to work for overlapping source and destination.
*/

Lspecial:
	movl	(%esi),%edx
	addl	$4,%esi

	decl	%ebp
	pushl	%ebp
	shrl	$3,%ebp

	addl	%edx,%edx
	incl	%ebp
	decl	%ebp
	jz	LLend

	movl	(%edi),%eax		/* fetch destination cache line */

	ALIGN	(2)
LLoop:	movl	28(%edi),%eax		/* fetch destination cache line */
	movl	%edx,%ebx

	movl	(%esi),%eax
	movl	4(%esi),%edx
	adcl	%eax,%eax
	movl	%ebx,(%edi)
	adcl	%edx,%edx
	movl	%eax,4(%edi)

	movl	8(%esi),%ebx
	movl	12(%esi),%eax
	adcl	%ebx,%ebx
	movl	%edx,8(%edi)
	adcl	%eax,%eax
	movl	%ebx,12(%edi)

	movl	16(%esi),%edx
	movl	20(%esi),%ebx
	adcl	%edx,%edx
	movl	%eax,16(%edi)
	adcl	%ebx,%ebx
	movl	%edx,20(%edi)

	movl	24(%esi),%eax
	movl	28(%esi),%edx
	adcl	%eax,%eax
	movl	%ebx,24(%edi)
	adcl	%edx,%edx
	movl	%eax,28(%edi)

	leal	32(%esi),%esi		/* use leal not to clobber carry */
	leal	32(%edi),%edi
	decl	%ebp
	jnz	LLoop

LLend:	popl	%ebp
	sbbl	%eax,%eax		/* save carry in %eax */
	andl	$7,%ebp
	jz	LLend2
	addl	%eax,%eax		/* restore carry from eax */
LLoop2:	movl	%edx,%ebx
	movl	(%esi),%edx
	adcl	%edx,%edx
	movl	%ebx,(%edi)

	leal	4(%esi),%esi		/* use leal not to clobber carry */
	leal	4(%edi),%edi
	decl	%ebp
	jnz	LLoop2

	jmp	LL1
LLend2:	addl	%eax,%eax		/* restore carry from eax */
LL1:	movl	%edx,(%edi)		/* store last limb */

	sbbl	%eax,%eax
	negl	%eax

	popl	%ebp
	popl	%ebx
	popl	%esi
	popl	%edi
	ret
