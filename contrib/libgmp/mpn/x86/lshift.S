/* i80386 __mpn_lshift -- 

Copyright (C) 1992, 1994 Free Software Foundation, Inc.

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

	movl	16(%esp),%edi		/* res_ptr */
	movl	20(%esp),%esi		/* s_ptr */
	movl	24(%esp),%edx		/* size */
	movl	28(%esp),%ecx		/* cnt */

	subl	$4,%esi			/* adjust s_ptr */

	movl	(%esi,%edx,4),%ebx	/* read most significant limb */
	xorl	%eax,%eax
	shldl	%cl,%ebx,%eax		/* compute carry limb */
	decl	%edx
	jz	Lend
	pushl	%eax			/* push carry limb onto stack */
	testb	$1,%edx
	jnz	L1			/* enter loop in the middle */
	movl	%ebx,%eax

	ALIGN (3)
Loop:	movl	(%esi,%edx,4),%ebx	/* load next lower limb */
	shldl	%cl,%ebx,%eax		/* compute result limb */
	movl	%eax,(%edi,%edx,4)	/* store it */
	decl	%edx
L1:	movl	(%esi,%edx,4),%eax
	shldl	%cl,%eax,%ebx
	movl	%ebx,(%edi,%edx,4)
	decl	%edx
	jnz	Loop

	shll	%cl,%eax		/* compute least significant limb */
	movl	%eax,(%edi)		/* store it */

	popl	%eax			/* pop carry limb */

	popl	%ebx
	popl	%esi
	popl	%edi
	ret

Lend:	shll	%cl,%ebx		/* compute least significant limb */
	movl	%ebx,(%edi)		/* store it */

	popl	%ebx
	popl	%esi
	popl	%edi
	ret
