/*
 * Copyright (c) 1993 Herb Peyerl (hpeyerl@novatel.ca) All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2. The name
 * of the author may not be used to endorse or promote products derived from
 * this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

struct ep_board {
	u_short prod_id;	/* product ID */
	int cmd_off;		/* command offset (bit shift) */
	int mii_trans;		/* activate MII transiever */
	u_short res_cfg;	/* resource configuration */
};

/*
 * Ethernet software status per interface.
 */
struct ep_softc {
	struct arpcom arpcom;	/* Ethernet common part	 */
	struct ifmedia ifmedia;	/* media info		 */

	device_t dev;

	struct resource *iobase;
	struct resource *irq;

	bus_space_handle_t ep_bhandle;
	bus_space_tag_t ep_btag;
	void *ep_intrhand;

	int ep_io_addr;		/* i/o bus address	 */

	u_short ep_connectors;	/* Connectors on this card. */
	u_char ep_connector;	/* Configured connector. */

	struct mbuf *top;
	struct mbuf *mcur;
	short cur_len;

	int stat;		/* some flags */
#define	F_RX_FIRST		0x001
#define	F_PROMISC		0x008
#define	F_ACCESS_32_BITS	0x100

	int gone;		/* adapter is not present (for PCCARD) */

	struct ep_board epb;

	int unit;

#ifdef  EP_LOCAL_STATS
	short tx_underrun;
	short rx_no_first;
	short rx_no_mbuf;
	short rx_overrunf;
	short rx_overrunl;
#endif
};

int ep_alloc(device_t);
void ep_free(device_t);
int ep_detach(device_t);
void ep_get_media(struct ep_softc *);
int ep_attach(struct ep_softc *);
void ep_intr(void *);
int get_e(struct ep_softc *, u_int16_t, u_int16_t *);
int ep_get_macaddr(struct ep_softc *, u_char *);
