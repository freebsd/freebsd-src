/*-
 * Copyright (c) 1994-2000
 *	Paul Richards.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name Paul Richards may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL RICHARDS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PAUL RICHARDS BE LIABLE
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

/*
#define DIAGNOSTIC
#define DEBUG
 *
 * TODO ----
 *
 * Check all the XXX comments -- some of them are just things I've left
 * unfinished rather than "difficult" problems that were hacked around.
 *
 * Check log settings.
 *
 * Check how all the arpcom flags get set and used.
 *
 * Re-inline and re-static all routines after debugging.
 *
 * Remember to assign iobase in SHMEM probe routines.
 *
 * Replace all occurences of LANCE-controller-card etc in prints by the name
 * strings of the appropriate type -- nifty window dressing
 *
 * Add DEPCA support -- mostly done.
 *
 */

#include "opt_inet.h"

/* Some defines that should really be in generic locations */
#define FCS_LEN 4
#define MULTICAST_FILTER_LEN 8

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/syslog.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net/bpf.h>

#include <machine/md_var.h>

#include <dev/lnc/if_lncvar.h>
#include <dev/lnc/if_lncreg.h>

devclass_t lnc_devclass;

static char const * const nic_ident[] = {
	"Unknown",
	"BICC",
	"NE2100",
	"DEPCA",
	"CNET98S",	/* PC-98 */
};

static char const * const ic_ident[] = {
	"Unknown",
	"LANCE",
	"C-LANCE",
	"PCnet-ISA",
	"PCnet-ISA+",
	"PCnet-ISA II",
	"PCnet-32 VL-Bus",
	"PCnet-PCI",
	"PCnet-PCI II",
	"PCnet-FAST",
	"PCnet-FAST+",
	"PCnet-Home",
};

static void lnc_setladrf(struct lnc_softc *sc);
static void lnc_reset(struct lnc_softc *sc);
static void lnc_free_mbufs(struct lnc_softc *sc);
static __inline int alloc_mbuf_cluster(struct lnc_softc *sc,
					    struct host_ring_entry *desc);
static __inline struct mbuf *chain_mbufs(struct lnc_softc *sc,
					      int start_of_packet,
					      int pkt_len);
static __inline struct mbuf *mbuf_packet(struct lnc_softc *sc,
					      int start_of_packet,
					      int pkt_len);
static __inline void lnc_rint(struct lnc_softc *sc);
static __inline void lnc_tint(struct lnc_softc *sc);

static void lnc_init(void *);
static __inline int mbuf_to_buffer(struct mbuf *m, char *buffer);
static __inline struct mbuf *chain_to_cluster(struct mbuf *m);
static void lnc_start(struct ifnet *ifp);
static int lnc_ioctl(struct ifnet *ifp, u_long command, caddr_t data);
static void lnc_watchdog(struct ifnet *ifp);
#ifdef DEBUG
void lnc_dump_state(struct lnc_softc *sc);
void mbuf_dump_chain(struct mbuf *m);
#endif

u_short
read_csr(struct lnc_softc *sc, u_short port)
{
	lnc_outw(sc->rap, port);
	return (lnc_inw(sc->rdp));
}

void
write_csr(struct lnc_softc *sc, u_short port, u_short val)
{
	lnc_outw(sc->rap, port);
	lnc_outw(sc->rdp, val);
}

static __inline void
write_bcr(struct lnc_softc *sc, u_short port, u_short val)
{
	lnc_outw(sc->rap, port);
	lnc_outw(sc->bdp, val);
}

static __inline u_short
read_bcr(struct lnc_softc *sc, u_short port)
{
	lnc_outw(sc->rap, port);
	return (lnc_inw(sc->bdp));
}

int
lance_probe(struct lnc_softc *sc)
{
	write_csr(sc, CSR0, STOP);

	if ((lnc_inw(sc->rdp) & STOP) && ! (read_csr(sc, CSR3))) {
		/*
		 * Check to see if it's a C-LANCE. For the LANCE the INEA bit
		 * cannot be set while the STOP bit is. This restriction is
		 * removed for the C-LANCE.
		 */
		write_csr(sc, CSR0, INEA);
		if (read_csr(sc, CSR0) & INEA)
			return (C_LANCE);
		else
			return (LANCE);
	} else
		return (UNKNOWN);
}

static __inline u_long
ether_crc(const u_char *ether_addr)
{
#define POLYNOMIAL           0xEDB88320UL
    u_char i, j, addr;
    u_int crc = 0xFFFFFFFFUL;

    for (i = 0; i < ETHER_ADDR_LEN; i++) {
	addr = *ether_addr++;
	for (j = 0; j < MULTICAST_FILTER_LEN; j++) {
	    crc = (crc >> 1) ^ (((crc ^ addr) & 1) ? POLYNOMIAL : 0);   
	    addr >>= 1;
	}
    }
    return crc;
#undef POLYNOMIAL
}

void
lnc_release_resources(device_t dev)
{
	lnc_softc_t *sc = device_get_softc(dev);

	if (sc->irqres) {
		bus_teardown_intr(dev, sc->irqres, sc->intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, sc->irqrid, sc->irqres);
	}

	if (sc->portres)
		bus_release_resource(dev, SYS_RES_IOPORT,
		                     sc->portrid, sc->portres);
	if (sc->drqres)
		bus_release_resource(dev, SYS_RES_DRQ, sc->drqrid, sc->drqres);

	if (sc->dmat) {
		if (sc->dmamap) {
			bus_dmamap_unload(sc->dmat, sc->dmamap);
			bus_dmamem_free(sc->dmat, sc->recv_ring, sc->dmamap);
		}
		bus_dma_tag_destroy(sc->dmat);
	}
}

/*
 * Set up the logical address filter for multicast packets
 */
static __inline void
lnc_setladrf(struct lnc_softc *sc)
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct ifmultiaddr *ifma;
	u_long index;
	int i;

	if (sc->flags & IFF_ALLMULTI) {
	    for (i=0; i < MULTICAST_FILTER_LEN; i++)
		sc->init_block->ladrf[i] = 0xFF;
	    return;
	}

	/*
	 * For each multicast address, calculate a crc for that address and
	 * then use the high order 6 bits of the crc as a hash code where
	 * bits 3-5 select the byte of the address filter and bits 0-2 select
	 * the bit within that byte.
	 */

	bzero(sc->init_block->ladrf, MULTICAST_FILTER_LEN);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;

		index = ether_crc(LLADDR((struct sockaddr_dl *)ifma->ifma_addr))
				>> 26;
		sc->init_block->ladrf[index >> 3] |= 1 << (index & 7);
	}
}

