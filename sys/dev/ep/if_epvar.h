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

/*
 * Ethernet software status per interface.
 */
struct ep_softc {
	struct arpcom	arpcom;		/* Ethernet common part		 */
	int		ep_io_addr;	/* i/o bus address		 */
	struct mbuf *	top;
	struct mbuf *	mcur;
	short		cur_len;
	u_short		ep_connectors;	/* Connectors on this card.	 */
	u_char		ep_connector;	/* Configured connector.	 */
	int		stat;		/* some flags */
	int		gone;		/* adapter is not present (for PCCARD) */
	struct resource *irq;		/* IRQ resource */
	void		*ih;		/* Interrupt handle cookie */
#define	F_RX_FIRST		0x1
#define	F_PROMISC		0x8

#define	F_ACCESS_32_BITS	0x100

	struct ep_board *epb;

	int		unit;

#ifdef  EP_LOCAL_STATS
	short		tx_underrun;
	short		rx_no_first;
	short		rx_no_mbuf;
	short		rx_bpf_disc;
	short		rx_overrunf;
	short		rx_overrunl;
#endif
};

struct ep_board {
	int		epb_addr;	/* address of this board */
	char		epb_used;	/* was this entry already used for configuring ? */
					/* data from EEPROM for later use */
	u_short		eth_addr[3];	/* Ethernet address */
	u_short		prod_id;	/* product ID */
	int		cmd_off;	/* command offset (bit shift) */
	int		mii_trans;	/* activate MII transiever */
	u_short		res_cfg;	/* resource configuration */
};

extern struct ep_softc*	ep_softc[];
extern struct ep_board	ep_board[];
extern int		ep_boards;
extern u_long		ep_unit;

extern struct ep_softc*	ep_alloc	(int, struct ep_board *);
extern int		ep_attach	(struct ep_softc *);
extern void		ep_free		(struct ep_softc *);
extern void		ep_intr		(void *);

extern u_int16_t	get_e		(struct ep_softc *, int);
