/*      $NetBSD: sa11x0_dmacreg.h,v 1.1 2001/07/08 23:37:53 rjs Exp $	*/

/*-
 * Copyright (c) 2001, The NetBSD Foundation, Inc.  All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by IWAMOTO Toshihiro.
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/arm/sa11x0/sa11x0_dmacreg.h,v 1.1.18.1 2008/11/25 02:59:29 kensmith Exp $
 *
 */

/* SA11[01]0 integrated DMA controller */

#define SADMAC_NPORTS		40

#define SADMAC_DAR0		0x00	/* DMA device address register */
#define SADMAC_DCR0_SET		0x04	/* DMA control/status (set) */
#define SADMAC_DCR0_CLR		0x08	/* DMA control/status (clear) */
#define SADMAC_DCR0		0x0C	/* DMA control/status (read only) */
#define SADMAC_DBSA0		0x10	/* DMA Buffer A start address */
#define SADMAC_DBTA0		0x14	/* DMA Buffer A transfer count */
#define SADMAC_DBSB0		0x18	/* DMA Buffer B start address */
#define SADMAC_DBTB0		0x1C	/* DMA Buffer B transfer count */

#define SADMAC_DAR1		0x20
#define SADMAC_DCR1_SET		0x24
#define SADMAC_DCR1_CLR		0x28
#define SADMAC_DCR1		0x2C
#define SADMAC_DBSA1		0x30
#define SADMAC_DBTA1		0x34
#define SADMAC_DBSB1		0x38
#define SADMAC_DBTB1		0x3C

#define SADMAC_DAR2		0x40
#define SADMAC_DCR2_SET		0x44
#define SADMAC_DCR2_CLR		0x48
#define SADMAC_DCR2		0x4C
#define SADMAC_DBSA2		0x50
#define SADMAC_DBTA2		0x54
#define SADMAC_DBSB2		0x58
#define SADMAC_DBTB2		0x5C

#define SADMAC_DAR3		0x60
#define SADMAC_DCR3_SET		0x64
#define SADMAC_DCR3_CLR		0x68
#define SADMAC_DCR3		0x6C
#define SADMAC_DBSA3		0x70
#define SADMAC_DBTA3		0x74
#define SADMAC_DBSB3		0x78
#define SADMAC_DBTB3		0x7C

#define SADMAC_DAR4		0x80
#define SADMAC_DCR4_SET		0x84
#define SADMAC_DCR4_CLR		0x88
#define SADMAC_DCR4		0x8C
#define SADMAC_DBSA4		0x90
#define SADMAC_DBTA4		0x94
#define SADMAC_DBSB4		0x98
#define SADMAC_DBTB4		0x9C

#define SADMAC_DAR5		0xA0
#define SADMAC_DCR5_SET		0xA4
#define SADMAC_DCR5_CLR		0xA8
#define SADMAC_DCR5		0xAC
#define SADMAC_DBSA5		0xB0
#define SADMAC_DBTA5		0xB4
#define SADMAC_DBSB5		0xB8
#define SADMAC_DBTB5		0xBC
