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
 *	$Id: apic_ipl.s,v 1.16 1997/07/23 05:40:55 smp Exp smp $
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
	cli
	movl	_mask8254, %eax		/* lazy masking */
	notl	%eax
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
	andl	$~IRQ_BIT(0), iactive ;	/* lazy masking */
	MEXITCOUNT
	jmp	_Xintr0			/* XXX might need _Xfastintr0 */
#else
	.globl	_vec8254
_vec8254:
	popl	%eax			/* return address */
	pushfl
	pushl	$KCSEL
	pushl	%eax
	cli
	movl	_mask8254, %eax		/* lazy masking */
	notl	%eax
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
	andl	$~IRQ_BIT(8), iactive ;	/* lazy masking */
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
#define QUALIFY_MASKS

#ifdef QUALIFY_MASKS
#define QUALIFY_MASK		\
	btrl %ecx, %eax ;	\
	andl %eax, %eax ;	\
	jz 1f ;			\
	pushl $bad_mask ;	\
	call _panic ;		\
1:

bad_mask:	.asciz	"bad mask"
#else
#define QUALIFY_MASK
#endif

/*
 * MULTIPLE_IOAPICSXXX: cannot assume apic #0 in the following function.
 * (soon to be) MP-safe function to clear ONE INT mask bit.
 * The passed arg is a 32bit u_int MASK.
 * It sets the associated bit in imen.
 * It sets the mask bit of the associated IO APIC register.
 */
	ALIGN_TEXT
	.globl _INTREN
_INTREN:
	movl 4(%esp), %eax		/* mask into %eax */
	bsfl %eax, %ecx			/* get pin index */
	btrl %ecx, _imen		/* update imen */

	QUALIFY_MASK

	leal 16(,%ecx,2), %ecx		/* calculate register index */

	movl $0, %edx			/* XXX FIXME: APIC # */
	movl _ioapic(,%edx,4), %edx	/* %edx holds APIC base address */

	movl %ecx, (%edx)		/* write the target register index */
	movl 16(%edx), %eax		/* read the target register data */
	andl $~IOART_INTMASK, %eax	/* clear mask bit */
	movl %eax, 16(%edx)		/* write the APIC register data */

	ret

/*
 * MULTIPLE_IOAPICSXXX: cannot assume apic #0 in the following function.
 * (soon to be) MP-safe function to set ONE INT mask bit.
 * The passed arg is a 32bit u_int MASK.
 * It clears the associated bit in imen.
 * It clears the mask bit of the associated IO APIC register.
 */
	ALIGN_TEXT
	.globl _INTRDIS
_INTRDIS:
	movl 4(%esp), %eax		/* mask into %eax */
	bsfl %eax, %ecx			/* get pin index */
	btsl %ecx, _imen		/* update imen */

	QUALIFY_MASK

	leal 16(,%ecx,2), %ecx		/* calculate register index */

	movl $0, %edx			/* XXX FIXME: APIC # */
	movl _ioapic(,%edx,4), %edx	/* %edx holds APIC base address */

	movl %ecx, (%edx)		/* write the target register index */
	movl 16(%edx), %eax		/* read the target register data */
	orl $IOART_INTMASK, %eax	/* set mask bit */
	movl %eax, 16(%edx)		/* write the APIC register data */

	ret


/******************************************************************************
 *
 */


/*
 * void clr_ioapic_maskbit(int apic, int bit); 
 */
	.align 2
clr_ioapic_maskbit:
	ret

/*
 * void set_ioapic_maskbit(int apic, int bit); 
 */
	.align 2
set_ioapic_maskbit:
	ret


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

	movl _imen, %ebx
	xorl _MASK, %ebx		/* %ebx = imen ^ mask */
	andl $_PIN_MASK, %ebx		/* %ebx = imen & 0x00ffffff */
	jz all_done			/* no change, return */

	movl _APIC, %esi		/* APIC # */
	movl _ioapic(,%esi,4), %esi	/* %esi holds APIC base address */

next_loop:				/* %ebx = diffs, %esi = APIC base */
	bsfl %ebx, %ecx			/* %ecx = index if 1st/next set bit */
	jz all_done

	btrl %ecx, %ebx			/* clear this bit in diffs */
	leal 16(,%ecx,2), %edx		/* calculate register index */

	movl %edx, (%esi)		/* write the target register index */
	movl 16(%esi), %eax		/* read the target register data */

	btl %ecx, _MASK			/* test for mask or unmask */
	jnc clear			/* bit is clear */
	orl $_INT_MASK, %eax		/* set mask bit */
	jmp write
clear:	andl $~_INT_MASK, %eax		/* clear mask bit */

write:	movl %eax, 16(%esi)		/* write the APIC register data */

	jmp next_loop			/* try another pass */

all_done:
	popl %esi
	popl %ebx
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
	movl _imen, %eax
	notl %eax			/* mask = ~mask */
	andl _imen, %eax		/* %eax = imen & ~mask */

	pushl %eax			/* new (future) imen value */
	pushl $0			/* APIC# arg */
	call write_ioapic_mask		/* modify the APIC registers */

	addl $4, %esp			/* remove APIC# arg from stack */
	popl _imen			/* imen |= mask */
	ret

