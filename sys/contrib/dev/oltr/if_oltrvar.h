/*
 * Copyright (c) 1998, Larry Lile
 * All rights reserved.
 *
 * For latest sources and information on this driver, please
 * go to http://anarchy.stdio.com.
 *
 * Questions, comments or suggestions should be directed to
 * Larry Lile <lile@stdio.com>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/*#define DEBUG_MASK DEBUG_POLL*/

#ifndef DEBUG_MASK
#define DEBUG_MASK 0x0000
#endif

#define DEBUG_POLL	0x0001
#define DEBUG_INT	0x0002
#define DEBUG_INIT	0x0004
#define DEBUG_FN_ENT	0x8000

#define PCI_VENDOR_OLICOM 0x108D

#define OLTR_PORT_COUNT	0x20

#define MIN3(A,B,C) (MIN(A, (MIN(B, C))))

struct oltr_rx_buf {
	int			index;
	char			*data;
	u_long			address;
};

struct oltr_tx_buf {
	int			index;
	char 			*data;
	u_long			address;
};

#define RING_BUFFER_LEN		16
#define RING_BUFFER(x)		((RING_BUFFER_LEN - 1) & x)
#define RX_BUFFER_LEN		2048
#define TX_BUFFER_LEN		2048

struct oltr_softc {
	struct arpcom		arpcom;
	struct ifmedia		ifmedia;
	bus_space_handle_t	oltr_bhandle;
	bus_space_tag_t		oltr_btag;
	void			*oltr_intrhand;
	int			irq_rid;
	struct resource		*irq_res;
	int			port_rid;
	struct resource		*port_res;
	int			drq_rid;
	struct resource		*drq_res;
	bus_dma_tag_t		bus_tag;
	bus_dma_tag_t		mem_tag;
	bus_dmamap_t		mem_map;
	bus_addr_t		queue_phys;
	char *			queue_addr;
	int			unit;
	int 			state;
#define OL_UNKNOWN	0
#define OL_INIT		1
#define OL_READY	2
#define OL_CLOSING	3
#define OL_CLOSED	4
#define OL_OPENING	5
#define OL_OPEN		6
#define OL_PROMISC	7
#define OL_DEAD		8
	struct oltr_rx_buf	rx_ring[RING_BUFFER_LEN];
	int			tx_head, tx_avail, tx_frame;
	struct oltr_tx_buf	tx_ring[RING_BUFFER_LEN];
	TRlldTransmit_t		frame_ring[RING_BUFFER_LEN];
	struct mbuf		*restart;
	TRlldAdapter_t		TRlldAdapter;
	unsigned long		TRlldAdapter_phys;
	TRlldStatistics_t	statistics;
	TRlldStatistics_t	current;
        TRlldAdapterConfig_t    config;
	u_short			AdapterMode;
	u_long			GroupAddress;
	u_long			FunctionalAddress;
	struct callout_handle	oltr_poll_ch;
	/*struct callout_handle	oltr_stat_ch;*/
	void			*work_memory;
};

#define SELF_TEST_POLLS	32

void oltr_poll 			__P((void *));
/*void oltr_stat 		__P((void *));*/

int oltr_attach			__P((device_t dev));
void oltr_stop			__P((struct oltr_softc *));
