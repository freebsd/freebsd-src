/* $FreeBSD: src/sys/alpha/include/intrcnt.h,v 1.3 1999/12/29 04:27:58 peter Exp $ */
/* $NetBSD: intrcnt.h,v 1.17 1998/11/19 01:48:04 ross Exp $ */

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#define	INTRCNT_CLOCK		0
#define	INTRCNT_ISA_IRQ		(INTRCNT_CLOCK + 1)
#define	INTRCNT_ISA_IRQ_LEN	16
#define	INTRCNT_OTHER_BASE	(INTRCNT_ISA_IRQ + INTRCNT_ISA_IRQ_LEN)
#define	INTRCNT_OTHER_LEN	48
#define	INTRCNT_COUNT (INTRCNT_OTHER_BASE + INTRCNT_OTHER_LEN)

#define	INTRCNT_A12_IRQ			INTRCNT_OTHER_BASE
#define	INTRCNT_DEC_1000A_IRQ		INTRCNT_OTHER_BASE
#define	INTRCNT_DEC_1000_IRQ		INTRCNT_OTHER_BASE
#define	INTRCNT_DEC_2100_A500_IRQ	INTRCNT_OTHER_BASE
#define	INTRCNT_DEC_550_IRQ		INTRCNT_OTHER_BASE
#define	INTRCNT_EB164_IRQ		INTRCNT_OTHER_BASE
#define	INTRCNT_EB64PLUS_IRQ		INTRCNT_OTHER_BASE
#define	INTRCNT_EB66_IRQ		INTRCNT_OTHER_BASE
#define	INTRCNT_IOASIC			INTRCNT_OTHER_BASE
#define	INTRCNT_KN15			INTRCNT_OTHER_BASE
#define	INTRCNT_KN16			INTRCNT_OTHER_BASE
#define	INTRCNT_KN20AA_IRQ		INTRCNT_OTHER_BASE
#define	INTRCNT_KN300_IRQ		INTRCNT_OTHER_BASE
#define	INTRCNT_KN8AE_IRQ		INTRCNT_OTHER_BASE
#define	INTRCNT_TCDS			INTRCNT_OTHER_BASE

#define	INTRCNT_A12_IRQ_LEN		10
#define	INTRCNT_DEC_1000A_IRQ_LEN	32
#define	INTRCNT_DEC_1000_IRQ_LEN	16
#define	INTRCNT_DEC_2100_A500_IRQ_LEN	16
#define	INTRCNT_DEC_550_IRQ_LEN		48
#define	INTRCNT_EB164_IRQ_LEN		24
#define	INTRCNT_EB64PLUS_IRQ_LEN	32
#define	INTRCNT_EB66_IRQ_LEN		32
#define	INTRCNT_IOASIC_LEN		4
#define	INTRCNT_ISA_IRQ_LEN		16
#define	INTRCNT_KN15_LEN		9
#define	INTRCNT_KN16_LEN		5
#define	INTRCNT_KN20AA_IRQ_LEN		32
#define	INTRCNT_KN300_LEN		19
#define	INTRCNT_KN8AE_IRQ_LEN		2
#define	INTRCNT_TCDS_LEN		2

#	define	INTRCNT_KN300_NCR810	INTRCNT_KN300_IRQ + 16
#	define	INTRCNT_KN300_I2C_CTRL	INTRCNT_KN300_IRQ + 17
#	define	INTRCNT_KN300_I2C_BUS	INTRCNT_KN300_IRQ + 18

#ifdef _KERNEL
#ifndef _LOCORE
extern volatile long intrcnt[];
#endif
#endif
