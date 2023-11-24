/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 *
 */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc. */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T		*/
/*	All Rights Reserved	*/

#ifndef	_REGSET_H
#define	_REGSET_H

/*
 * #pragma ident	"@(#)regset.h	1.11	05/06/08 SMI"
 */

#ifdef __cplusplus
extern "C" {
#endif

#define REG_ZERO	0
#define REG_RA		1
#define REG_SP		2
#define REG_GP		3
#define REG_TP		4
#define REG_T0		5
#define REG_T1		6
#define REG_T2		7
#define REG_S0		8
#define REG_FP		8
#define REG_S1		9
#define REG_A0		10
#define REG_A1		11
#define REG_A2		12
#define REG_A3		13
#define REG_A4		14
#define REG_A5		15
#define REG_A6		16
#define REG_A7		17
#define REG_S2		18
#define REG_S3		19
#define REG_S4		20
#define REG_S5		21
#define REG_S6		22
#define REG_S7		23
#define REG_S8		24
#define REG_S9		25
#define REG_S10		26
#define REG_S11		27
#define REG_T3		28
#define REG_T4		29
#define REG_T5		30
#define REG_T6		31
#define REG_PC		32

#ifdef	__cplusplus
}
#endif

#endif	/* _REGSET_H */
