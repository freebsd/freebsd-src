/*-
 * Copyright (c) 2016 Emmanuel Vadot <manu@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef __AW_P2WI_H__
#define	__AW_P2WI_H__

#define	P2WI_CTRL			0x00
#define	 P2WI_CTRL_SOFT_RESET		(1 << 0)
#define	 P2WI_CTRL_GLOBAL_INT_ENB	(1 << 1)
#define	 P2WI_CTRL_ABORT_TRANS		(1 << 6)
#define	 P2WI_CTRL_START_TRANS		(1 << 7)

#define	P2WI_CCR			0x04
#define	 P2WI_CCR_CLK_DIV_SHIFT		0
#define	 P2WI_CCR_CLK_DIV_MASK		0xFF
#define	 P2WI_CCR_SDA_ODLY_SHIFT	7
#define	 P2WI_CCR_SDA_ODLY_MASK		0x700

#define	P2WI_INTE	0x08
#define	 P2WI_INTE_TRANS_OVER_ENB
#define	 P2WI_INTE_TRANS_ERR_ENB
#define	 P2WI_INTE_LOAD_BSY_ENB

#define	P2WI_STAT			0x0C
#define	 P2WI_STAT_TRANS_OVER		(1 << 0)
#define	 P2WI_STAT_TRANS_ERR		(1 << 1)
#define	 P2WI_STAT_LOAD_BSY		(1 << 2)
#define	 P2WI_STAT_TRANS_ERR_ID_SHIFT	7
#define	 P2WI_STAT_TRANS_ERR_ID_MASK	0xFF00

#define	P2WI_DADDR0	0x10

#define	P2WI_DADDR1	0x14

#define	P2WI_DLEN		0x18
#define	 P2WI_DLEN_LEN(x)	((x - 1) & 0x7)
#define	 P2WI_DLEN_READ		(1 << 4)

#define	P2WI_DATA0	0x1C

#define	P2WI_DATA1	0x20

#define	P2WI_LCR		0x24
#define	 P2WI_LCR_SDA_CTL_EN	(1 << 0)
#define	 P2WI_LCR_SDA_CTL	(1 << 1)
#define	 P2WI_LCR_SCL_CTL_EN	(1 << 2)
#define	 P2WI_LCR_SCL_CTL	(1 << 3)
#define	 P2WI_LCR_SDA_STATE	(1 << 4)
#define	 P2WI_LCR_SCL_STATE	(1 << 5)


#define	P2WI_PMCR	0x28

#endif /* __AW_P2WI_H__ */
