/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$NetBSD: psl.h,v 1.4 2000/02/13 10:25:07 tsubai Exp $
 * $FreeBSD$
 */

#ifndef	_MACHINE_PSL_H_
#define	_MACHINE_PSL_H_

/*
 * Machine State Register (MSR)
 *
 * The PowerPC 601 does not implement the following bits:
 *
 *	POW, ILE, BE, RI, LE[*]
 *
 * [*] Little-endian mode on the 601 is implemented in the HID0 register.
 */
#define	PSL_POW		0x00040000	/* power management */
#define	PSL_ILE		0x00010000	/* interrupt endian mode (1 == le) */
#define	PSL_EE		0x00008000	/* external interrupt enable */
#define	PSL_PR		0x00004000	/* privilege mode (1 == user) */
#define	PSL_FP		0x00002000	/* floating point enable */
#define	PSL_ME		0x00001000	/* machine check enable */
#define	PSL_FE0		0x00000800	/* floating point interrupt mode 0 */
#define	PSL_SE		0x00000400	/* single-step trace enable */
#define	PSL_BE		0x00000200	/* branch trace enable */
#define	PSL_FE1		0x00000100	/* floating point interrupt mode 1 */
#define	PSL_IP		0x00000040	/* interrupt prefix */
#define	PSL_IR		0x00000020	/* instruction address relocation */
#define	PSL_DR		0x00000010	/* data address relocation */
#define	PSL_RI		0x00000002	/* recoverable interrupt */
#define	PSL_LE		0x00000001	/* endian mode (1 == le) */

#define	PSL_601_MASK	~(PSL_POW|PSL_ILE|PSL_BE|PSL_RI|PSL_LE)

/*
 * Floating-point exception modes:
 */
#define	PSL_FE_DIS	0		/* none */
#define	PSL_FE_NONREC	PSL_FE1		/* imprecise non-recoverable */
#define	PSL_FE_REC	PSL_FE0		/* imprecise recoverable */
#define	PSL_FE_PREC	(PSL_FE0 | PSL_FE1) /* precise */
#define	PSL_FE_DFLT	PSL_FE_DIS	/* default == none */

/*
 * Note that PSL_POW and PSL_ILE are not in the saved copy of the MSR
 */
#define	PSL_MBO		0
#define	PSL_MBZ		0

#define	PSL_USERSET	(PSL_EE | PSL_PR | PSL_ME | PSL_IR | PSL_DR | PSL_RI)

#define	PSL_USERSTATIC	(PSL_USERSET | PSL_IP | 0x87c0008c)

#endif	/* _MACHINE_PSL_H_ */