void
lnc_stop(struct lnc_softc *sc)
{
	write_csr(sc, CSR0, STOP);
}

static void
lnc_reset(struct lnc_softc *sc)
{
	lnc_init(sc);
}

static void
lnc_free_mbufs(struct lnc_softc *sc)
{
	int i;

	/*
	 * We rely on other routines to keep the buff.mbuf field valid. If
	 * it's not NULL then we assume it points to an allocated mbuf.
	 */

	for (i = 0; i < NDESC(sc->nrdre); i++)
		if ((sc->recv_ring + i)->buff.mbuf)
			m_free((sc->recv_ring + i)->buff.mbuf);

	for (i = 0; i < NDESC(sc->ntdre); i++)
		if ((sc->trans_ring + i)->buff.mbuf)
			m_free((sc->trans_ring + i)->buff.mbuf);

	if (sc->mbuf_count)
		m_freem(sc->mbufs);
}

static __inline int
alloc_mbuf_cluster(struct lnc_softc *sc, struct host_ring_entry *desc)
{
	register struct mds *md = desc->md;
	struct mbuf *m=0;
	int addr;

	/* Try and get cluster off local cache */
	if (sc->mbuf_count) {
		sc->mbuf_count--;
		m = sc->mbufs;
		sc->mbufs = m->m_next;
		/* XXX m->m_data = m->m_ext.ext_buf;*/
	} else {
		MGET(m, M_DONTWAIT, MT_DATA);
   	if (!m)
			return(1);
        MCLGET(m, M_DONTWAIT);
   	if (!m->m_ext.ext_buf) {
			m_free(m);
			return(1);
		}
	}

	desc->buff.mbuf = m;
	addr = kvtop(m->m_data);
	md->md0 = addr;
	md->md1= ((addr >> 16) & 0xff) | OWN;
	md->md2 = -(short)(MCLBYTES - sizeof(struct pkthdr));
	md->md3 = 0;
	return(0);
}

static __inline struct mbuf *
chain_mbufs(struct lnc_softc *sc, int start_of_packet, int pkt_len)
{
	struct mbuf *head, *m;
	struct host_ring_entry *desc;

	/*
	 * Turn head into a pkthdr mbuf --
	 * assumes a pkthdr type mbuf was
	 * allocated to the descriptor
	 * originally.
	 */

	desc = sc->recv_ring + start_of_packet;

	head = desc->buff.mbuf;
	head->m_flags |= M_PKTHDR;
	bzero(&head->m_pkthdr, sizeof(head->m_pkthdr));

	m = head;
	do {
		m = desc->buff.mbuf;
		m->m_len = min((MCLBYTES - sizeof(struct pkthdr)), pkt_len);
		pkt_len -= m->m_len;
		if (alloc_mbuf_cluster(sc, desc))
			return((struct mbuf *)NULL);
		INC_MD_PTR(start_of_packet, sc->nrdre)
		desc = sc->recv_ring + start_of_packet;
		m->m_next = desc->buff.mbuf;
	} while (start_of_packet != sc->recv_next);

	m->m_next = 0;
	return(head);
}

static __inline struct mbuf *
mbuf_packet(struct lnc_softc *sc, int start_of_packet, int pkt_len)
{

	struct host_ring_entry *start;
	struct mbuf *head,*m,*m_prev;
	char *data,*mbuf_data;
	short blen;
	int amount;

	/* Get a pkthdr mbuf for the start of packet */
	MGETHDR(head, M_DONTWAIT, MT_DATA);
	if (!head) {
		LNCSTATS(drop_packet)
		return(0);
	}

	m = head;
	m->m_len = 0;
	start = sc->recv_ring + start_of_packet;
	/*blen = -(start->md->md2);*/
	blen = RECVBUFSIZE; /* XXX More PCnet-32 crap */
	data = start->buff.data;
	mbuf_data = m->m_data;

	while (start_of_packet != sc->recv_next) {
		/*
		 * If the data left fits in a single buffer then set
		 * blen to the size of the data left.
		 */
		if (pkt_len < blen)
			blen = pkt_len;

		/*
		 * amount is least of data in current ring buffer and
		 * amount of space left in current mbuf.
		 */
		amount = min(blen, M_TRAILINGSPACE(m));
		if (amount == 0) {
			/* mbuf must be empty */
			m_prev = m;
			MGET(m, M_DONTWAIT, MT_DATA);
			if (!m) {
				m_freem(head);
				return(0);
			}
			if (pkt_len >= MINCLSIZE)
				MCLGET(m, M_DONTWAIT);
			m->m_len = 0;
			m_prev->m_next = m;
			amount = min(blen, M_TRAILINGSPACE(m));
			mbuf_data = m->m_data;
		}
		bcopy(data, mbuf_data, amount);
		blen -= amount;
		pkt_len -= amount;
		m->m_len += amount;
		data += amount;
		mbuf_data += amount;

		if (blen == 0) {
			start->md->md1 &= HADR;
			start->md->md1 |= OWN;
			start->md->md2 = -RECVBUFSIZE; /* XXX - shouldn't be necessary */
			INC_MD_PTR(start_of_packet, sc->nrdre)
			start = sc->recv_ring + start_of_packet;
			data = start->buff.data;
			/*blen = -(start->md->md2);*/
			blen = RECVBUFSIZE; /* XXX More PCnet-32 crap */
		}
	}
	return(head);
}


static __inline void
lnc_rint(struct lnc_softc *sc)
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct host_ring_entry *next, *start;
	int start_of_packet;
	struct mbuf *head;
	struct ether_header *eh;
	int lookahead;
	int flags;
	int pkt_len;

	/*
	 * The LANCE will issue a RINT interrupt when the ownership of the
	 * last buffer of a receive packet has been relinquished by the LANCE.
	 * Therefore, it can be assumed that a complete packet can be found
	 * before hitting buffers that are still owned by the LANCE, if not
	 * then there is a bug in the driver that is causing the descriptors
	 * to get out of sync.
	 */

#ifdef DIAGNOSTIC
	if ((sc->recv_ring + sc->recv_next)->md->md1 & OWN) {
		int unit = ifp->if_unit;
		log(LOG_ERR, "lnc%d: Receive interrupt with buffer still owned by controller -- Resetting\n", unit);
		lnc_reset(sc);
		return;
	}
	if (!((sc->recv_ring + sc->recv_next)->md->md1 & STP)) {
		int unit = ifp->if_unit;
		log(LOG_ERR, "lnc%d: Receive interrupt but not start of packet -- Resetting\n", unit);
		lnc_reset(sc);
		return;
	}
