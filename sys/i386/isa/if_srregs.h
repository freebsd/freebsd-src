/*
 * Copyright (c) 1995 John Hay.  All rights reserved.
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
 *	This product includes software developed by [your name]
 *	and [any other names deserving credit ]
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY [your name] AND CONTRIBUTORS ``AS IS'' AND
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
 * $Id$
 */
#ifndef _IF_SRREGS_H_
#define _IF_SRREGS_H_

#define NCHAN			2    /* A HD64570 chip have 2 channels */

#define SR_BUF_SIZ		512
#define SR_TX_BLOCKS		2    /* Sepperate sets of tx buffers */
#define SRC_IO_SIZ		0x10 /* Actually a lie. It uses a lot more. */
#define SRC_WIN_SIZ		0x00004000
#define SRC_WIN_MSK		(SRC_WIN_SIZ - 1)
#define SRC_WIN_SHFT		14

#define SR_FLAGS_NCHAN_MSK	0x0000000F
#define SR_FLAGS_0_CLK_MSK	0x00000030
#define SR_FLAGS_0_EXT_CLK	0x00000000 /* External RX clock shared by TX */
#define SR_FLAGS_0_EXT_SEP_CLK	0x00000010 /* Sepperate external clocks */
#define SR_FLAGS_0_INT_CLK	0x00000020 /* Internal clock */
#define SR_FLAGS_1_CLK_MSK	0x000000C0
#define SR_FLAGS_1_EXT_CLK	0x00000000 /* External RX clock shared by TX */
#define SR_FLAGS_1_EXT_SEP_CLK	0x00000040 /* Sepperate external clocks */
#define SR_FLAGS_1_INT_CLK	0x00000080 /* Internal clock */

#define SR_FLAGS_CLK_SHFT	4
#define SR_FLAGS_CLK_CHAN_SHFT  2
#define SR_FLAGS_EXT_CLK	0x00000000 /* External RX clock shared by TX */
#define SR_FLAGS_EXT_SEP_CLK	0x00000001 /* Sepperate external clocks */
#define SR_FLAGS_INT_CLK	0x00000002 /* Internal clock */

#define SR_PCR			0x00 /* RW, PC Control Register */
#define SR_BAR			0x02 /* RW, Base Address Register */
#define SR_PSR			0x04 /* RW, Page Scan Register */
#define SR_MCR			0x06 /* RW, Modem Control Register */

#define SR_PCR_SCARUN		0x01 /* !Reset */
#define SR_PCR_EN_VPM		0x02 /* Running above 1M */
#define SR_PCR_MEM_WIN		0x04 /* Open memory window */
#define SR_PCR_ISA16		0x08 /* 16 bit ISA mode */
#define SR_PCR_16M_SEL		0xF0 /* A20-A23 Addresses */

#define SR_PSR_PG_SEL		0x1F /* Page 0 - 31 select */
#define SR_PG_MSK		0x1F
#define SR_PSR_WIN_SIZ		0x60 /* Window size select */
#define SR_PSR_WIN_16K		0x00
#define SR_PSR_WIN_32K		0x20
#define SR_PSR_WIN_64K		0x40
#define SR_PSR_WIN_128K		0x60
#define SR_PSR_EN_SCA_DMA	0x80 /* Enable the SCA DMA */

#define SR_MCR_DTR0		0x01 /* Deactivate DTR0 */
#define SR_MCR_DTR1		0x02 /* Deactivate DTR1 */
#define SR_MCR_DSR0		0x04 /* DSR0 Status */
#define SR_MCR_DSR1		0x08 /* DSR1 Status */
#define SR_MCR_TE0		0x10 /* Enable RS422 TXD */
#define SR_MCR_TE1		0x20 /* Enable RS422 TXD */
#define SR_MCR_ETC0		0x40 /* Enable Ext Clock out */
#define SR_MCR_ETC1		0x80 /* Enable Ext Clock out */

#endif /* _IF_SRREGS_H_ */
