/*-
 * Copyright (c) 1995 - 2001 John Hay.
 * Copyright (c) 1996 SDL Communications, Inc.
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
#ifndef _IF_SRREGS_H_
#define _IF_SRREGS_H_

#define NCHAN			2    /* A HD64570 chip have 2 channels */

#define SR_BUF_SIZ		512
#define SR_TX_BLOCKS		2    /* Sepperate sets of tx buffers */

#define SR_CRD_N2		1
#define SR_CRD_N2PCI		2

/*
 * RISCom/N2 ISA card.
 */
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

/*
 * RISCom/N2 PCI card.
 */
#define SR_FECR			0x0200 /* Front End Control Register */
#define SR_FECR_ETC0		0x0001 /* Enable Ext Clock out */
#define SR_FECR_ETC1		0x0002 /* Enable Ext Clock out */
#define SR_FECR_TE0		0x0004 /* Enable RS422 TXD */
#define SR_FECR_TE1		0x0008 /* Enable RS422 TXD */
#define SR_FECR_GPO0		0x0010 /* General Purpose Output */
#define SR_FECR_GPO1		0x0020 /* General Purpose Output */
#define SR_FECR_DTR0		0x0040 /* 0 for active, 1 for inactive */
#define SR_FECR_DTR1		0x0080 /* 0 for active, 1 for inactive */
#define SR_FECR_DSR0		0x0100 /* DSR0 Status */
#define SR_FECR_ID0		0x0E00 /* ID of channel 0 */
#define SR_FECR_DSR1		0x1000 /* DSR1 Status */
#define SR_FECR_ID1		0xE000 /* ID of channel 1 */

#define SR_FE_ID_V35		0x00   /* V.35 Interface */
#define SR_FE_ID_RS232		0x01   /* RS232 Interface */
#define SR_FE_ID_TEST		0x02   /* Test Board */
#define SR_FE_ID_RS422		0x03   /* RS422 Interface */
#define SR_FE_ID_HSSI		0x05   /* HSSI Interface */
#define SR_FE_ID_X21		0x06   /* X.21 Interface */
#define SR_FE_ID_NONE		0x07   /* No card present */
#define SR_FE_ID0_SHFT		   9
#define SR_FE_ID1_SHFT		  13

/*
 * These macros are used to hide the difference between the way the
 * ISA N2 cards and the PCI N2 cards access the Hitachi 64570 SCA.
 */
#define SRC_GET8(hc,off)	(*hc->src_get8)(hc,(uintptr_t)&off)
#define SRC_GET16(hc,off)	(*hc->src_get16)(hc,(uintptr_t)&off)
#define SRC_PUT8(hc,off,d)	(*hc->src_put8)(hc,(uintptr_t)&off,d)
#define SRC_PUT16(hc,off,d)	(*hc->src_put16)(hc,(uintptr_t)&off,d)

/*
 * These macros enable/disable the DPRAM and select the correct
 * DPRAM page.
 */
#define SRC_GET_WIN(addr)	((addr >> SRC_WIN_SHFT) & SR_PG_MSK)

#define SRC_SET_ON(hc)		sr_outb(hc, SR_PCR,			     \
					SR_PCR_MEM_WIN | sr_inb(hc, SR_PCR))
#define SRC_SET_MEM(hc,win)	sr_outb(hc, SR_PSR, SRC_GET_WIN(win) |	     \
					(sr_inb(hc, SR_PSR) & ~SR_PG_MSK))
#define SRC_SET_OFF(hc)		sr_outb(hc, SR_PCR,			     \
					~SR_PCR_MEM_WIN & sr_inb(hc, SR_PCR))

/*
 * Define the hardware (card information) structure needed to keep
 * track of the device itself... There is only one per card.
 */
struct sr_hardc {
	struct	sr_softc *sc;		/* software channels */
	int	cunit;			/* card w/in system */

	u_short	iobase;			/* I/O Base Address */
	int	cardtype;
	int	numports;		/* # of ports on cd */
	int	mempages;
	u_int	memsize;		/* DPRAM size: bytes */
	u_int	winmsk;
	vm_offset_t	mem_pstart;	/* start of buffer */
	caddr_t	mem_start;		/* start of DP RAM */
	caddr_t	mem_end;		/* end of DP RAM */

	sca_regs	*sca;		/* register array */

	bus_space_tag_t bt_ioport;
	bus_space_tag_t bt_memory;
	bus_space_handle_t bh_ioport;
	bus_space_handle_t bh_memory;
	int rid_ioport;
	int rid_memory;
	int rid_plx_memory;
	int rid_irq;
	struct resource* res_ioport;	/* resource for port range */
	struct resource* res_memory;	/* resource for mem range */
	struct resource* res_plx_memory;
	struct resource* res_irq;	/* resource for irq range */
	void	*intr_cookie;

	/*
	 * We vectorize the following functions to allow re-use between the
	 * ISA card's needs and those of the PCI card.
	 */
	void    (*src_put8)(struct sr_hardc *hc, u_int off, u_int val);
	void    (*src_put16)(struct sr_hardc *hc, u_int off, u_int val);
	u_int	(*src_get8)(struct sr_hardc *hc, u_int off);
	u_int	(*src_get16)(struct sr_hardc *hc, u_int off);
};

extern devclass_t sr_devclass;

int sr_allocate_ioport(device_t device, int rid, u_long size);
int sr_allocate_irq(device_t device, int rid, u_long size);
int sr_allocate_memory(device_t device, int rid, u_long size);
int sr_allocate_plx_memory(device_t device, int rid, u_long size);
int sr_deallocate_resources(device_t device);
int sr_attach(device_t device);
int sr_detach(device_t device);

#define sr_inb(hc, port) \
	bus_space_read_1((hc)->bt_ioport, (hc)->bh_ioport, (port))

#define sr_outb(hc, port, value) \
	bus_space_write_1((hc)->bt_ioport, (hc)->bh_ioport, (port), (value))

#define sr_read_fecr(hc) \
	bus_space_read_4((hc)->bt_memory, (hc)->bh_memory, SR_FECR)

#define sr_write_fecr(hc, value) \
	bus_space_write_4((hc)->bt_memory, (hc)->bh_memory, SR_FECR, (value))

#endif /* _IF_SRREGS_H_ */