#endif

	lookahead = 0;
	next = sc->recv_ring + sc->recv_next;
	while ((flags = next->md->md1) & STP) {

		/* Make a note of the start of the packet */
		start_of_packet = sc->recv_next;

		/*
		 * Find the end of the packet. Even if not data chaining,
		 * jabber packets can overrun into a second descriptor.
	 	 * If there is no error, then the ENP flag is set in the last
		 * descriptor of the packet. If there is an error then the ERR
		 * flag will be set in the descriptor where the error occured.
		 * Therefore, to find the last buffer of a packet we search for
		 * either ERR or ENP.
		 */

		if (!(flags & (ENP | MDERR))) {
			do {
				INC_MD_PTR(sc->recv_next, sc->nrdre)
				next = sc->recv_ring + sc->recv_next;
				flags = next->md->md1;
			} while (!(flags & (STP | OWN | ENP | MDERR)));

			if (flags & STP) {
				int unit = ifp->if_unit;
				log(LOG_ERR, "lnc%d: Start of packet found before end of previous in receive ring -- Resetting\n", unit);
				lnc_reset(sc);
				return;
			}
			if (flags & OWN) {
				if (lookahead) {
					/*
					 * Looked ahead into a packet still
					 * being received
					 */
					sc->recv_next = start_of_packet;
					break;
				} else {
					int unit = ifp->if_unit;
					log(LOG_ERR, "lnc%d: End of received packet not found-- Resetting\n", unit);
					lnc_reset(sc);
					return;
				}
			}
		}

		pkt_len = (next->md->md3 & MCNT) - FCS_LEN;

		/* Move pointer onto start of next packet */
		INC_MD_PTR(sc->recv_next, sc->nrdre)
		next = sc->recv_ring + sc->recv_next;

		if (flags & MDERR) {
			int unit = ifp->if_unit;
			if (flags & RBUFF) {
				LNCSTATS(rbuff)
				log(LOG_ERR, "lnc%d: Receive buffer error\n", unit);
			}
			if (flags & OFLO) {
				/* OFLO only valid if ENP is not set */
				if (!(flags & ENP)) {
					LNCSTATS(oflo)
					log(LOG_ERR, "lnc%d: Receive overflow error \n", unit);
				}
			} else if (flags & ENP) {
			    if ((ifp->if_flags & IFF_PROMISC)==0) {
				/*
				 * FRAM and CRC are valid only if ENP
				 * is set and OFLO is not.
				 */
				if (flags & FRAM) {
					LNCSTATS(fram)
					log(LOG_ERR, "lnc%d: Framing error\n", unit);
					/*
					 * FRAM is only set if there's a CRC
					 * error so avoid multiple messages
					 */
				} else if (flags & CRC) {
					LNCSTATS(crc)
					log(LOG_ERR, "lnc%d: Receive CRC error\n", unit);
				}
			    }
			}

			/* Drop packet */
			LNCSTATS(rerr)
			ifp->if_ierrors++;
			while (start_of_packet != sc->recv_next) {
				start = sc->recv_ring + start_of_packet;
				start->md->md2 = -RECVBUFSIZE; /* XXX - shouldn't be necessary */
				start->md->md1 &= HADR;
				start->md->md1 |= OWN;
				INC_MD_PTR(start_of_packet, sc->nrdre)
			}
		} else { /* Valid packet */

			ifp->if_ipackets++;


			if (sc->nic.mem_mode == DMA_MBUF)
				head = chain_mbufs(sc, start_of_packet, pkt_len);
			else
				head = mbuf_packet(sc, start_of_packet, pkt_len);

			if (head) {
				/*
				 * First mbuf in packet holds the
				 * ethernet and packet headers
				 */
				head->m_pkthdr.rcvif = ifp;
				head->m_pkthdr.len = pkt_len ;
				eh = (struct ether_header *) head->m_data;

				/*
				 * vmware ethernet hardware emulation loops
				 * packets back to itself, violates IFF_SIMPLEX.
				 * drop it if it is from myself.
				*/
				if (bcmp(eh->ether_shost,
				      sc->arpcom.ac_enaddr, ETHER_ADDR_LEN) == 0) {
				    m_freem(head);
				} else {
				    (*ifp->if_input)(ifp, head);
				}
			} else {
				int unit = ifp->if_unit;
				log(LOG_ERR,"lnc%d: Packet dropped, no mbufs\n",unit);
				LNCSTATS(drop_packet)
			}
		}

		lookahead++;
	}

	/*
	 * At this point all completely received packets have been processed
	 * so clear RINT since any packets that have arrived while we were in
	 * here have been dealt with.
	 */

	lnc_outw(sc->rdp, RINT | INEA);
}

