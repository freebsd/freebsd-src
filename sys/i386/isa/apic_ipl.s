/*-
 * Copyright (c) 1997, by Steve Passe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 *	$Id: apic_ipl.s,v 1.9 1997/07/23 21:25:31 fsmp Exp $
 */


#include <machine/smptests.h>		/** NEW_STRATEGY, APIC_PIN0_TIMER */

	.data
	ALIGN_DATA

#ifdef NEW_STRATEGY

/* this allows us to change the 8254 APIC pin# assignment */
	.globl _Xintr8254
_Xintr8254:
	.long	_Xintr7

/* used by this file, microtime.s and clock.c */
	.globl _mask8254
_mask8254:
	.long	0

#else /** NEW_STRATEGY */

#ifndef APIC_PIN0_TIMER
/* this allows us to change the 8254 APIC pin# assignment */
	.globl _Xintr8254
_Xintr8254:
	.long	_Xintr7

/* used by this file, microtime.s and clock.c */
	.globl _mask8254
_mask8254:
	.long	0
#endif /* APIC_PIN0_TIMER */

#endif /** NEW_STRATEGY */

/*  */
	.globl _vec
_vec:
	.long	 vec0,  vec1,  vec2,  vec3,  vec4,  vec5,  vec6,  vec7
	.long	 vec8,  vec9, vec10, vec11, vec12, vec13, vec14, vec15
	.long	vec16, vec17, vec18, vec19, vec20, vec21, vec22, vec23

	.align 4				/* MUST be 32bit aligned */
	.globl _imen_lock
_imen_lock:
	.long	0

/*
 * Note:
 *	This is the UP equivilant of _imen.
 *	It is OPAQUE, and must NOT be accessed directly.
 *	It MUST be accessed along with the IO APIC as a 'critical region'.
 *	Accessed by:
 *		INTREN()
 *		INTRDIS()
 *		MAYBE_MASK_IRQ
 *		MAYBE_UNMASK_IRQ
 *		imen_dump()
 */
	.globl _apic_imen
_apic_imen:
	.long	HWI_MASK

/*
 * 
 */
	.text
	SUPERALIGN_TEXT

/*
 * Fake clock interrupt(s) so that they appear to come from our caller instead
 * of from here, so that system profiling works.
 * XXX do this more generally (for all vectors; look up the C entry point).
 * XXX frame bogusness stops us from just jumping to the C entry point.
 */

/*
 * generic vector function for 8254 clock
 */
	ALIGN_TEXT
#ifdef NEW_STRATEGY

	.globl	_vec8254
_vec8254:
	popl	%eax			/* return address */
	pushfl
	pushl	$KCSEL
	pushl	%eax
	movl	_mask8254, %eax		/* lazy masking */
	notl	%eax
	cli
	lock				/* MP-safe */
	andl	%eax, iactive
	MEXITCOUNT
	movl	_Xintr8254, %eax
	jmp	%eax			/* XXX might need _Xfastintr# */

#else /** NEW_STRATEGY */

#ifdef APIC_PIN0_TIMER
vec0:
	popl	%eax			/* return address */
	pushfl
	pushl	$KCSEL
	pushl	%eax
	cli
	lock				/* MP-safe */
	andl	$~IRQ_BIT(0), iactive	/* lazy masking */
	MEXITCOUNT
	jmp	_Xintr0			/* XXX might need _Xfastintr0 */
#else
	.globl	_vec8254
_vec8254:
	popl	%eax			/* return address */
	pushfl
	pushl	$KCSEL
	pushl	%eax
	movl	_mask8254, %eax		/* lazy masking */
	notl	%eax
	cli
	lock				/* MP-safe */
	andl	%eax, iactive
	MEXITCOUNT
	movl	_Xintr8254, %eax
	jmp	%eax			/* XXX might need _Xfastintr# */
#endif /* APIC_PIN0_TIMER */

#endif /** NEW_STRATEGY */


/*
 * generic vector function for RTC clock
 */
	ALIGN_TEXT
vec8:
	popl	%eax	
	pushfl
	pushl	$KCSEL
	pushl	%eax
	cli
	lock				/* MP-safe */
	andl	$~IRQ_BIT(8), iactive	/* lazy masking */
	MEXITCOUNT
	jmp	_Xintr8			/* XXX might need _Xfastintr8 */

/*
 * The 'generic' vector stubs.
 */

#define BUILD_VEC(irq_num)						\
	ALIGN_TEXT ;							\
__CONCAT(vec,irq_num): ;						\
	popl	%eax ;							\
	pushfl ;							\
	pushl	$KCSEL ;						\
	pushl	%eax ;							\
	cli ;								\
	lock ;					/* MP-safe */		\
	andl	$~IRQ_BIT(irq_num), iactive ;	/* lazy masking */	\
	MEXITCOUNT ;							\
	jmp	__CONCAT(_Xintr,irq_num)


