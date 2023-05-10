/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018, 2019 Rubicon Communications, LLC (Netgate)
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

#ifndef	_A37X0_IICREG_H_
#define	_A37X0_IICREG_H_

#define	A37X0_IIC_IBMR		0x00
#define	A37X0_IIC_IDBR		0x04
#define	A37X0_IIC_ICR		0x08
#define	 ICR_START		(1 << 0)
#define	 ICR_STOP		(1 << 1)
#define	 ICR_ACKNAK		(1 << 2)
#define	 ICR_TB			(1 << 3)
#define	 ICR_MA			(1 << 4)
#define	 ICR_SCLE		(1 << 5)
#define	 ICR_IUE		(1 << 6)
#define	 ICR_GCD		(1 << 7)
#define	 ICR_ITEIE		(1 << 8)
#define	 ICR_IRFIE		(1 << 9)
#define	 ICR_BEIE		(1 << 10)
#define	 ICR_SSDIE		(1 << 11)
#define	 ICR_ALDIE		(1 << 12)
#define	 ICR_SADIE		(1 << 13)
#define	 ICR_UR			(1 << 14)
#define	 ICR_FAST_MODE		(1 << 16)
#define	 ICR_HIGH_SPEED		(1 << 17)
#define	 ICR_MODE_MASK		(ICR_FAST_MODE | ICR_HIGH_SPEED)
#define	 ICR_INIT							\
    (ICR_BEIE | ICR_IRFIE | ICR_ITEIE | ICR_GCD | ICR_SCLE)
#define	A37X0_IIC_ISR		0x0c
#define	 ISR_RWM		(1 << 0)
#define	 ISR_ACKNAK		(1 << 1)
#define	 ISR_UB			(1 << 2)
#define	 ISR_IBB		(1 << 3)
#define	 ISR_SSD		(1 << 4)
#define	 ISR_ALD		(1 << 5)
#define	 ISR_ITE		(1 << 6)
#define	 ISR_IRF		(1 << 7)
#define	 ISR_GCAD		(1 << 8)
#define	 ISR_SAD		(1 << 9)
#define	 ISR_BED		(1 << 10)

#endif	/* _A37X0_IICREG_H_ */