static __inline void
lnc_tint(struct lnc_softc *sc)
{
	struct host_ring_entry *next, *start;
	int start_of_packet;
	int lookahead;

	/*
	 * If the driver is reset in this routine then we return immediately to
	 * the interrupt driver routine. Any interrupts that have occured
	 * since the reset will be dealt with there. sc->trans_next
	 * should point to the start of the first packet that was awaiting
	 * transmission after the last transmit interrupt was dealt with. The
	 * LANCE should have relinquished ownership of that descriptor before
	 * the interrupt. Therefore, sc->trans_next should point to a
	 * descriptor with STP set and OWN cleared. If not then the driver's
	 * pointers are out of sync with the LANCE, which signifies a bug in
	 * the driver. Therefore, the following two checks are really
	 * diagnostic, since if the driver is working correctly they should
	 * never happen.
	 */

#ifdef DIAGNOSTIC
	if ((sc->trans_ring + sc->trans_next)->md->md1 & OWN) {
		int unit = sc->arpcom.ac_if.if_unit;
		log(LOG_ERR, "lnc%d: Transmit interrupt with buffer still owned by controller -- Resetting\n", unit);
		lnc_reset(sc);
		return;
	}
#endif


	/*
	 * The LANCE will write the status information for the packet it just
	 * tried to transmit in one of two places. If the packet was
	 * transmitted successfully then the status will be written into the
	 * last descriptor of the packet. If the transmit failed then the
	 * status will be written into the descriptor that was being accessed
	 * when the error occured and all subsequent descriptors in that
	 * packet will have been relinquished by the LANCE.
	 *
	 * At this point we know that sc->trans_next points to the start
	 * of a packet that the LANCE has just finished trying to transmit.
	 * We now search for a buffer with either ENP or ERR set.
	 */

	lookahead = 0;

	do {
		start_of_packet = sc->trans_next;
		next = sc->trans_ring + sc->trans_next;

#ifdef DIAGNOSTIC
		if (!(next->md->md1 & STP)) {
			int unit = sc->arpcom.ac_if.if_unit;
			log(LOG_ERR, "lnc%d: Transmit interrupt but not start of packet -- Resetting\n", unit);
			lnc_reset(sc);
			return;
		}
#endif

		/*
		 * Find end of packet.
		 */

		if (!(next->md->md1 & (ENP | MDERR))) {
			do {
				INC_MD_PTR(sc->trans_next, sc->ntdre)
				next = sc->trans_ring + sc->trans_next;
			} while (!(next->md->md1 & (STP | OWN | ENP | MDERR)));

			if (next->md->md1 & STP) {
				int unit = sc->arpcom.ac_if.if_unit;
				log(LOG_ERR, "lnc%d: Start of packet found before end of previous in transmit ring -- Resetting\n", unit);
				lnc_reset(sc);
				return;
			}
			if (next->md->md1 & OWN) {
				if (lookahead) {
					/*
					 * Looked ahead into a packet still
					 * being transmitted
					 */
					sc->trans_next = start_of_packet;
					break;
				} else {
					int unit = sc->arpcom.ac_if.if_unit;
					log(LOG_ERR, "lnc%d: End of transmitted packet not found -- Resetting\n", unit);
					lnc_reset(sc);
					return;
				}
			}
		}
		/*
		 * Check for ERR first since other flags are irrelevant if an
		 * error occurred.
		 */
		if (next->md->md1 & MDERR) {

			int unit = sc->arpcom.ac_if.if_unit;

			LNCSTATS(terr)
				sc->arpcom.ac_if.if_oerrors++;

			if (next->md->md3 & LCOL) {
				LNCSTATS(lcol)
				log(LOG_ERR, "lnc%d: Transmit late collision  -- Net error?\n", unit);
				sc->arpcom.ac_if.if_collisions++;
				/*
				 * Clear TBUFF since it's not valid when LCOL
				 * set
				 */
				next->md->md3 &= ~TBUFF;
			}
			if (next->md->md3 & LCAR) {
				LNCSTATS(lcar)
				log(LOG_ERR, "lnc%d: Loss of carrier during transmit -- Net error?\n", unit);
			}
			if (next->md->md3 & RTRY) {
				LNCSTATS(rtry)
				log(LOG_ERR, "lnc%d: Transmit of packet failed after 16 attempts -- TDR = %d\n", unit, ((sc->trans_ring + sc->trans_next)->md->md3 & TDR));
				sc->arpcom.ac_if.if_collisions += 16;
				/*
				 * Clear TBUFF since it's not valid when RTRY
				 * set
				 */
				next->md->md3 &= ~TBUFF;
			}
			/*
			 * TBUFF is only valid if neither LCOL nor RTRY are set.
			 * We need to check UFLO after LCOL and RTRY so that we
			 * know whether or not TBUFF is valid. If either are
			 * set then TBUFF will have been cleared above. A
			 * UFLO error will turn off the transmitter so we
			 * have to reset.
			 *
			 */

			if (next->md->md3 & UFLO) {
				LNCSTATS(uflo)
				/*
				 * If an UFLO has occured it's possibly due
				 * to a TBUFF error
				 */
				if (next->md->md3 & TBUFF) {
					LNCSTATS(tbuff)
					log(LOG_ERR, "lnc%d: Transmit buffer error -- Resetting\n", unit);
				} else
					log(LOG_ERR, "lnc%d: Transmit underflow error -- Resetting\n", unit);
				lnc_reset(sc);
				return;
			}
			do {
				INC_MD_PTR(sc->trans_next, sc->ntdre)
				next = sc->trans_ring + sc->trans_next;
			} while (!(next->md->md1 & STP) && (sc->trans_next != sc->next_to_send));

		} else {
			/*
			 * Since we check for ERR first then if we get here
			 * the packet was transmitted correctly. There may
			 * still have been non-fatal errors though.
			 * Don't bother checking for DEF, waste of time.
			 */

			sc->arpcom.ac_if.if_opackets++;

			if (next->md->md1 & MORE) {
				LNCSTATS(more)
				sc->arpcom.ac_if.if_collisions += 2;
			}

			/*
			 * ONE is invalid if LCOL is set. If LCOL was set then
			 * ERR would have also been set and we would have
			 * returned from lnc_tint above. Therefore we can
			 * assume if we arrive here that ONE is valid.
			 *
			 */

			if (next->md->md1 & ONE) {
				LNCSTATS(one)
				sc->arpcom.ac_if.if_collisions++;
			}
			INC_MD_PTR(sc->trans_next, sc->ntdre)
			next = sc->trans_ring + sc->trans_next;
		}

		/*
		 * Clear descriptors and free any mbufs.
		 */

		do {
			start = sc->trans_ring + start_of_packet;
			start->md->md1 &= HADR;
			if (sc->nic.mem_mode == DMA_MBUF) {
				/* Cache clusters on a local queue */
				if ((start->buff.mbuf->m_flags & M_EXT) && (sc->mbuf_count < MBUF_CACHE_LIMIT)) {
					if (sc->mbuf_count) {
						start->buff.mbuf->m_next = sc->mbufs;
						sc->mbufs = start->buff.mbuf;
					} else
						sc->mbufs = start->buff.mbuf;
					sc->mbuf_count++;
					start->buff.mbuf = 0;
				} else {
					/*
					 * XXX should this be m_freem()?
					 */
					m_free(start->buff.mbuf);
					start->buff.mbuf = NULL;
				}
			}
			sc->pending_transmits--;
			INC_MD_PTR(start_of_packet, sc->ntdre)
		}while (start_of_packet != sc->trans_next);

		/*
		 * There's now at least one free descriptor
		 * in the ring so indicate that we can accept
		 * more packets again.
		 */

		sc->arpcom.ac_if.if_flags &= ~IFF_OACTIVE;

		lookahead++;

	} while (sc->pending_transmits && !(next->md->md1 & OWN));

	/*
	 * Clear TINT since we've dealt with all
	 * the completed transmissions.
	 */

	lnc_outw(sc->rdp, TINT | INEA);
}

