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
 * $FreeBSD$
 */

	.data
	ALIGN_DATA

/*
 * Note:
 *	This is the UP equivilant of _imen.
 *	It is OPAQUE, and must NOT be accessed directly.
 *	It MUST be accessed along with the IO APIC as a 'critical region'.
 *	Accessed by:
 *		INTREN()
 *		INTRDIS()
 *		imen_dump()
 */
	.p2align 2				/* MUST be 32bit aligned */
	.globl apic_imen
apic_imen:
	.long	HWI_MASK

	.text
	SUPERALIGN_TEXT

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
	call	panic ;		\
1:

bad_mask:	.asciz	"bad mask"
#else
#define QUALIFY_MASK
#endif

/*
 * MP-safe function to clear ONE INT mask bit.
 * The passed arg is a 32bit u_int MASK.
 * It sets the associated bit in _apic_imen.
 * It sets the mask bit of the associated IO APIC register.
 */
ENTRY(INTREN)
	movl	4(%esp), %eax		/* mask into %eax */
	bsfl	%eax, %ecx		/* get pin index */
	btrl	%ecx, apic_imen		/* update apic_imen */

	QUALIFY_MASK

	shll	$4, %ecx
	movl	CNAME(int_to_apicintpin) + 8(%ecx), %edx
	movl	CNAME(int_to_apicintpin) + 12(%ecx), %ecx
	testl	%edx, %edx
	jz	1f

	movl	%ecx, (%edx)		/* write the target register index */
	movl	IOAPIC_WINDOW(%edx), %eax /* read the target register data */
	andl	$~IOART_INTMASK, %eax	/* clear mask bit */
	movl	%eax, IOAPIC_WINDOW(%edx) /* write the APIC register data */
1:	
	ret

/*
 * MP-safe function to set ONE INT mask bit.
 * The passed arg is a 32bit u_int MASK.
 * It clears the associated bit in _apic_imen.
 * It clears the mask bit of the associated IO APIC register.
 */
ENTRY(INTRDIS)
	movl	4(%esp), %eax		/* mask into %eax */
	bsfl	%eax, %ecx		/* get pin index */
	btsl	%ecx, apic_imen		/* update apic_imen */

	QUALIFY_MASK

	shll	$4, %ecx
	movl	CNAME(int_to_apicintpin) + 8(%ecx), %edx
	movl	CNAME(int_to_apicintpin) + 12(%ecx), %ecx
	testl	%edx, %edx
	jz	1f

	movl	%ecx, (%edx)		/* write the target register index */
	movl	IOAPIC_WINDOW(%edx), %eax /* read the target register data */
	orl	$IOART_INTMASK, %eax	/* set mask bit */
	movl	%eax, IOAPIC_WINDOW(%edx) /* write the APIC register data */
1:	
	ret
