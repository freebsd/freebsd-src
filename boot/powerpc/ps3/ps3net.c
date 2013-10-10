/*-
 * Copyright (C) 2010 Nathan Whitehorn
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
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/socket.h>
  
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>

#define _KERNEL
#include <machine/cpufunc.h>

#include <stand.h>
#include <net.h>
#include <netif.h>
#include "bootstrap.h"
#include "lv1call.h"
#include "ps3.h"

#define GELIC_DESCR_OWNED	0xa0000000
#define GELIC_CMDSTAT_NOIPSEC	0x00080000
#define GELIC_CMDSTAT_LAST	0x00040000
#define GELIC_RXERRORS		0x7def8000

#define GELIC_POLL_PERIOD	100 /* microseconds */

static int	ps3net_probe(struct netif *, void *);
static int	ps3net_match(struct netif *, void *);
static void	ps3net_init(struct iodesc *, void *);
static int	ps3net_get(struct iodesc *, void *, size_t, time_t);
static int	ps3net_put(struct iodesc *, void *, size_t);
static void	ps3net_end(struct netif *);

struct netif_stats ps3net_stats[1];
struct netif_dif ps3net_ifs[] = {{0, 1, ps3net_stats, 0}};

/* XXX: Get from firmware, not hardcoding */
static int busid = 1;
static int devid = 0;
static int vlan;
static uint64_t dma_base;

struct gelic_dmadesc {
	uint32_t paddr;
	uint32_t len;
	uint32_t next;
	uint32_t cmd_stat;
	uint32_t result_size;
	uint32_t valid_size;
	uint32_t data_stat;
	uint32_t rxerror;
};

struct netif_driver ps3net = {
	"net",
	ps3net_match,
	ps3net_probe,
	ps3net_init,
	ps3net_get,
	ps3net_put,
	ps3net_end,
	ps3net_ifs, 1
};

static int
ps3net_match(struct netif *nif, void *machdep_hint)
{
	return (1);
}

static int
ps3net_probe(struct netif *nif, void *machdep_hint)
{
	return (0);
}

static int
ps3net_put(struct iodesc *desc, void *pkt, size_t len)
{
	volatile static struct gelic_dmadesc txdesc __aligned(32);
	volatile static char txbuf[1536] __aligned(128);
	size_t sendlen;
	int err;

#if defined(NETIF_DEBUG)
	struct ether_header *eh;

	printf("net_put: desc %p, pkt %p, len %d\n", desc, pkt, len);
	eh = pkt;
	printf("dst: %s ", ether_sprintf(eh->ether_dhost));
	printf("src: %s ", ether_sprintf(eh->ether_shost));
	printf("type: 0x%x\n", eh->ether_type & 0xffff);
#endif

	while (txdesc.cmd_stat & GELIC_DESCR_OWNED) {
		printf("Stalled XMIT!\n");
		delay(10);
	}

	/*
	 * We must add 4 extra bytes to this packet to store the destination
	 * VLAN. 
	 */
	memcpy(txbuf, pkt, 12);
	sendlen = 12;
	
	if (vlan >= 0) {
		sendlen += 4;
		((uint8_t *)txbuf)[12] = 0x81;
		((uint8_t *)txbuf)[13] = 0x00;
		((uint8_t *)txbuf)[14] = vlan >> 8;
		((uint8_t *)txbuf)[15] = vlan & 0xff;
	}
	memcpy((void *)txbuf + sendlen, pkt + 12, len - 12);
	sendlen += len - 12;

	bzero(&txdesc, sizeof(txdesc));
	txdesc.paddr = dma_base + (uint32_t)txbuf;
	txdesc.len = sendlen;
	txdesc.cmd_stat = GELIC_CMDSTAT_NOIPSEC | GELIC_CMDSTAT_LAST |
	    GELIC_DESCR_OWNED;

	powerpc_sync();

	do {
		err = lv1_net_start_tx_dma(busid, devid,
		    dma_base + (uint32_t)&txdesc, 0);
		delay(1);
		if (err != 0)
			printf("TX Error: %d\n",err);
	} while (err != 0);

	return (len);
}

static int
ps3net_get(struct iodesc *desc, void *pkt, size_t len, time_t timeout)
{
	volatile static struct gelic_dmadesc rxdesc __aligned(32);
	volatile static char rxbuf[1536] __aligned(128);
	int err = 0;

	if (len == 0)
		goto restartdma;

	timeout *= 1000000; /* convert to microseconds */
	while (rxdesc.cmd_stat & GELIC_DESCR_OWNED) {
		if (timeout < GELIC_POLL_PERIOD) 
			return (ETIMEDOUT);
		delay(GELIC_POLL_PERIOD);
		timeout -= GELIC_POLL_PERIOD;
	}

	delay(200);
	if (rxdesc.rxerror & GELIC_RXERRORS) {
		err = -1;
		goto restartdma;
	}

	/*
	 * Copy the packet to the receive buffer, leaving out the
	 * 2 byte VLAN header.
	 */
	len = min(len, rxdesc.valid_size - 2);
	memcpy(pkt, (u_char *)rxbuf + 2, len);
	err = len;

#if defined(NETIF_DEBUG)
{
	struct ether_header *eh;

	printf("net_get: desc %p, pkt %p, len %d\n", desc, pkt, len);
	eh = pkt;
	printf("dst: %s ", ether_sprintf(eh->ether_dhost));
	printf("src: %s ", ether_sprintf(eh->ether_shost));
	printf("type: 0x%x\n", eh->ether_type & 0xffff);
}
#endif

restartdma:
	lv1_net_stop_rx_dma(busid, devid, 0);
	powerpc_sync();

	bzero(&rxdesc, sizeof(rxdesc));
	rxdesc.paddr = dma_base + (uint32_t)rxbuf;
	rxdesc.len = sizeof(rxbuf);
	rxdesc.next = 0;
	rxdesc.cmd_stat = GELIC_DESCR_OWNED;
	powerpc_sync();

	lv1_net_start_rx_dma(busid, devid, dma_base + (uint32_t)&rxdesc, 0);

	return (err);
}

static void
ps3net_init(struct iodesc *desc, void *machdep_hint)
{
	uint64_t mac, val;
	int i,err;

	err = lv1_open_device(busid, devid, 0);

	lv1_net_stop_tx_dma(busid, devid, 0);
	lv1_net_stop_rx_dma(busid, devid, 0);

	/*
	 * Wait for link to come up
	 */

	for (i = 0; i < 1000; i++) {
		lv1_net_control(busid, devid, GELIC_GET_LINK_STATUS, 2, 0,
		    0, &val);
		if (val & GELIC_LINK_UP)
			break;
		delay(500);
	}

	/*
	 * Set up DMA IOMMU entries
	 */

	err = lv1_setup_dma(busid, devid, &dma_base);

	/*
	 * Get MAC address and VLAN IDs
	 */

	lv1_net_control(busid, devid, GELIC_GET_MAC_ADDRESS, 0, 0, 0, &mac);
	bcopy(&((uint8_t *)&mac)[2], desc->myea, sizeof(desc->myea));

	vlan = -1;
	err = lv1_net_control(busid, devid, GELIC_GET_VLAN_ID, 2, 0,
	    0, &val);
	if (err == 0)
		vlan = val;

	/*
	 * Start RX DMA engine
	 */

	ps3net_get(NULL, NULL, 0, 0);
}

static void
ps3net_end(struct netif *nif)
{
	lv1_close_device(busid, devid);
}