#ifdef NEW_STRATEGY

	BUILD_VEC(0)

#else /** NEW_STRATEGY */

#ifndef APIC_PIN0_TIMER
	BUILD_VEC(0)
#endif /* APIC_PIN0_TIMER */

#endif /** NEW_STRATEGY */
	BUILD_VEC(1)
	BUILD_VEC(2)
	BUILD_VEC(3)
	BUILD_VEC(4)
	BUILD_VEC(5)
	BUILD_VEC(6)
	BUILD_VEC(7)
	/* IRQ8 is special case, done above */
	BUILD_VEC(9)
	BUILD_VEC(10)
	BUILD_VEC(11)
	BUILD_VEC(12)
	BUILD_VEC(13)
	BUILD_VEC(14)
	BUILD_VEC(15)
	BUILD_VEC(16)			/* 8 additional INTs in IO APIC */
	BUILD_VEC(17)
	BUILD_VEC(18)
	BUILD_VEC(19)
	BUILD_VEC(20)
	BUILD_VEC(21)
	BUILD_VEC(22)
	BUILD_VEC(23)


/******************************************************************************
 * XXX FIXME: figure out where these belong.
 */

/* this nonsense is to verify that masks ALWAYS have 1 and only 1 bit set */
#define QUALIFY_MASKS_NOT

#ifdef QUALIFY_MASKS
#define QUALIFY_MASK		\
	btrl	%ecx, %eax ;	\
	andl	%eax, %eax ;	\
	jz	1f ;		\
	pushl	$bad_mask ;	\
	call	_panic ;	\
1:

bad_mask:	.asciz	"bad mask"
#else
#define QUALIFY_MASK
#endif

/*
 * MULTIPLE_IOAPICSXXX: cannot assume apic #0 in the following function.
 * (soon to be) MP-safe function to clear ONE INT mask bit.
 * The passed arg is a 32bit u_int MASK.
 * It sets the associated bit in _apic_imen.
 * It sets the mask bit of the associated IO APIC register.
 */
ENTRY(INTREN)
	pushfl				/* save state of EI flag */
	cli				/* prevent recursion */
	IMASK_LOCK			/* enter critical reg */

	movl	8(%esp), %eax		/* mask into %eax */
	bsfl	%eax, %ecx		/* get pin index */
	btrl	%ecx, _apic_imen	/* update _apic_imen */

	QUALIFY_MASK

	leal	16(,%ecx,2), %ecx	/* calculate register index */

	movl	$0, %edx		/* XXX FIXME: APIC # */
	movl	_ioapic(,%edx,4), %edx	/* %edx holds APIC base address */

	movl	%ecx, (%edx)		/* write the target register index */
	movl	16(%edx), %eax		/* read the target register data */
	andl	$~IOART_INTMASK, %eax	/* clear mask bit */
	movl	%eax, 16(%edx)		/* write the APIC register data */

	IMASK_UNLOCK			/* exit critical reg */
	popfl				/* restore old state of EI flag */
	ret

/*
 * MULTIPLE_IOAPICSXXX: cannot assume apic #0 in the following function.
 * (soon to be) MP-safe function to set ONE INT mask bit.
 * The passed arg is a 32bit u_int MASK.
 * It clears the associated bit in _apic_imen.
 * It clears the mask bit of the associated IO APIC register.
 */
ENTRY(INTRDIS)
	pushfl				/* save state of EI flag */
	cli				/* prevent recursion */
	IMASK_LOCK			/* enter critical reg */

	movl	8(%esp), %eax		/* mask into %eax */
	bsfl	%eax, %ecx		/* get pin index */
	btsl	%ecx, _apic_imen	/* update _apic_imen */

	QUALIFY_MASK

	leal	16(,%ecx,2), %ecx	/* calculate register index */

	movl	$0, %edx		/* XXX FIXME: APIC # */
	movl	_ioapic(,%edx,4), %edx	/* %edx holds APIC base address */

	movl	%ecx, (%edx)		/* write the target register index */
	movl	16(%edx), %eax		/* read the target register data */
	orl	$IOART_INTMASK, %eax	/* set mask bit */
	movl	%eax, 16(%edx)		/* write the APIC register data */

	IMASK_UNLOCK			/* exit critical reg */
	popfl				/* restore old state of EI flag */
	ret


/******************************************************************************
 *
 */


/*
 * void write_ioapic_mask(int apic, u_int mask); 
 */

#define _INT_MASK	0x00010000
#define _PIN_MASK	0x00ffffff

#define _OLD_ESI	  0(%esp)
#define _OLD_EBX	  4(%esp)
#define _RETADDR	  8(%esp)
#define _APIC		 12(%esp)
#define _MASK		 16(%esp)

	.align 2
