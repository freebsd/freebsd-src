/*-
 * Copyright (c) 1998 Doug Rabson
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

#ifndef _MACHINE_FPU_H_
#define _MACHINE_FPU_H_

/*
 * Floating point control register bits.
 *
 * From Alpha AXP Architecture Reference Manual, Instruction
 * Descriptions (I) PP 4-69. 
 */

#define FPCR_INVD	(1LL << 49)	/* Invalid Operation DIsable */
#define FPCR_DZED	(1LL << 50)	/* Division by Zero Disable */
#define FPCR_OVFD	(1LL << 51)	/* Overflow Disable */
#define FPCR_INV	(1LL << 52)	/* Invalid Operation */
#define FPCR_DZE	(1LL << 53)	/* Division by Zero */
#define FPCR_OVF	(1LL << 54)	/* Overflow */
#define FPCR_UNF	(1LL << 55)	/* Underflow */
#define FPCR_INE	(1LL << 56)	/* Inexact Result */
#define FPCR_IOV	(1LL << 57)	/* Integer Overflow */
#define FPCR_DYN_CHOPPED (0LL << 58)	/* Chopped rounding mode */
#define FPCR_DYN_MINUS	(1LL << 58)	/* Minus infinity */
#define FPCR_DYN_NORMAL (2LL << 58)	/* Normal rounding */
#define FPCR_DYN_PLUS	(3LL << 58)	/* Plus infinity */
#define FPCR_DYN_MASK	(3LL << 58)	/* Rounding mode mask */
#define FPCR_DYN_SHIFT	58
#define FPCR_UNDZ	(1LL << 60)	/* Underflow to Zero */
#define FPCR_UNFD	(1LL << 61)	/* Underflow Disable */
#define FPCR_INED	(1LL << 62)	/* Inexact Disable */
#define FPCR_SUM	(1LL << 63)	/* Summary Bit */
#define FPCR_MASK	(~0LL << 49)

/*
 * Exception summary bits.
 *
 * From Alpha AXP Architecture Reference Manual, DEC OSF/1 Exceptions
 * and Interrupts (II-B) PP 5-5.
 */

#define EXCSUM_SWC	(1LL << 0)	/* Software completion */
#define EXCSUM_INV	(1LL << 1)	/* Invalid operation */
#define EXCSUM_DZE	(1LL << 2)	/* Division by zero */
#define EXCSUM_OVF	(1LL << 3)	/* Overflow */
#define EXCSUM_UNF	(1LL << 4)	/* Underflow */
#define EXCSUM_INE	(1LL << 5)	/* Inexact result */
#define EXCSUM_IOV	(1LL << 6)	/* Integer overflow */

/*
 * Definitions for IEEE trap enables.  These are implemented in
 * software and should be compatible with OSF/1 and Linux.
 */

/* read/write flags */
#define IEEE_TRAP_ENABLE_INV	(1LL << 1) /* Invalid operation */
#define IEEE_TRAP_ENABLE_DZE	(1LL << 2) /* Division by zero */
#define IEEE_TRAP_ENABLE_OVF	(1LL << 3) /* Overflow */
#define IEEE_TRAP_ENABLE_UNF	(1LL << 4) /* Underflow */
#define IEEE_TRAP_ENABLE_INE	(1LL << 5) /* Inexact result */
#define IEEE_TRAP_ENABLE_MASK	(IEEE_TRAP_ENABLE_INV		\
				 | IEEE_TRAP_ENABLE_DZE		\
				 | IEEE_TRAP_ENABLE_OVF		\
				 | IEEE_TRAP_ENABLE_UNF		\
				 | IEEE_TRAP_ENABLE_INE)

/* read only flags */
#define IEEE_STATUS_INV		(1LL << 17) /* Invalid operation */
#define IEEE_STATUS_DZE		(1LL << 18) /* Division by zero */
#define IEEE_STATUS_OVF		(1LL << 19) /* Overflow */
#define IEEE_STATUS_UNF		(1LL << 20) /* Underflow */
#define IEEE_STATUS_INE		(1LL << 21) /* Inexact result */
#define IEEE_STATUS_MASK	(IEEE_STATUS_INV		\
				 | IEEE_STATUS_DZE		\
				 | IEEE_STATUS_OVF		\
				 | IEEE_STATUS_UNF		\
				 | IEEE_STATUS_INE)
#define IEEE_STATUS_TO_EXCSUM_SHIFT	16 /* convert to excsum */
#define IEEE_STATUS_TO_FPCR_SHIFT	35 /* convert to fpcr */

#define IEEE_INHERIT		(1LL << 63) /* inherit on fork */

/* read and write floating point control register */
#define GET_FPCR(x) \
	__asm__("trapb"); \
	__asm__("mf_fpcr %0" : "=f" (x)); \
	__asm__("trapb")
#define SET_FPCR(x) \
	__asm__("trapb"); \
	__asm__("mt_fpcr %0" : : "f" (x)); \
	__asm__("trapb")

#ifdef _KERNEL
extern int fp_software_completion(u_int64_t regmask, struct thread *td);
#endif

#endif /* ! _MACHINE_FPU_H_ */
