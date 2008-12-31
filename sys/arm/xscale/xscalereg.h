/*	$NetBSD: xscalereg.h,v 1.2 2002/08/07 05:15:02 briggs Exp $	*/

/*-
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/arm/xscale/xscalereg.h,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _ARM_XSCALE_XSCALEREG_H_ 
#define _ARM_XSCALE_XSCALEREG_H_ 

/*
 * Register definitions for the Intel XScale processor core.
 */

/*
 * Performance Monitoring Unit		(CP14)
 *
 *	CP14.0		Performance Monitor Control Register
 *	CP14.1		Clock Counter
 *	CP14.2		Performance Counter Register 0
 *	CP14.3		Performance Counter Register 1
 */

#define	PMNC_E		0x00000001	/* enable counters */
#define	PMNC_P		0x00000002	/* reset both PMNs to 0 */
#define	PMNC_C		0x00000004	/* clock counter reset */
#define	PMNC_D		0x00000008	/* clock counter / 64 */
#define	PMNC_PMN0_IE	0x00000010	/* enable PMN0 interrupt */
#define	PMNC_PMN1_IE	0x00000020	/* enable PMN1 interrupt */
#define	PMNC_CC_IE	0x00000040	/* enable clock counter interrupt */
#define	PMNC_PMN0_IF	0x00000100	/* PMN0 overflow/interrupt */
#define	PMNC_PMN1_IF	0x00000200	/* PMN1 overflow/interrupt */
#define	PMNC_CC_IF	0x00000400	/* clock counter overflow/interrupt */
#define	PMNC_EVCNT0_MASK 0x000ff000	/* event to count for PMN0 */
#define	PMNC_EVCNT0_SHIFT 12
#define	PMNC_EVCNT1_MASK 0x0ff00000	/* event to count for PMN1 */
#define	PMNC_EVCNT1_SHIFT 20

void	xscale_pmu_init(void);

#endif /* _ARM_XSCALE_XSCALEREG_H_ */