_INTRDIS:
	movl _imen, %eax
	orl 4(%esp), %eax		/* %eax = imen | mask */

	pushl %eax			/* new (future) imen value */
	pushl $0			/* APIC# arg */
	call write_ioapic_mask		/* modify the APIC registers */

	addl $4, %esp			/* remove APIC# arg from stack */
	popl _imen			/* imen |= mask */
	ret

#endif /* oldcode */


#ifdef ready

/*
 * u_int read_io_apic_mask(int apic); 
 */
	.align 2
read_io_apic_mask:
	ret

/*
 * Set INT mask bit for each bit set in 'mask'.
 * Ignore INT mask bit for all others.
 * Only consider lower 24 bits in mask.
 *
 * void set_io_apic_mask24(apic, u_int32_t bits); 
 */
	.align 2
set_io_apic_mask:
	ret

/*
 * Clear INT mask bit for each bit set in 'mask'.
 * Ignore INT mask bit for all others.
 * Only consider lower 24 bits in mask.
 *
 * void clr_io_apic_mask24(int apic, u_int32_t bits); 
 */
	.align 2
clr_io_apic_mask24:
	ret

/*
 * 
 */
	.align 2

	ret

#endif /** ready */

/******************************************************************************
 * 
 */

/*
 * u_int io_apic_write(int apic, int select);
 */
	.align 2
	.globl _io_apic_read
_io_apic_read:
	movl 4(%esp), %ecx		/* APIC # */
	movl _ioapic(,%ecx,4), %edx	/* APIC base register address */
	movl 8(%esp), %eax		/* target register index */
	movl %eax, (%edx)		/* write the target register index */
	movl 16(%edx), %eax		/* read the APIC register data */
	ret				/* %eax = register value */

/*
 * void io_apic_write(int apic, int select, int value);
 */
	.align 2
	.globl _io_apic_write
_io_apic_write:
	movl 4(%esp), %ecx		/* APIC # */
	movl _ioapic(,%ecx,4), %edx	/* APIC base register address */
	movl 8(%esp), %eax		/* target register index */
	movl %eax, (%edx)		/* write the target register index */
	movl 12(%esp), %eax		/* target register value */
	movl %eax, 16(%edx)		/* write the APIC register data */
	ret				/* %eax = void */

/*
 * Send an EOI to the local APIC.
 */
	.align 2
	.globl _apic_eoi
_apic_eoi:
	movl $0, _lapic+0xb0
	ret


/******************************************************************************
 *
 */

/*
 * test_and_set(struct simplelock *lkp);
 */
	.align 2
	.globl _test_and_set
_test_and_set:
	movl 4(%esp), %eax		/* get the address of the lock */
	lock				/* do it atomically */
	btsl $0, (%eax)			/* set 0th bit, old value in CF */
	setc %al			/* previous value into %al */
	movzbl %al, %eax		/* return as an int */
	ret

/******************************************************************************
 *
 */

/*
 * The simple-lock routines are the primitives out of which the lock
 * package is built. The machine-dependent code must implement an
 * atomic test_and_set operation that indivisibly sets the simple lock
 * to non-zero and returns its old value. It also assumes that the
 * setting of the lock to zero below is indivisible. Simple locks may
 * only be used for exclusive locks.
 * 
 * struct simplelock {
 * 	int	lock_data;
 * };
 */

/*
 * void
 * s_lock_init(struct simplelock *lkp)
 * {
 * 	lkp->lock_data = 0;
 * }
 */
	.align 2
	.globl _s_lock_init
_s_lock_init:
	movl 4(%esp), %eax		/* get the address of the lock */
	xorl %ecx, %ecx			/* cheap clear */
	xchgl %ecx, (%eax)		/* in case its not 32bit aligned */
	ret


/*
 * void
 * s_lock(__volatile struct simplelock *lkp)
 * {
 * 	while (test_and_set(&lkp->lock_data))
 * 		continue;
 * }
 */
	.align 2
	.globl _s_lock
_s_lock:
	movl 4(%esp), %eax		/* get the address of the lock */
1:	lock				/* do it atomically */
	btsl $0, (%eax)			/* set 0th bit, old value in CF */
	jc 1b				/* already set, try again */
	ret


/*
 * int
 * s_lock_try(__volatile struct simplelock *lkp)
 * {
 * 	return (!test_and_set(&lkp->lock_data));
 * }
 */
	.align 2
	.globl _s_lock_try
_s_lock_try:
	movl 4(%esp), %eax		/* get the address of the lock */
	lock				/* do it atomically */
	btsl $0, (%eax)			/* set 0th bit, old value in CF */
	setnc %al			/* 1 if previous value was 0 */
	movzbl %al, %eax		/* convert to an int */
	ret


/*
 * void
 * s_unlock(__volatile struct simplelock *lkp)
 * {
 * 	lkp->lock_data = 0;
 * }
 */
	.align 2
	.globl _s_unlock
_s_unlock:
	movl 4(%esp), %eax		/* get the address of the lock */
	xorl %ecx, %ecx			/* cheap clear */
	xchgl %ecx, (%eax)		/* in case its not 32bit aligned */
	ret
