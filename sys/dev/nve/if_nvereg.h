/*
 * Copyright (c) 2005 by David E. O'Brien <obrien@FreeBSD.org>.
 * Copyright (c) 2003 by Quinton Dolan <q@onthenet.com.au>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS `AS IS'' AND
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
 * $Id: if_nvreg.h,v 1.6 2004/08/12 14:00:05 q Exp $
 * $FreeBSD$
 */
 
#ifndef _IF_NVEREG_H_
#define _IF_NVEREG_H_

#ifndef NVIDIA_VENDORID
#define	NVIDIA_VENDORID 0x10DE
#endif

#define	NFORCE_MCPNET1_DEVICEID 0x01C3
#define	NFORCE_MCPNET2_DEVICEID 0x0066
#define	NFORCE_MCPNET3_DEVICEID 0x00D6
#define	NFORCE_MCPNET4_DEVICEID 0x0086
#define	NFORCE_MCPNET5_DEVICEID 0x008C
#define	NFORCE_MCPNET6_DEVICEID 0x00E6
#define	NFORCE_MCPNET7_DEVICEID 0x00DF
#define	NFORCE_MCPNET8_DEVICEID 0x0056
#define	NFORCE_MCPNET9_DEVICEID 0x0057
#define	NFORCE_MCPNET10_DEVICEID 0x0037
#define	NFORCE_MCPNET11_DEVICEID 0x0038 

#define	NV_RID		0x10

#define	TX_RING_SIZE	64
#define	RX_RING_SIZE	64
#define	NV_MAX_FRAGS	63

#define	FCS_LEN 4

#define	NVE_DEBUG		0x0000
#define	NVE_DEBUG_INIT		0x0001
#define	NVE_DEBUG_RUNNING	0x0002
#define	NVE_DEBUG_DEINIT 	0x0004
#define	NVE_DEBUG_IOCTL		0x0008
#define	NVE_DEBUG_INTERRUPT	0x0010
#define	NVE_DEBUG_API		0x0020
#define	NVE_DEBUG_LOCK		0x0040
#define	NVE_DEBUG_BROKEN	0x0080
#define	NVE_DEBUG_MII		0x0100
#define	NVE_DEBUG_ALL		0xFFFF

#if NVE_DEBUG
#define	DEBUGOUT(level, fmt, args...) if (NVE_DEBUG & level) \
    printf(fmt, ## args)
#else
#define	DEBUGOUT(level, fmt, args...)
#endif

typedef unsigned long	ulong;

struct nve_map_buffer {
	struct mbuf *mbuf;	/* mbuf receiving packet */
	bus_dmamap_t map;	/* DMA map */	
};

struct nve_dma_info {
	bus_dma_tag_t tag;
	struct nve_map_buffer buf;
	u_int16_t buflength;
	caddr_t vaddr;		/* Virtual memory address */
	bus_addr_t paddr;	/* DMA physical address */
};

struct nve_rx_desc {
	struct nve_rx_desc *next;
	struct nve_map_buffer buf;
	u_int16_t buflength;
	caddr_t vaddr;
	bus_addr_t paddr;
};

struct nve_tx_desc {
	/* Don't add anything above this structure */
	TX_INFO_ADAP TxInfoAdap;
	struct nve_tx_desc *next;
	struct nve_map_buffer buf;
	u_int16_t buflength;
	u_int32_t numfrags;
	bus_dma_segment_t frags[NV_MAX_FRAGS + 1];
};

struct nve_softc {
	struct ifnet *ifp;	/* interface info */
	struct resource *res;
	struct resource *irq;

	ADAPTER_API *hwapi;
	OS_API osapi;
		
	device_t miibus;
	device_t dev;
	struct callout stat_callout;

	void *sc_ih;
	bus_space_tag_t sc_st;
	bus_space_handle_t sc_sh;
	bus_dma_tag_t mtag;
	bus_dma_tag_t rtag;
	bus_dmamap_t rmap;
	bus_dma_tag_t ttag;
	bus_dmamap_t tmap;

	struct nve_rx_desc *rx_desc;
	struct nve_tx_desc *tx_desc;
	bus_addr_t rx_addr;
	bus_addr_t tx_addr;
	u_int16_t rx_ring_full;
	u_int16_t tx_ring_full;
	u_int32_t cur_rx;
	u_int32_t cur_tx;
	u_int32_t pending_rxs;
	u_int32_t pending_txs;

	struct mtx mtx;

	/* Stuff for dealing with the NVIDIA OS API */
	struct callout ostimer;
	PTIMER_FUNC ostimer_func;
	void *ostimer_params;
	int linkup;
	ulong tx_errors;
	NV_UINT32 hwmode;
	NV_UINT32 max_frame_size;
	NV_UINT32 phyaddr;
	NV_UINT32 media;
	CMNDATA_OS_ADAPTER adapterdata;
	unsigned char original_mac_addr[6];
};

struct nve_type {
	u_int16_t	vid_id;
	u_int16_t	dev_id;
	char		*name;
};

#define NVE_LOCK(_sc)		mtx_lock(&(_sc)->mtx)
#define NVE_UNLOCK(_sc)		mtx_unlock(&(_sc)->mtx)
#define NVE_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->mtx, MA_OWNED)

#define IF_Kbps(x) ((x) * 1000)			/* kilobits/sec. */
#define IF_Mbps(x) (IF_Kbps((x) * 1000))	/* megabits/sec. */
#define ETHER_ALIGN 2

extern int ADAPTER_ReadPhy (PVOID pContext, ULONG ulPhyAddr, ULONG ulReg, ULONG *pulVal);
extern int ADAPTER_WritePhy (PVOID pContext, ULONG ulPhyAddr, ULONG ulReg, ULONG ulVal);
extern int ADAPTER_Init (PVOID pContext, USHORT usForcedSpeed, UCHAR ucForceDpx, UCHAR ucForceMode, UINT *puiLinkState);

#endif	/* _IF_NVEREG_H_ */