int
lnc_attach_common(device_t dev)
{
	int unit = device_get_unit(dev);
	lnc_softc_t *sc = device_get_softc(dev);
	int i;
	int skip;

	switch (sc->nic.ident) {
	case BICC:
	case CNET98S:
		skip = 2;
		break;
	default:
		skip = 1;
		break;
	}

	/* Set default mode */
	sc->nic.mode = NORMAL;

	/* Fill in arpcom structure entries */

	sc->arpcom.ac_if.if_softc = sc;
	sc->arpcom.ac_if.if_name = "lnc";
	sc->arpcom.ac_if.if_unit = unit;
	sc->arpcom.ac_if.if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	sc->arpcom.ac_if.if_timer = 0;
	sc->arpcom.ac_if.if_output = ether_output;
	sc->arpcom.ac_if.if_start = lnc_start;
	sc->arpcom.ac_if.if_ioctl = lnc_ioctl;
	sc->arpcom.ac_if.if_watchdog = lnc_watchdog;
	sc->arpcom.ac_if.if_init = lnc_init;
	sc->arpcom.ac_if.if_snd.ifq_maxlen = IFQ_MAXLEN;

	/* Extract MAC address from PROM */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sc->arpcom.ac_enaddr[i] = lnc_inb(i * skip);

	/*
	 * XXX -- should check return status of if_attach
	 */

	ether_ifattach(&sc->arpcom.ac_if, sc->arpcom.ac_enaddr);

	printf("lnc%d: ", unit);
	if (sc->nic.ic == LANCE || sc->nic.ic == C_LANCE)
		printf("%s (%s)",
		       nic_ident[sc->nic.ident], ic_ident[sc->nic.ic]);
	else
		printf("%s", ic_ident[sc->nic.ic]);
	printf(" address %6D\n", sc->arpcom.ac_enaddr, ":");

	return (1);
}

static void
lnc_init(xsc)
	void *xsc;
{
	struct lnc_softc *sc = xsc;
	int s, i;
	char *lnc_mem;

	/* Check that interface has valid address */

	if (TAILQ_EMPTY(&sc->arpcom.ac_if.if_addrhead)) { /* XXX unlikely */
printf("XXX no address?\n");
		return;
	}

	/* Shut down interface */

	s = splimp();
	lnc_stop(sc);
	sc->arpcom.ac_if.if_flags |= IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST; /* XXX??? */

	/*
	 * This sets up the memory area for the controller. Memory is set up for
	 * the initialisation block (12 words of contiguous memory starting
	 * on a word boundary),the transmit and receive ring structures (each
	 * entry is 4 words long and must start on a quadword boundary) and
	 * the data buffers.
	 *
	 * The alignment tests are particularly paranoid.
	 */

	sc->recv_next = 0;
	sc->trans_ring = sc->recv_ring + NDESC(sc->nrdre);
	sc->trans_next = 0;

	if (sc->nic.mem_mode == SHMEM)
		lnc_mem = (char *) sc->nic.iobase;
	else
		lnc_mem = (char *) (sc->trans_ring + NDESC(sc->ntdre));

	lnc_mem = (char *)(((int)lnc_mem + 1) & ~1);
	sc->init_block = (struct init_block *) ((int) lnc_mem & ~1);
	lnc_mem = (char *) (sc->init_block + 1);
	lnc_mem = (char *)(((int)lnc_mem + 7) & ~7);

	/* Initialise pointers to descriptor entries */
	for (i = 0; i < NDESC(sc->nrdre); i++) {
		(sc->recv_ring + i)->md = (struct mds *) lnc_mem;
		lnc_mem += sizeof(struct mds);
	}
	for (i = 0; i < NDESC(sc->ntdre); i++) {
		(sc->trans_ring + i)->md = (struct mds *) lnc_mem;
		lnc_mem += sizeof(struct mds);
	}

	/* Initialise the remaining ring entries */

	if (sc->nic.mem_mode == DMA_MBUF) {

		sc->mbufs = 0;
		sc->mbuf_count = 0;

		/* Free previously allocated mbufs */
		if (sc->flags & LNC_INITIALISED)
			lnc_free_mbufs(sc);


		for (i = 0; i < NDESC(sc->nrdre); i++) {
			if (alloc_mbuf_cluster(sc, sc->recv_ring+i)) {
				log(LOG_ERR, "Initialisation failed -- no mbufs\n");
				splx(s);
				return;
			}
		}

		for (i = 0; i < NDESC(sc->ntdre); i++) {
			(sc->trans_ring + i)->buff.mbuf = 0;
			(sc->trans_ring + i)->md->md0 = 0;
			(sc->trans_ring + i)->md->md1 = 0;
			(sc->trans_ring + i)->md->md2 = 0;
			(sc->trans_ring + i)->md->md3 = 0;
		}
	} else {
		for (i = 0; i < NDESC(sc->nrdre); i++) {
			(sc->recv_ring + i)->md->md0 = kvtop(lnc_mem);
			(sc->recv_ring + i)->md->md1 = ((kvtop(lnc_mem) >> 16) & 0xff) | OWN;
			(sc->recv_ring + i)->md->md2 = -RECVBUFSIZE;
			(sc->recv_ring + i)->md->md3 = 0;
			(sc->recv_ring + i)->buff.data = lnc_mem;
			lnc_mem += RECVBUFSIZE;
		}
		for (i = 0; i < NDESC(sc->ntdre); i++) {
			(sc->trans_ring + i)->md->md0 = kvtop(lnc_mem);
			(sc->trans_ring + i)->md->md1 = ((kvtop(lnc_mem) >> 16) & 0xff);
			(sc->trans_ring + i)->md->md2 = 0;
			(sc->trans_ring + i)->md->md3 = 0;
			(sc->trans_ring + i)->buff.data = lnc_mem;
			lnc_mem += TRANSBUFSIZE;
		}
	}

	sc->next_to_send = 0;

	/* Set up initialisation block */

	sc->init_block->mode = sc->nic.mode;

	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sc->init_block->padr[i] = sc->arpcom.ac_enaddr[i];

	lnc_setladrf(sc);

	sc->init_block->rdra = kvtop(sc->recv_ring->md);
	sc->init_block->rlen = ((kvtop(sc->recv_ring->md) >> 16) & 0xff) | (sc->nrdre << 13);
	sc->init_block->tdra = kvtop(sc->trans_ring->md);
	sc->init_block->tlen = ((kvtop(sc->trans_ring->md) >> 16) & 0xff) | (sc->ntdre << 13);


	/* Set flags to show that the memory area is valid */
	sc->flags |= LNC_INITIALISED;

	sc->pending_transmits = 0;

	/* Give the LANCE the physical address of the initialisation block */

	if (sc->nic.ic == PCnet_Home) {
		u_short	media;
		/* Set PHY_SEL to HomeRun */
		media = read_bcr(sc, BCR49);
		media &= ~3;
		media |= 1;
		write_bcr(sc, BCR49, media);
	}

	write_csr(sc, CSR1, kvtop(sc->init_block));
	write_csr(sc, CSR2, (kvtop(sc->init_block) >> 16) & 0xff);

	/*
	 * Depending on which controller this is, CSR3 has different meanings.
	 * For the Am7990 it controls DMA operations, for the Am79C960 it
	 * controls interrupt masks and transmitter algorithms. In either
	 * case, none of the flags are set.
	 *
	 */

	write_csr(sc, CSR3, 0);

	/* Let's see if it starts */
/*
printf("Enabling lnc interrupts\n");
	sc->arpcom.ac_if.if_timer = 10;
	write_csr(sc, CSR0, INIT|INEA);
*/

	/*
	 * Now that the initialisation is complete there's no reason to
	 * access anything except CSR0, so we leave RAP pointing there
	 * so we can just access RDP from now on, saving an outw each
	 * time.
	 */

	write_csr(sc, CSR0, INIT);
	for(i=0; i < 1000; i++)
		if (read_csr(sc, CSR0) & IDON)
			break;

	if (read_csr(sc, CSR0) & IDON) {
		/*
		 * Enable interrupts, start the LANCE, mark the interface as
		 * running and transmit any pending packets.
		 */
		write_csr(sc, CSR0, STRT | INEA);
		sc->arpcom.ac_if.if_flags |= IFF_RUNNING;
		sc->arpcom.ac_if.if_flags &= ~IFF_OACTIVE;
		lnc_start(&sc->arpcom.ac_if);
	} else
		log(LOG_ERR, "lnc%d: Initialisation failed\n", 
		    sc->arpcom.ac_if.if_unit);

	splx(s);
}