write_ioapic_mask:
	pushl %ebx			/* scratch */
	pushl %esi			/* scratch */

	movl	_apic_imen, %ebx
	xorl	_MASK, %ebx		/* %ebx = _apic_imen ^ mask */
	andl	$_PIN_MASK, %ebx	/* %ebx = _apic_imen & 0x00ffffff */
	jz	all_done		/* no change, return */

	movl	_APIC, %esi		/* APIC # */
	movl	_ioapic(,%esi,4), %esi	/* %esi holds APIC base address */

next_loop:				/* %ebx = diffs, %esi = APIC base */
	bsfl	%ebx, %ecx		/* %ecx = index if 1st/next set bit */
	jz	all_done

	btrl	%ecx, %ebx		/* clear this bit in diffs */
	leal	16(,%ecx,2), %edx	/* calculate register index */

	movl	%edx, (%esi)		/* write the target register index */
	movl	16(%esi), %eax		/* read the target register data */

	btl	%ecx, _MASK		/* test for mask or unmask */
	jnc	clear			/* bit is clear */
	orl	$_INT_MASK, %eax	/* set mask bit */
	jmp	write
clear:	andl	$~_INT_MASK, %eax	/* clear mask bit */

write:	movl	%eax, 16(%esi)		/* write the APIC register data */

	jmp	next_loop		/* try another pass */

all_done:
	popl	%esi
	popl	%ebx
	ret

#undef _OLD_ESI
#undef _OLD_EBX
#undef _RETADDR
#undef _APIC
#undef _MASK

#undef _PIN_MASK
#undef _INT_MASK

#ifdef oldcode

_INTREN:
	movl _apic_imen, %eax
	notl %eax			/* mask = ~mask */
	andl _apic_imen, %eax		/* %eax = _apic_imen & ~mask */

	pushl %eax			/* new (future) _apic_imen value */
	pushl $0			/* APIC# arg */
	call write_ioapic_mask		/* modify the APIC registers */

	addl $4, %esp			/* remove APIC# arg from stack */
	popl _apic_imen			/* _apic_imen |= mask */
	ret

_INTRDIS:
	movl _apic_imen, %eax
	orl 4(%esp), %eax		/* %eax = _apic_imen | mask */

	pushl %eax			/* new (future) _apic_imen value */
	pushl $0			/* APIC# arg */
	call write_ioapic_mask		/* modify the APIC registers */

	addl $4, %esp			/* remove APIC# arg from stack */
	popl _apic_imen			/* _apic_imen |= mask */
	ret

#endif /* oldcode */


#ifdef ready

/*
 * u_int read_io_apic_mask(int apic); 
 */
	ALIGN_TEXT
read_io_apic_mask:
	ret

/*
 * Set INT mask bit for each bit set in 'mask'.
 * Ignore INT mask bit for all others.
 *
 * void set_io_apic_mask(apic, u_int32_t bits); 
 */
	ALIGN_TEXT
set_io_apic_mask:
	ret

/*
 * void set_ioapic_maskbit(int apic, int bit); 
 */
	ALIGN_TEXT
set_ioapic_maskbit:
	ret

/*
 * Clear INT mask bit for each bit set in 'mask'.
 * Ignore INT mask bit for all others.
 *
 * void clr_io_apic_mask(int apic, u_int32_t bits); 
 */
	ALIGN_TEXT
clr_io_apic_mask:
	ret

/*
 * void clr_ioapic_maskbit(int apic, int bit); 
 */
	ALIGN_TEXT
clr_ioapic_maskbit:
	ret

#endif /** ready */

/******************************************************************************
 * 
 */

/*
 * u_int io_apic_write(int apic, int select);
 */
ENTRY(io_apic_read)
	movl	4(%esp), %ecx		/* APIC # */
	movl	_ioapic(,%ecx,4), %edx	/* APIC base register address */
	movl	8(%esp), %eax		/* target register index */
	movl	%eax, (%edx)		/* write the target register index */
	movl	16(%edx), %eax		/* read the APIC register data */
	ret				/* %eax = register value */

/*
 * void io_apic_write(int apic, int select, int value);
 */
ENTRY(io_apic_write)
	movl	4(%esp), %ecx		/* APIC # */
	movl	_ioapic(,%ecx,4), %edx	/* APIC base register address */
	movl	8(%esp), %eax		/* target register index */
	movl	%eax, (%edx)		/* write the target register index */
	movl	12(%esp), %eax		/* target register value */
	movl	%eax, 16(%edx)		/* write the APIC register data */
	ret				/* %eax = void */

/*
 * Send an EOI to the local APIC.
 */
ENTRY(apic_eoi)
	movl	$0, _lapic+0xb0
	ret
