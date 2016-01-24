/*	$NetBSD: fusu.S,v 1.10 2003/12/01 13:34:44 rearnsha Exp $	*/

/*-
 * Copyright (c) 1996-1998 Mark Brinicombe.
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
 *	This product includes software developed by Mark Brinicombe
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <machine/asm.h>
#include <machine/armreg.h>
#include "assym.s"
__FBSDID("$FreeBSD$");

	.syntax	unified

#ifdef _ARM_ARCH_6
#define GET_PCB(tmp) \
	mrc p15, 0, tmp, c13, c0, 4; \
	add	tmp, tmp, #(TD_PCB)
#else
.Lcurpcb:
	.word	_C_LABEL(__pcpu) + PC_CURPCB
#define GET_PCB(tmp) \
	ldr	tmp, .Lcurpcb
#endif

/*
 * fuword(caddr_t uaddr);
 * Fetch an int from the user's address space.
 */

ENTRY(casuword)
EENTRY_NP(casuword32)
	GET_PCB(r3)
	ldr	r3, [r3]

#ifdef DIAGNOSTIC
	teq	r3, #0x00000000
	beq	.Lfusupcbfault
#endif
	stmfd	sp!, {r4, r5}
	adr	r4, .Lcasuwordfault
	str	r4, [r3, #PCB_ONFAULT]
#ifdef _ARM_ARCH_6
1:    
	cmp     r0, #KERNBASE
	mvnhs   r0, #0
	bhs     2f
	
	ldrex   r5, [r0]
	cmp     r5, r1
	movne   r0, r5
	bne     2f
	strex   r5, r2, [r0]
	cmp     r5, #0
	bne     1b
#else
	ldrt	r5, [r0]
	cmp	r5, r1
	movne	r0, r5
	strteq	r2, [r0]
#endif
	moveq	r0, r1
2:
	ldmfd	sp!, {r4, r5}
	mov	r1, #0x00000000
	str	r1, [r3, #PCB_ONFAULT]
	RET
EEND(casuword32)
END(casuword)

/*
 * Handle faults from casuword.  Clean up and return -1.
 */

.Lcasuwordfault:
	mov	r0, #0x00000000
	str	r0, [r3, #PCB_ONFAULT]
	mvn	r0, #0x00000000
	ldmfd	sp!, {r4, r5}
	RET	

/*
 * fuword(caddr_t uaddr);
 * Fetch an int from the user's address space.
 */

ENTRY(fuword)
EENTRY_NP(fuword32)
	GET_PCB(r2)
	ldr	r2, [r2]

#ifdef DIAGNOSTIC
	teq	r2, #0x00000000
	beq	.Lfusupcbfault
#endif

	adr	r1, .Lfusufault
	str	r1, [r2, #PCB_ONFAULT]

	ldrt	r3, [r0]

	mov	r1, #0x00000000
	str	r1, [r2, #PCB_ONFAULT]
	mov	r0, r3
	RET
EEND(fueword32)
END(fueword)

/*
 * fusword(caddr_t uaddr);
 * Fetch a short from the user's address space.
 */

ENTRY(fusword)
	GET_PCB(r2)
	ldr	r2, [r2]

#ifdef DIAGNOSTIC
	teq	r2, #0x00000000
	beq	.Lfusupcbfault
#endif

	adr	r1, .Lfusufault
	str	r1, [r2, #PCB_ONFAULT]

	ldrbt	r3, [r0], #1
	ldrbt	ip, [r0]
#ifdef __ARMEB__
	orr	r0, ip, r3, asl #8
#else
	orr	r0, r3, ip, asl #8
#endif
	mov	r1, #0x00000000
	str	r1, [r2, #PCB_ONFAULT]
	RET
END(fusword)

/*
 * fuswintr(caddr_t uaddr);
 * Fetch a short from the user's address space.  Can be called during an
 * interrupt.
 */

ENTRY(fuswintr)
	ldr	r2, Lblock_userspace_access
	ldr	r2, [r2]
	teq	r2, #0
	mvnne	r0, #0x00000000
	RETne

	GET_PCB(r2)
	ldr	r2, [r2]

#ifdef DIAGNOSTIC
	teq	r2, #0x00000000
	beq	.Lfusupcbfault
#endif

	adr	r1, _C_LABEL(fusubailout)
	str	r1, [r2, #PCB_ONFAULT]

	ldrbt	r3, [r0], #1
	ldrbt	ip, [r0]
#ifdef __ARMEB__
	orr	r0, ip, r3, asl #8
#else
	orr	r0, r3, ip, asl #8
#endif

	mov	r1, #0x00000000
	str	r1, [r2, #PCB_ONFAULT]
	RET
END(fuswintr)

Lblock_userspace_access:
	.word	_C_LABEL(block_userspace_access)

	.data
	.align	2
	.global	_C_LABEL(block_userspace_access)
_C_LABEL(block_userspace_access):
	.word	0
	.text

/*
 * fubyte(caddr_t uaddr);
 * Fetch a byte from the user's address space.
 */

ENTRY(fubyte)
	GET_PCB(r2)
	ldr	r2, [r2]

#ifdef DIAGNOSTIC
	teq	r2, #0x00000000
	beq	.Lfusupcbfault
#endif

	adr	r1, .Lfusufault
	str	r1, [r2, #PCB_ONFAULT]

	ldrbt	r3, [r0]

	mov	r1, #0x00000000
	str	r1, [r2, #PCB_ONFAULT]
	mov	r0, r3
	RET
END(fubyte)

/*
 * Handle faults from [fs]u*().  Clean up and return -1.
 */

.Lfusufault:
	mov	r0, #0x00000000
	str	r0, [r2, #PCB_ONFAULT]
	mvn	r0, #0x00000000
	RET

/*
 * Handle faults from [fs]u*().  Clean up and return -1.  This differs from
 * fusufault() in that trap() will recognise it and return immediately rather
 * than trying to page fault.
 */

/* label must be global as fault.c references it */
	.global	_C_LABEL(fusubailout)
_C_LABEL(fusubailout):
	mov	r0, #0x00000000
	str	r0, [r2, #PCB_ONFAULT]
	mvn	r0, #0x00000000
	RET

#ifdef DIAGNOSTIC
/*
 * Handle earlier faults from [fs]u*(), due to no pcb
 */

.Lfusupcbfault:
	mov	r1, r0
	adr	r0, fusupcbfaulttext
	b	_C_LABEL(panic)

fusupcbfaulttext:
	.asciz	"Yikes - no valid PCB during fusuxxx() addr=%08x\n"
	.align	2
#endif

/*
 * suword(caddr_t uaddr, int x);
 * Store an int in the user's address space.
 */

ENTRY(suword)
EENTRY_NP(suword32)
	GET_PCB(r2)
	ldr	r2, [r2]

#ifdef DIAGNOSTIC
	teq	r2, #0x00000000
	beq	.Lfusupcbfault
#endif

	adr	r3, .Lfusufault
	str	r3, [r2, #PCB_ONFAULT]

	strt	r1, [r0]

	mov	r0, #0x00000000
	str	r0, [r2, #PCB_ONFAULT]
	RET
EEND(suword32)
END(suword)

/*
 * suswintr(caddr_t uaddr, short x);
 * Store a short in the user's address space.  Can be called during an
 * interrupt.
 */

ENTRY(suswintr)
	ldr	r2, Lblock_userspace_access
	ldr	r2, [r2]
	teq	r2, #0
	mvnne	r0, #0x00000000
	RETne

	GET_PCB(r2)
	ldr	r2, [r2]

#ifdef DIAGNOSTIC
	teq	r2, #0x00000000
	beq	.Lfusupcbfault
#endif

	adr	r3, _C_LABEL(fusubailout)
	str	r3, [r2, #PCB_ONFAULT]

#ifdef __ARMEB__
	mov	ip, r1, lsr #8
	strbt	ip, [r0], #1
#else
	strbt	r1, [r0], #1
	mov	r1, r1, lsr #8
#endif
	strbt	r1, [r0]

	mov	r0, #0x00000000
	str	r0, [r2, #PCB_ONFAULT]
	RET
END(suswintr)

/*
 * susword(caddr_t uaddr, short x);
 * Store a short in the user's address space.
 */

ENTRY(susword)
	GET_PCB(r2)
	ldr	r2, [r2]

#ifdef DIAGNOSTIC
	teq	r2, #0x00000000
	beq	.Lfusupcbfault
#endif

	adr	r3, .Lfusufault
	str	r3, [r2, #PCB_ONFAULT]

#ifdef __ARMEB__
	mov	ip, r1, lsr #8
	strbt	ip, [r0], #1
#else
	strbt	r1, [r0], #1
	mov	r1, r1, lsr #8
#endif
	strbt	r1, [r0]

	mov	r0, #0x00000000
	str	r0, [r2, #PCB_ONFAULT]
	RET
END(susword)

/*
 * subyte(caddr_t uaddr, char x);
 * Store a byte in the user's address space.
 */

ENTRY(subyte)
	GET_PCB(r2)
	ldr	r2, [r2]


#ifdef DIAGNOSTIC
	teq	r2, #0x00000000
	beq	.Lfusupcbfault
#endif

	adr	r3, .Lfusufault
	str	r3, [r2, #PCB_ONFAULT]

	strbt	r1, [r0]
	mov	r0, #0x00000000
	str	r0, [r2, #PCB_ONFAULT]
	RET
END(subyte)