/*
 * The interrupt flag (INTR) will be set and provided that the interrupt enable
 * flag (INEA) is also set, the interrupt pin will be driven low when any of
 * the following occur:
 *
 * 1) Completion of the initialisation routine (IDON). 2) The reception of a
 * packet (RINT). 3) The transmission of a packet (TINT). 4) A transmitter
 * timeout error (BABL). 5) A missed packet (MISS). 6) A memory error (MERR).
 *
 * The interrupt flag is cleared when all of the above conditions are cleared.
 *
 * If the driver is reset from this routine then it first checks to see if any
 * interrupts have ocurred since the reset and handles them before returning.
 * This is because the NIC may signify a pending interrupt in CSR0 using the
 * INTR flag even if a hardware interrupt is currently inhibited (at least I
 * think it does from reading the data sheets). We may as well deal with
 * these pending interrupts now rather than get the overhead of another
 * hardware interrupt immediately upon returning from the interrupt handler.
 *
 */

void
lncintr(void *arg)
{
	lnc_softc_t *sc = arg;
	int unit = sc->arpcom.ac_if.if_unit;
	u_short csr0;

	/*
	 * INEA is the only bit that can be cleared by writing a 0 to it so
	 * we have to include it in any writes that clear other flags.
	 */

	while ((csr0 = lnc_inw(sc->rdp)) & INTR) {

		/*
		 * Clear interrupt flags early to avoid race conditions. The
		 * controller can still set these flags even while we're in
		 * this interrupt routine. If the flag is still set from the
		 * event that caused this interrupt any new events will
		 * be missed.
		 */

		lnc_outw(sc->rdp, csr0);
		/*lnc_outw(sc->rdp, IDON | CERR | BABL | MISS | MERR | RINT | TINT | INEA);*/

#ifdef notyet
		if (csr0 & IDON) {
printf("IDON\n");
			sc->arpcom.ac_if.if_timer = 0;
			write_csr(sc, CSR0, STRT | INEA);
			sc->arpcom.ac_if.if_flags |= IFF_RUNNING;
			sc->arpcom.ac_if.if_flags &= ~IFF_OACTIVE;
			lnc_start(&sc->arpcom.ac_if);
			continue;
		}
#endif

		if (csr0 & ERR) {
			if (csr0 & CERR) {
				log(LOG_ERR, "lnc%d: Heartbeat error -- SQE test failed\n", unit);
				LNCSTATS(cerr)
			}
			if (csr0 & BABL) {
				log(LOG_ERR, "lnc%d: Babble error - more than 1519 bytes transmitted\n", unit);
				LNCSTATS(babl)
				sc->arpcom.ac_if.if_oerrors++;
			}
			if (csr0 & MISS) {
				log(LOG_ERR, "lnc%d: Missed packet -- no receive buffer\n", unit);
				LNCSTATS(miss)
				sc->arpcom.ac_if.if_ierrors++;
			}
			if (csr0 & MERR) {
				log(LOG_ERR, "lnc%d: Memory error  -- Resetting\n", unit);
				LNCSTATS(merr)
				lnc_reset(sc);
				continue;
			}
		}
		if (csr0 & RINT) {
			LNCSTATS(rint)
			lnc_rint(sc);
		}
		if (csr0 & TINT) {
			LNCSTATS(tint)
			sc->arpcom.ac_if.if_timer = 0;
			lnc_tint(sc);
		}

		/*
		 * If there's room in the transmit descriptor ring then queue
		 * some more transmit packets.
		 */

		if (!(sc->arpcom.ac_if.if_flags & IFF_OACTIVE))
			lnc_start(&sc->arpcom.ac_if);
	}
}

static __inline int
mbuf_to_buffer(struct mbuf *m, char *buffer)
{

	int len=0;

	for( ; m; m = m->m_next) {
		bcopy(mtod(m, caddr_t), buffer, m->m_len);
		buffer += m->m_len;
		len += m->m_len;
	}

	return(len);
}

static __inline struct mbuf *
chain_to_cluster(struct mbuf *m)
{
	struct mbuf *new;

	MGET(new, M_DONTWAIT, MT_DATA);
	if (new) {
		MCLGET(new, M_DONTWAIT);
		if (new->m_ext.ext_buf) {
			new->m_len = mbuf_to_buffer(m, new->m_data);
			m_freem(m);
			return(new);
		} else
			m_free(new);
	}
	return(0);
}

