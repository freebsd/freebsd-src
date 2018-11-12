/*-
 * Copyright (c) 2009 Yohanes Nugroho <yohanes@gmail.com>
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef	_IFECEVAR_H
#define	_IFECEVAR_H

#define	ECE_MAX_TX_BUFFERS	128
#define	ECE_MAX_RX_BUFFERS	128
#define	MAX_FRAGMENT		32

typedef struct {
	/* 1st 32Bits */
	uint32_t		data_ptr;
	/* 2nd	32Bits*/
	uint32_t		length:16;

	uint32_t		tco:1; /*tcp checksum offload*/
	uint32_t		uco:1; /*udp checksum offload*/
	uint32_t		ico:1; /*ip checksum offload*/
	/* force_route_port_map*/
	uint32_t		pmap:3;
	/* force_route */
	uint32_t		fr:1;
	/* force_priority_value */
	uint32_t		pri:3;
	/* force_priority */
	uint32_t		fp:1;
	/*interrupt_bit*/
	uint32_t		interrupt:1;
	/*last_seg*/
	uint32_t		ls:1;
	/*first_seg*/
	uint32_t		fs:1;
	/* end_bit */
	uint32_t		eor:1;
	/* c_bit */
	uint32_t		cown:1;
	/* 3rd 32Bits*/
	/*vid_index*/
	uint32_t		vid:3;
	/*insert_vid_tag*/
	uint32_t		insv:1;
	/*pppoe_section_index*/
	uint32_t		sid:3;
	/*insert_pppoe_section*/
	uint32_t		inss:1;
	uint32_t		unused:24;
	/* 4th 32Bits*/
	uint32_t		unused2;

} eth_tx_desc_t;

typedef struct{
	uint32_t		data_ptr;
	uint32_t		length:16;
	uint32_t		l4f:1;
	uint32_t		ipf:1;
	uint32_t		prot:2;
	uint32_t		hr:6;
	uint32_t		sp:2;
	uint32_t		ls:1;
	uint32_t		fs:1;
	uint32_t		eor:1;
	uint32_t		cown:1;
	uint32_t		unused;
	uint32_t		unused2;
} eth_rx_desc_t;


struct rx_desc_info {
	struct mbuf*buff;
	bus_dmamap_t dmamap;
	eth_rx_desc_t *desc;
};

struct tx_desc_info {
	struct mbuf*buff;
	bus_dmamap_t dmamap;
	eth_tx_desc_t *desc;
};


struct ece_softc
{
	struct ifnet *ifp;		/* ifnet pointer */
	struct mtx sc_mtx;		/* global mutex */
	struct mtx sc_mtx_tx;		/* tx mutex */
	struct mtx sc_mtx_rx;		/* rx mutex */
	struct mtx sc_mtx_cleanup;	/* rx mutex */

	bus_dma_tag_t	sc_parent_tag;	/* parent bus DMA tag */

	device_t dev;			/* Myself */
	device_t miibus;		/* My child miibus */
	void *intrhand;			/* Interrupt handle */
	void *intrhand_qf;		/* queue full */
	void *intrhand_tx;		/* tx complete */
	void *intrhand_status;		/* error status */

	struct resource *irq_res_tx;	/* transmit */
	struct resource *irq_res_rec;	/* receive */
	struct resource *irq_res_qf;	/* queue full */
	struct resource *irq_res_status; /* status */

	struct resource	*mem_res;	/* Memory resource */

	struct callout tick_ch;		/* Tick callout */

	struct taskqueue *sc_tq;
	struct task	sc_intr_task;
	struct task	sc_cleanup_task;
	struct task	sc_tx_task;

	bus_dmamap_t	dmamap_ring_tx;
	bus_dmamap_t	dmamap_ring_rx;
	bus_dmamap_t	rx_sparemap;

	/*dma tag for ring*/
	bus_dma_tag_t	dmatag_ring_tx;
	bus_dma_tag_t	dmatag_ring_rx;

	/*dma tag for data*/
	bus_dma_tag_t	dmatag_data_tx;
	bus_dma_tag_t	dmatag_data_rx;

	/*the ring*/
	eth_tx_desc_t*	desc_tx;
	eth_rx_desc_t*	desc_rx;

	/*ring physical address*/
	bus_addr_t	ring_paddr_tx;
	bus_addr_t	ring_paddr_rx;

	/*index of last received descriptor*/
	uint32_t last_rx;
	struct rx_desc_info rx_desc[ECE_MAX_RX_BUFFERS];

	/* tx producer index */
	uint32_t tx_prod;
	/* tx consumer index */
	uint32_t tx_cons;
	/* tx ring index*/
	uint32_t desc_curr_tx;

	struct tx_desc_info tx_desc[ECE_MAX_TX_BUFFERS];
};


struct arl_table_entry_t {
	uint32_t cmd_complete: 1;
	uint32_t table_end: 1;
	uint32_t search_match: 1;
	uint32_t filter:1; /*if set, packet will be dropped */
	uint32_t vlan_mac:1; /*indicates that this is the gateway mac address*/
	uint32_t vlan_gid:3; /*vlan id*/
	uint32_t age_field:3;
	uint32_t port_map:3;
	 /*48 bit mac address*/
	uint8_t mac_addr[6];
	uint8_t pad[2];
};

struct mac_list{
	char mac_addr[6];
	struct mac_list *next;
};

#endif
