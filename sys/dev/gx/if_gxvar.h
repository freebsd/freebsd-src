/*-
 * Copyright (c) 1999,2000,2001 Jonathan Lemon
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
 *	$FreeBSD$
 */

#if __FreeBSD_version < 500000
#define GX_LOCK(gx)		
#define GX_UNLOCK(gx)		
#define mtx_init(a, b, c, d)
#define mtx_destroy(a)
struct mtx { int filler; };
#else
#define GX_LOCK(gx)		mtx_lock(&(gx)->gx_mtx)
#define GX_UNLOCK(gx)		mtx_unlock(&(gx)->gx_mtx)
#endif

#ifdef __alpha__
#undef vtophys
#define vtophys(va)		alpha_XXX_dmamap((vm_offset_t)va)
#endif

#ifndef PCIM_CMD_MWIEN
#define PCIM_CMD_MWIEN		0x0010
#endif

#define ETHER_ALIGN	2

/* CSR_WRITE_8 assumes the register is in low/high order */
#define CSR_WRITE_8(gx, reg, val) do { \
	bus_space_write_4(gx->gx_btag, gx->gx_bhandle, reg, val & 0xffffffff); \
	bus_space_write_4(gx->gx_btag, gx->gx_bhandle, reg + 4, val >> 32); \
} while (0)
#define CSR_WRITE_4(gx, reg, val) \
	bus_space_write_4(gx->gx_btag, gx->gx_bhandle, reg, val)
#define CSR_WRITE_2(gx, reg, val) \
	bus_space_write_2(gx->gx_btag, gx->gx_bhandle, reg, val)
#define CSR_WRITE_1(gx, reg, val) \
	bus_space_write_1(gx->gx_btag, gx->gx_bhandle, reg, val)

#define CSR_READ_4(gx, reg) \
	bus_space_read_4(gx->gx_btag, gx->gx_bhandle, reg)
#define CSR_READ_2(gx, reg) \
	bus_space_read_2(gx->gx_btag, gx->gx_bhandle, reg)
#define CSR_READ_1(gx, reg) \
	bus_space_read_1(gx->gx_btag, gx->gx_bhandle, reg)

#define GX_SETBIT(gx, reg, x) \
	CSR_WRITE_4(gx, reg, (CSR_READ_4(gx, reg) | (x)))
#define GX_CLRBIT(gx, reg, x) \
	CSR_WRITE_4(gx, reg, (CSR_READ_4(gx, reg) & ~(x)))

/*
 * In theory, these can go up to 64K each, but due to chip bugs, 
 * they are limited to 256 max.  Descriptor counts should be a 
 * multiple of 8.
 */
#define GX_TX_RING_CNT		256
#define GX_RX_RING_CNT		256

#define GX_INC(x, y)		(x) = (x + 1) % y
#define GX_PREV(x, y)		(x == 0 ? y - 1 : x - 1)

#define GX_MAX_MTU		(16 * 1024)

struct gx_ring_data {
	struct 	gx_rx_desc gx_rx_ring[GX_RX_RING_CNT];
	struct 	gx_tx_desc gx_tx_ring[GX_TX_RING_CNT];
};

struct gx_chain_data {
	struct	mbuf *gx_rx_chain[GX_RX_RING_CNT];
	struct	mbuf *gx_tx_chain[GX_TX_RING_CNT];
};

struct gx_regs {
	int	r_rx_base;
	int	r_rx_length;
	int	r_rx_head;
	int	r_rx_tail;
	int	r_rx_delay;
	int	r_rx_dma_ctrl;

	int	r_tx_base;
	int	r_tx_length;
	int	r_tx_head;
	int	r_tx_tail;
	int	r_tx_delay;
	int	r_tx_dma_ctrl;
};

struct gx_softc {
	struct arpcom		arpcom;		/* interface info */
	struct ifmedia		gx_media;	/* media info */
	bus_space_handle_t	gx_bhandle;	/* bus space handle */
	bus_space_tag_t		gx_btag;	/* bus space tag */
	void			*gx_intrhand;	/* irq handler handle */
	struct resource		*gx_irq;	/* IRQ resource handle */
	struct resource		*gx_res;	/* I/O or shared mem handle */
	device_t		gx_dev;
	device_t		gx_miibus;
	u_int8_t		gx_unit;	/* controller number */
	u_int8_t		gx_tbimode;	/* transceiver flag */
	int			gx_vflags;	/* version-specific flags */
	u_int32_t		gx_ipg;		/* version-specific IPG */
	struct gx_ring_data	*gx_rdata;
	struct gx_chain_data	gx_cdata;
	int			gx_if_flags;
	struct mbuf 		*gx_pkthdr;
	struct mbuf 		**gx_pktnextp;
	int			gx_rx_tail_idx;	/* receive ring tail index */
	int			gx_tx_tail_idx;	/* transmit ring tail index */
	int			gx_tx_head_idx;	/* transmit ring tail index */
	int			gx_txcnt;
	int			gx_txcontext;	/* current TX context */
	struct gx_regs		gx_reg;
	struct mtx		gx_mtx;

/* tunables */
	int			gx_tx_intr_delay;
	int			gx_rx_intr_delay;
	
/* statistics */
	int			gx_tx_interrupts;
	int			gx_rx_interrupts;
	int			gx_interrupts;
};

/*
 * flags to compensate for differing chip variants
 */
#define GXF_FORCE_TBI		0x0001	/* force TBI mode on */
#define GXF_DMA			0x0002	/* has DMA control registers */
#define GXF_ENABLE_MWI		0x0004	/* supports MWI burst mode */
#define GXF_OLD_REGS		0x0008	/* use old register mapping */
#define GXF_CSUM		0x0010	/* hardware checksum offload */

/*
 * TX Context definitions.
 */
#define GX_TXCONTEXT_NONE	0
#define GX_TXCONTEXT_TCPIP	1
#define GX_TXCONTEXT_UDPIP	2