/*
 * IFF_OACTIVE and IFF_RUNNING are checked in ether_output so it's redundant
 * to check them again since we wouldn't have got here if they were not
 * appropriately set. This is also called from lnc_init and lncintr but the
 * flags should be ok at those points too.
 */

static void
lnc_start(struct ifnet *ifp)
{

	struct lnc_softc *sc = ifp->if_softc;
	struct host_ring_entry *desc;
	int tmp;
	int end_of_packet;
	struct mbuf *head, *m;
	int len, chunk;
	int addr;
	int no_entries_needed;

	do {

		IF_DEQUEUE(&sc->arpcom.ac_if.if_snd, head);
		if (!head)
			return;

		if (sc->nic.mem_mode == DMA_MBUF) {

			no_entries_needed = 0;
			for (m=head; m; m = m->m_next)
				no_entries_needed++;

			/*
			 * We try and avoid bcopy as much as possible
			 * but there are two cases when we use it.
			 *
			 * 1) If there are not enough free entries in the ring
			 * to hold each mbuf in the chain then compact the
			 * chain into a single cluster.
			 *
			 * 2) The Am7990 and Am79C90 must not have less than
			 * 100 bytes in the first descriptor of a chained
			 * packet so it's necessary to shuffle the mbuf
			 * contents to ensure this.
			 */


			if (no_entries_needed > (NDESC(sc->ntdre) - sc->pending_transmits)) {
				if (!(head = chain_to_cluster(head))) {
					log(LOG_ERR, "lnc%d: Couldn't get mbuf for transmit packet -- Resetting \n ",ifp->if_unit);
					lnc_reset(sc);
					return;
				}
			} else if ((sc->nic.ic == LANCE) || (sc->nic.ic == C_LANCE)) {
				if ((head->m_len < 100) && (head->m_next)) {
					len = 100 - head->m_len;
					if (M_TRAILINGSPACE(head) < len) {
						/*
						 * Move data to start of data
						 * area. We assume the first
						 * mbuf has a packet header
						 * and is not a cluster.
						 */
						bcopy((caddr_t)head->m_data, (caddr_t)head->m_pktdat, head->m_len);
						head->m_data = head->m_pktdat;
					}
					m = head->m_next;
					while (m && (len > 0)) {
						chunk = min(len, m->m_len);
						bcopy(mtod(m, caddr_t), mtod(head, caddr_t) + head->m_len, chunk);
						len -= chunk;
						head->m_len += chunk;
						m->m_len -= chunk;
						m->m_data += chunk;
						if (m->m_len <= 0) {
							m = m_free(m);
							head->m_next = m;
						}
					}
				}
			}

			tmp = sc->next_to_send;

			/*
			 * On entering this loop we know that tmp points to a
			 * descriptor with a clear OWN bit.
			 */

			desc = sc->trans_ring + tmp;
			len = ETHER_MIN_LEN;
			for (m = head; m; m = m->m_next) {
				desc->buff.mbuf = m;
				addr = kvtop(m->m_data);
				desc->md->md0 = addr;
				desc->md->md1 = ((addr >> 16) & 0xff);
				desc->md->md3 = 0;
				desc->md->md2 = -m->m_len;
				sc->pending_transmits++;
				len -= m->m_len;

				INC_MD_PTR(tmp, sc->ntdre)
				desc = sc->trans_ring + tmp;
			}

			end_of_packet = tmp;
			DEC_MD_PTR(tmp, sc->ntdre)
			desc = sc->trans_ring + tmp;
			desc->md->md1 |= ENP;

			if (len > 0)
				desc->md->md2 -= len;

			/*
			 * Set OWN bits in reverse order, otherwise the Lance
			 * could start sending the packet before all the
			 * buffers have been relinquished by the host.
			 */

			while (tmp != sc->next_to_send) {
				desc->md->md1 |= OWN;
				DEC_MD_PTR(tmp, sc->ntdre)
				desc = sc->trans_ring + tmp;
			}
			sc->next_to_send = end_of_packet;
			desc->md->md1 |= STP | OWN;
		} else {
			sc->pending_transmits++;
			desc = sc->trans_ring + sc->next_to_send;
			len = mbuf_to_buffer(head, desc->buff.data);
			desc->md->md3 = 0;
			desc->md->md2 = -max(len, ETHER_MIN_LEN - ETHER_CRC_LEN);
			desc->md->md1 |= OWN | STP | ENP;
			INC_MD_PTR(sc->next_to_send, sc->ntdre)
		}

		/* Force an immediate poll of the transmit ring */
		lnc_outw(sc->rdp, TDMD | INEA);

		/*
		 * Set a timer so if the buggy Am7990.h shuts
		 * down we can wake it up.
		 */

		ifp->if_timer = 2;

		BPF_MTAP(&sc->arpcom.ac_if, head);

		if (sc->nic.mem_mode != DMA_MBUF)
			m_freem(head);

	} while (sc->pending_transmits < NDESC(sc->ntdre));

	/*
	 * Transmit ring is full so set IFF_OACTIVE
	 * since we can't buffer any more packets.
	 */

	sc->arpcom.ac_if.if_flags |= IFF_OACTIVE;
	LNCSTATS(trans_ring_full)
}

static int
lnc_ioctl(struct ifnet * ifp, u_long command, caddr_t data)
{

	struct lnc_softc *sc = ifp->if_softc;
	int s, error = 0;

	s = splimp();

	switch (command) {
	case SIOCSIFFLAGS:
#ifdef DEBUG
		if (ifp->if_flags & IFF_DEBUG)
			sc->lnc_debug = 1;
		else
			sc->lnc_debug = 0;
#endif
		if (ifp->if_flags & IFF_PROMISC) {
			if (!(sc->nic.mode & PROM)) {
				sc->nic.mode |= PROM;
				lnc_init(sc);
			}
		} else if (sc->nic.mode & PROM) {
			sc->nic.mode &= ~PROM;
			lnc_init(sc);
		}

		if ((ifp->if_flags & IFF_ALLMULTI) &&
		    !(sc->flags & LNC_ALLMULTI)) {
			sc->flags |= LNC_ALLMULTI;
			lnc_init(sc);
		} else if (!(ifp->if_flags & IFF_ALLMULTI) &&
			    (sc->flags & LNC_ALLMULTI)) {
			sc->flags &= ~LNC_ALLMULTI;
			lnc_init(sc);
		}

		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running,
			 * then stop it.
			 */
			lnc_stop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			lnc_init(sc);
		}
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		lnc_init(sc);
		error = 0;
		break;
	default:
                error = ether_ioctl(ifp, command, data);
		break;
	}
	(void) splx(s);
	return error;
}

