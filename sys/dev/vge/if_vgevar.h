/*-
 * Copyright (c) 2004
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/vge/if_vgevar.h,v 1.4 2005/06/10 16:49:16 brooks Exp $
 */

#if !defined(__i386__)
#define VGE_FIXUP_RX
#endif

#define VGE_JUMBO_MTU	9000

#define VGE_IFQ_MAXLEN 64

#define VGE_TX_DESC_CNT		256
#define VGE_RX_DESC_CNT		256	/* Must be a multiple of 4!! */
#define VGE_RING_ALIGN		256
#define VGE_RX_LIST_SZ		(VGE_RX_DESC_CNT * sizeof(struct vge_rx_desc))
#define VGE_TX_LIST_SZ		(VGE_TX_DESC_CNT * sizeof(struct vge_tx_desc))
#define VGE_TX_DESC_INC(x)	(x = (x + 1) % VGE_TX_DESC_CNT)
#define VGE_RX_DESC_INC(x)	(x = (x + 1) % VGE_RX_DESC_CNT)
#define VGE_ADDR_LO(y)		((u_int64_t) (y) & 0xFFFFFFFF)
#define VGE_ADDR_HI(y)		((u_int64_t) (y) >> 32)
#define VGE_BUFLEN(y)		((y) & 0x7FFF)
#define VGE_OWN(x)		(le32toh((x)->vge_sts) & VGE_RDSTS_OWN)
#define VGE_RXBYTES(x)		((le32toh((x)->vge_sts) & \
				 VGE_RDSTS_BUFSIZ) >> 16)
#define VGE_MIN_FRAMELEN	60

#ifdef VGE_FIXUP_RX
#define VGE_ETHER_ALIGN		sizeof(uint32_t)
#else
#define VGE_ETHER_ALIGN		0
#endif

struct vge_type {
	uint16_t		vge_vid;
	uint16_t		vge_did;
	char			*vge_name;
};

struct vge_softc;

struct vge_dmaload_arg {
	struct vge_softc	*sc;
	int			vge_idx;
	int			vge_maxsegs;
	struct mbuf		*vge_m0;
	u_int32_t		vge_flags;
};

struct vge_list_data {
	struct mbuf		*vge_tx_mbuf[VGE_TX_DESC_CNT];
	struct mbuf		*vge_rx_mbuf[VGE_RX_DESC_CNT];
	int			vge_tx_prodidx;
	int			vge_rx_prodidx;
	int			vge_tx_considx;
	int			vge_tx_free;
	bus_dmamap_t		vge_tx_dmamap[VGE_TX_DESC_CNT];
	bus_dmamap_t		vge_rx_dmamap[VGE_RX_DESC_CNT];
	bus_dma_tag_t		vge_mtag;        /* mbuf mapping tag */
	bus_dma_tag_t		vge_rx_list_tag;
	bus_dmamap_t		vge_rx_list_map;
	struct vge_rx_desc	*vge_rx_list;
	bus_addr_t		vge_rx_list_addr;
	bus_dma_tag_t		vge_tx_list_tag;
	bus_dmamap_t		vge_tx_list_map;
	struct vge_tx_desc	*vge_tx_list;
	bus_addr_t		vge_tx_list_addr;
};

struct vge_softc {
	struct ifnet		*vge_ifp;	/* interface info */
	device_t		vge_dev;
	bus_space_handle_t	vge_bhandle;	/* bus space handle */
	bus_space_tag_t		vge_btag;	/* bus space tag */
	struct resource		*vge_res;
	struct resource		*vge_irq;
	void			*vge_intrhand;
	device_t		vge_miibus;
	bus_dma_tag_t		vge_parent_tag;
	bus_dma_tag_t		vge_tag;
	u_int8_t		vge_unit;	/* interface number */
	u_int8_t		vge_type;
	int			vge_if_flags;
	int			vge_rx_consumed;
	int			vge_link;
	int			vge_camidx;
	struct task		vge_txtask;
	struct mtx		vge_mtx;
	struct mbuf		*vge_head;
	struct mbuf		*vge_tail;

	struct vge_list_data	vge_ldata;

	int			suspended;	/* 0 = normal  1 = suspended */
#ifdef DEVICE_POLLING
	int			rxcycles;
#endif
};

#define	VGE_LOCK(_sc)		mtx_lock(&(_sc)->vge_mtx)
#define	VGE_UNLOCK(_sc)		mtx_unlock(&(_sc)->vge_mtx)
#define	VGE_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->vge_mtx, MA_OWNED)

/*
 * register space access macros
 */
#define CSR_WRITE_STREAM_4(sc, reg, val)	\
	bus_space_write_stream_4(sc->vge_btag, sc->vge_bhandle, reg, val)
#define CSR_WRITE_4(sc, reg, val)	\
	bus_space_write_4(sc->vge_btag, sc->vge_bhandle, reg, val)
#define CSR_WRITE_2(sc, reg, val)	\
	bus_space_write_2(sc->vge_btag, sc->vge_bhandle, reg, val)
#define CSR_WRITE_1(sc, reg, val)	\
	bus_space_write_1(sc->vge_btag, sc->vge_bhandle, reg, val)

#define CSR_READ_4(sc, reg)		\
	bus_space_read_4(sc->vge_btag, sc->vge_bhandle, reg)
#define CSR_READ_2(sc, reg)		\
	bus_space_read_2(sc->vge_btag, sc->vge_bhandle, reg)
#define CSR_READ_1(sc, reg)		\
	bus_space_read_1(sc->vge_btag, sc->vge_bhandle, reg)

#define CSR_SETBIT_1(sc, reg, x)	\
	CSR_WRITE_1(sc, reg, CSR_READ_1(sc, reg) | (x))
#define CSR_SETBIT_2(sc, reg, x)	\
	CSR_WRITE_2(sc, reg, CSR_READ_2(sc, reg) | (x))
#define CSR_SETBIT_4(sc, reg, x)	\
	CSR_WRITE_4(sc, reg, CSR_READ_4(sc, reg) | (x))

#define CSR_CLRBIT_1(sc, reg, x)	\
	CSR_WRITE_1(sc, reg, CSR_READ_1(sc, reg) & ~(x))
#define CSR_CLRBIT_2(sc, reg, x)	\
	CSR_WRITE_2(sc, reg, CSR_READ_2(sc, reg) & ~(x))
#define CSR_CLRBIT_4(sc, reg, x)	\
	CSR_WRITE_4(sc, reg, CSR_READ_4(sc, reg) & ~(x))

#define VGE_TIMEOUT		10000

