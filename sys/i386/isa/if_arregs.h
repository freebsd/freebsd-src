/*
 * Copyright (c) 1995, 1999 John Hay.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
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
 * $FreeBSD: src/sys/i386/isa/if_arregs.h,v 1.6 1999/10/17 09:38:25 jhay Exp $
 */
#ifndef _IF_ARREGS_H_
#define _IF_ARREGS_H_

#define NCHAN			2    /* A HD64570 chip have 2 channels */
#define NPORT			4    /* A ArNet board can have 4 ports or */
				     /* channels */

#define AR_BUF_SIZ		512
#define AR_TX_BLOCKS		2
#define ARC_IO_SIZ		0x10
#define ARC_WIN_SIZ		0x00004000
#define ARC_WIN_MSK		(ARC_WIN_SIZ - 1)
#define ARC_WIN_SHFT		14

/* Some PCI specific offsets. */
#define AR_PCI_SCA_1_OFFSET	0x00040000
#define AR_PCI_SCA_2_OFFSET	0x00040400
#define AR_PCI_ORBASE_OFFSET	0x00041000
#define AR_PCI_SCA_PCR		0x0208
#define AR_PCI_SCA_DMER		0x0309
/* PCI Legacy (below 1M) offsets. */
#define AR_PCI_L_SCA_1_OFFSET	0x00004000
#define AR_PCI_L_SCA_2_OFFSET	0x00004400
#define AR_PCI_L_ORBASE_OFFSET	0x00005000

#define AR_ID_5			0x00 /* RO, Card probe '5' */
#define AR_ID_7			0x01 /* RO, Card probe '7' */
#define AR_ID_0			0x02 /* RO, Card probe '0' */
#define AR_BMI			0x03 /* RO, Bus, mem and interface type */
#define AR_REV			0x04 /* RO, Adapter revision */
#define AR_PNUM			0x05 /* RO, Port number */
#define AR_HNDSH		0x06 /* RO, Supported handshake */
#define AR_ISTAT		0x07 /* RO, DCD and Interrupt status */
#define AR_MSCA_EN		0x08 /* WO, Memory and SCA enable */
#define AR_TXC_DTR0		0x09 /* WO, Tx Clock and DTR control 0 + 1 */
#define AR_SEC_PAL		0x0A /* RW, Security PAL */
#define AR_INT_ACK0		0x0B /* RO, Interrupt Acknowledge 0 + 1 */
#define AR_INT_SEL		0x0C /* RW, Interrupt Select */
#define AR_MEM_SEL		0x0D /* RW, Memory Select */
#define AR_INT_ACK2		0x0E /* RO, Interrupt Acknowledge 2 + 3 */
#define AR_TXC_DTR2		0x0E /* WO, Tx Clock and DTR control 2 + 3 */
/* PCI only */
#define AR_PIMCTRL		0x4C /* RW, PIM and LEDs */
#define AR_INT_SCB		0x50 /* RO, Interrupt Scoreboard */

#define AR_REV_MSK		0x0F
#define AR_WSIZ_MSK		0xE0
#define AR_WSIZ_SHFT		5
/* Bus memory and interface type */
#define AR_BUS_MSK		0x03
#define AR_BUS_ISA		0x00
#define AR_BUS_MCA		0x01
#define AR_BUS_EISA		0x02
#define AR_BUS_PCI		0x03

#define AR_MEM_MSK		0x1C
#define AR_MEM_SHFT		0x02
#define AR_MEM_64K		0x00
#define AR_MEM_128K		0x04
#define AR_MEM_256K		0x08
#define AR_MEM_512K		0x0C

/*
 * EIA-232
 * V.35/EIA-232
 * EIA-530
 * X.21
 * EIA-530/X.21 Combo
 */
#define AR_IFACE_MSK		0xE0
#define AR_IFACE_SHFT		0x05
#define AR_IFACE_EIA_232	0x00  /* Only on the 570 card, not 570i */
#define AR_IFACE_V_35		0x20  /* Selectable between V.35 and EIA-232 */
#define AR_IFACE_EIA_530	0x40
#define AR_IFACE_X_21		0x60
#define AR_IFACE_COMBO		0xC0  /* X.21 / EIA-530 */
#define AR_IFACE_PIM		0xE0  /* PIM module */
#define AR_IFACE_LOOPBACK	0xFE
#define AR_IFACE_UNKNOWN	0xFF

/* Supported Handshake signals */
#define AR_SHSK_DTR		0x01
#define AR_SHSK_RTS		0x02
#define AR_SHSK_CTS		0x10
#define AR_SHSK_DSR		0x20
#define AR_SHSK_RI		0x40
#define AR_SHSK_DCD		0x80

/* DCD and Interrupt status */
#define AR_BD_INT		0x01
#define AR_INT_0		0x20
#define AR_INT_1		0x40

#define AR_DCD_MSK		0x1E
#define AR_DCD_SHFT		0x01
#define AR_DCD_0		0x02
#define AR_DCD_1		0x04
#define AR_DCD_2		0x08
#define AR_DCD_3		0x10

/* Memory and SCA enable */
#define AR_WIN_MSK		0x1F

#define AR_SEL_SCA_0		0x00
#define AR_SEL_SCA_1		0x20
#define AR_ENA_SCA		0x40
#define AR_ENA_MEM		0x80

/* Transmit Clock and DTR and RESET */
#define AR_TXC_DTR_TX0		0x01
#define AR_TXC_DTR_TX1		0x02
#define AR_TXC_DTR_DTR0		0x04
#define AR_TXC_DTR_DTR1		0x08
#define AR_TXC_DTR_TXCS0	0x10
#define AR_TXC_DTR_TXCS1	0x20
#define AR_TXC_DTR_NOTRESET	0x40
#define AR_TXC_DTR_RESET	0x00

/* Interrupt select register */
#define AR_INTS_CEN		0x01
#define AR_INTS_ISEL0		0x02
#define AR_INTS_ISEL1		0x04
#define AR_INTS_ISEL2		0x08
#define AR_INTS_CMA14		0x10
#define AR_INTS_CMA15		0x20

/* Advanced PIM Control */
#define AR_PIM_STROBE		0x01
#define AR_PIM_DATA		0x02
#define AR_PIM_MODEG		0x04
#define AR_PIM_A2D_STROBE	0x04
#define AR_PIM_MODEY		0x08
#define AR_PIM_A2D_DOUT		0x08
#define AR_PIM_AUTO_LED		0x10
#define AR_PIM_INT		0x20

#define AR_PIM_RESET		0x00 /* MODEG and MODEY 0 */
#define AR_PIM_READ		AR_PIM_MODEG
#define AR_PIM_WRITE		AR_PIM_MODEY

#endif /* _IF_ARREGS_H_ */