static void
lnc_watchdog(struct ifnet *ifp)
{
	log(LOG_ERR, "lnc%d: Device timeout -- Resetting\n", ifp->if_unit);
	ifp->if_oerrors++;
	lnc_reset(ifp->if_softc);
}

#ifdef DEBUG
void
lnc_dump_state(struct lnc_softc *sc)
{
	int             i;

	printf("\nDriver/NIC [%d] state dump\n", sc->arpcom.ac_if.if_unit);
	printf("Memory access mode: %b\n", sc->nic.mem_mode, MEM_MODES);
	printf("Host memory\n");
	printf("-----------\n");

	printf("Receive ring: base = %p, next = %p\n",
	    (void *)sc->recv_ring, (void *)(sc->recv_ring + sc->recv_next));
	for (i = 0; i < NDESC(sc->nrdre); i++)
		printf("\t%d:%p md = %p buff = %p\n",
		    i, (void *)(sc->recv_ring + i),
		    (void *)(sc->recv_ring + i)->md,
		    (void *)(sc->recv_ring + i)->buff.data);

	printf("Transmit ring: base = %p, next = %p\n",
	    (void *)sc->trans_ring, (void *)(sc->trans_ring + sc->trans_next));
	for (i = 0; i < NDESC(sc->ntdre); i++)
		printf("\t%d:%p md = %p buff = %p\n",
		    i, (void *)(sc->trans_ring + i),
		    (void *)(sc->trans_ring + i)->md,
		    (void *)(sc->trans_ring + i)->buff.data);
	printf("Lance memory (may be on host(DMA) or card(SHMEM))\n");
	printf("Init block = %p\n", (void *)sc->init_block);
	printf("\tmode = %b rlen:rdra = %x:%x tlen:tdra = %x:%x\n",
	    sc->init_block->mode, INIT_MODE, sc->init_block->rlen,
	  sc->init_block->rdra, sc->init_block->tlen, sc->init_block->tdra);
	printf("Receive descriptor ring\n");
	for (i = 0; i < NDESC(sc->nrdre); i++)
		printf("\t%d buffer = 0x%x%x, BCNT = %d,\tMCNT = %u,\tflags = %b\n",
		    i, ((sc->recv_ring + i)->md->md1 & HADR),
		    (sc->recv_ring + i)->md->md0,
		    -(short) (sc->recv_ring + i)->md->md2,
		    (sc->recv_ring + i)->md->md3,
		    (((sc->recv_ring + i)->md->md1 & ~HADR) >> 8), RECV_MD1);
	printf("Transmit descriptor ring\n");
	for (i = 0; i < NDESC(sc->ntdre); i++)
		printf("\t%d buffer = 0x%x%x, BCNT = %d,\tflags = %b %b\n",
		    i, ((sc->trans_ring + i)->md->md1 & HADR),
		    (sc->trans_ring + i)->md->md0,
		    -(short) (sc->trans_ring + i)->md->md2,
		    ((sc->trans_ring + i)->md->md1 >> 8), TRANS_MD1,
		    ((sc->trans_ring + i)->md->md3 >> 10), TRANS_MD3);
	printf("\nnext_to_send = %x\n", sc->next_to_send);
	printf("\n CSR0 = %b CSR1 = %x CSR2 = %x CSR3 = %x\n\n",
	    read_csr(sc, CSR0), CSR0_FLAGS, read_csr(sc, CSR1),
	    read_csr(sc, CSR2), read_csr(sc, CSR3));

	/* Set RAP back to CSR0 */
	lnc_outw(sc->rap, CSR0);
}

void
mbuf_dump_chain(struct mbuf * m)
{

#define MBUF_FLAGS \
	"\20\1M_EXT\2M_PKTHDR\3M_EOR\4UNKNOWN\5M_BCAST\6M_MCAST"

	if (!m)
		log(LOG_DEBUG, "m == NULL\n");
	do {
		log(LOG_DEBUG, "m = %p\n", (void *)m);
		log(LOG_DEBUG, "m_hdr.mh_next = %p\n",
		    (void *)m->m_hdr.mh_next);
		log(LOG_DEBUG, "m_hdr.mh_nextpkt = %p\n",
		    (void *)m->m_hdr.mh_nextpkt);
		log(LOG_DEBUG, "m_hdr.mh_len = %d\n", m->m_hdr.mh_len);
		log(LOG_DEBUG, "m_hdr.mh_data = %p\n",
		    (void *)m->m_hdr.mh_data);
		log(LOG_DEBUG, "m_hdr.mh_type = %d\n", m->m_hdr.mh_type);
		log(LOG_DEBUG, "m_hdr.mh_flags = %b\n", m->m_hdr.mh_flags,
		    MBUF_FLAGS);
		if (!(m->m_hdr.mh_flags & (M_PKTHDR | M_EXT)))
			log(LOG_DEBUG, "M_dat.M_databuf = %p\n",
			    (void *)m->M_dat.M_databuf);
		else {
			if (m->m_hdr.mh_flags & M_PKTHDR) {
				log(LOG_DEBUG, "M_dat.MH.MH_pkthdr.len = %d\n",
				    m->M_dat.MH.MH_pkthdr.len);
				log(LOG_DEBUG,
				    "M_dat.MH.MH_pkthdr.rcvif = %p\n",
				    (void *)m->M_dat.MH.MH_pkthdr.rcvif);
				if (!(m->m_hdr.mh_flags & M_EXT))
					log(LOG_DEBUG,
					    "M_dat.MH.MH_dat.MH_databuf = %p\n",
					(void *)m->M_dat.MH.MH_dat.MH_databuf);
			}
			if (m->m_hdr.mh_flags & M_EXT) {
				log(LOG_DEBUG,
				    "M_dat.MH.MH_dat.MH_ext.ext_buff %p\n",
				    (void *)m->M_dat.MH.MH_dat.MH_ext.ext_buf);
				log(LOG_DEBUG,
				    "M_dat.MH.MH_dat.MH_ext.ext_free %p\n",
				    (void *)m->M_dat.MH.MH_dat.MH_ext.ext_free);
				log(LOG_DEBUG,
				    "M_dat.MH.MH_dat.MH_ext.ext_size %d\n",
				    m->M_dat.MH.MH_dat.MH_ext.ext_size);
			}
		}
	} while ((m = m->m_next) != NULL);
}
#endif
