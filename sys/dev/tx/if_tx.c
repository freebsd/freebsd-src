/*-
 * Copyright (c) 1997 Semen Ustimenko (semen@iclub.nsu.ru)
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
 *	$Id: if_tx.c,v 1.32 1998/07/03 23:59:09 galv Exp $
 *
 */

/*
 * EtherPower II 10/100  Fast Ethernet (tx0)
 * (aka SMC9432TX based on SMC83c170 EPIC chip)
 *
 * TODO:
 *	Deal with TX threshold (probably we should calculate it depending
 *	    on processor speed, as did the MS-DOS driver).
 *	Deal with bus mastering, i.e. i realy don't know what to do with
 *	    it and how it can improve performance.
 *	Implement FULL IFF_MULTICAST support.
 *	Calculate optimal RX and TX rings size.
 *	Test, test and test again:-)
 *	
 */

/* We should define compile time options before smc83c170.h included */
/*#define	EPIC_NOIFMEDIA	1*/
/*#define	EPIC_USEIOSPACE	1*/
/*#define	EARLY_RX	1*/
/*#define	EARLY_TX	1*/
/*#define	EPIC_DEBUG	1*/

#if defined(EPIC_DEBUG)
#define dprintf(a) printf a
#else
#define dprintf(a)
#endif

/* Macro to get either mbuf cluster or nothing */
#define EPIC_MGETCLUSTER(m) \
	{ MGETHDR((m),M_DONTWAIT,MT_DATA); \
	  if (m) { \
	    MCLGET((m),M_DONTWAIT); \
	    if( NULL == ((m)->m_flags & M_EXT) ){ \
	      m_freem(m); \
	      (m) = NULL; \
	    } \
	  } \
	}

#include "pci.h"
#if NPCI > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sockio.h>
#include <net/if.h>
#if defined(SIOCSIFMEDIA) && !defined(EPIC_NOIFMEDIA)
#include <net/if_media.h>
#endif
#include <net/if_mib.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/clock.h>

#include <pci/pcivar.h>
#include <pci/smc83c170.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

/*
 * Global variables
 */
static u_long epic_pci_count;
static struct pci_device txdevice = { 
	"tx",
	epic_pci_probe,
	epic_pci_attach,
	&epic_pci_count,
	NULL };

/*
 * Append this driver to pci drivers list
 */
DATA_SET ( pcidevice_set, txdevice );

/*
 * ifioctl function
 *
 * splimp() invoked here
 */
static int
epic_ifioctl __P((
    register struct ifnet * ifp,
    u_long command, caddr_t data))
{
	epic_softc_t *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *) data;
	int x, error = 0;

	x = splimp();

	switch (command) {

	case SIOCSIFADDR:
	case SIOCGIFADDR:
		ether_ioctl(ifp, command, data);
		break;

	case SIOCSIFFLAGS:
		/*
		 * If the interface is marked up and stopped, then start it.
		 * If it is marked down and running, then stop it.
		 */
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_flags & IFF_RUNNING) == 0) {
				epic_init(sc);
				break;
			}
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				epic_stop(sc);
				break;
			}
		}

		/* Handle IFF_PROMISC flag */
		epic_set_rx_mode(sc);

#if !defined(_NET_IF_MEDIA_H_)
		/* Handle IFF_LINKx flags */
		epic_set_media_speed(sc);
#endif

		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:

		/* Update out multicast list */
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
		epic_set_mc_table(sc);
		error = 0;
#else
		error = (command == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->epic_ac) :
		    ether_delmulti(ifr, &sc->epic_ac);

		if (error == ENETRESET) {
			epic_set_mc_table(sc);
			error = 0;
		}
#endif
		break;

	case SIOCSIFMTU:
		/*
		 * Set the interface MTU.
		 */
		if (ifr->ifr_mtu > ETHERMTU) {
			error = EINVAL;
		} else {
			ifp->if_mtu = ifr->ifr_mtu;
		}
		break;

#if defined(_NET_IF_MEDIA_H_)
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->ifmedia, command);
		break;
#endif

	default:
		error = EINVAL;
	}
	splx(x);

	return error;
}

/*
 * ifstart function
 *
 * splimp() assumed to be done
 */
static void
epic_ifstart(struct ifnet * const ifp){
	epic_softc_t *sc = ifp->if_softc;
	struct epic_tx_buffer *buf;
	struct epic_tx_desc *desc;
	struct epic_frag_list *flist;
	struct mbuf *m,*m0;

#if defined(EPIC_DEBUG)
	if( sc->epic_if.if_flags & IFF_DEBUG ) epic_dump_state(sc);
#endif
	/* If no link is established,   */
	/* simply free all mbufs in queue */
	PHY_READ_2( sc, DP83840_BMSR );
	if( !(BMSR_LINK_STATUS & PHY_READ_2( sc, DP83840_BMSR )) ){
		IF_DEQUEUE( &(sc->epic_if.if_snd), m0 );
		while( m0 ) {
			m_freem(m0);
			IF_DEQUEUE( &(sc->epic_if.if_snd), m0 );
		}
		return;
	}

	/* Link is OK, queue packets to NIC */
	while( sc->pending_txs < TX_RING_SIZE  ){
		buf = sc->tx_buffer + sc->cur_tx;
		desc = sc->tx_desc + sc->cur_tx;
		flist = sc->tx_flist + sc->cur_tx;

		/* Get next packet to send */
		IF_DEQUEUE( &(sc->epic_if.if_snd), m0 );

		/* If nothing to send, return */
		if( NULL == m0 ) return;

		/* If descriptor is busy, set IFF_OACTIVE and exit */
		if( desc->status & 0x8000 ) {
			dprintf(("\ntx%d: desc is busy in ifstart, up and down interface please",sc->unit));
			break;
		}

		if( buf->mbuf ) {
			dprintf(("\ntx%d: mbuf not freed in ifstart, up and down interface plase",sc->unit));
			break;
		}

		/* Fill fragments list */
		flist->numfrags = 0;
		for(m=m0;(NULL!=m)&&(flist->numfrags<63);m=m->m_next) {
			flist->frag[flist->numfrags].fraglen = m->m_len; 
			flist->frag[flist->numfrags].fragaddr = vtophys( mtod(m, caddr_t) );
			flist->numfrags++;
		}

		/* If packet was more than 63 parts, */
		/* recopy packet to new allocated mbuf cluster */
		if( NULL != m ){
			EPIC_MGETCLUSTER(m);
			if( NULL == m ){
				printf("\ntx%d: cannot allocate mbuf cluster",sc->unit);
				m_freem(m0);
				sc->epic_if.if_oerrors++;
				continue;
			}

			m_copydata( m0, 0, m0->m_pkthdr.len, mtod(m,caddr_t) );
			flist->frag[0].fraglen = m->m_pkthdr.len = m->m_len = m0->m_pkthdr.len;
			m->m_pkthdr.rcvif = &sc->epic_if;

			flist->numfrags = 1;
			flist->frag[0].fragaddr = vtophys( mtod(m, caddr_t) );
			m_freem(m0);
			m0 = m;
		}

		/* Save mbuf */
		buf->mbuf = m0;

		/* Packet queued successful */
		sc->pending_txs++;

		/* Switch to next descriptor */
		sc->cur_tx = ( sc->cur_tx + 1 ) % TX_RING_SIZE;

		/* Does not generate TXC */
		desc->control = 0x01;

		/* Packet should be at least ETHER_MIN_LEN */
		desc->txlength = max(m0->m_pkthdr.len,ETHER_MIN_LEN-ETHER_CRC_LEN);

		/* Pass ownership to the chip */
		desc->status = 0x8000;

		/* Trigger an immediate transmit demand. */
		CSR_WRITE_4( sc, COMMAND, COMMAND_TXQUEUED );

#if defined(EPIC_DEBUG)
		if( sc->epic_if.if_flags & IFF_DEBUG ) epic_dump_state(sc);
#endif

		/* Set watchdog timer */
		ifp->if_timer = 8;

#if NBPFILTER > 0
		if( ifp->if_bpf ) bpf_mtap( ifp, m0 );
#endif
	}

	sc->epic_if.if_flags |= IFF_OACTIVE;

	return;
	
}

/*
 *
 * splimp() invoked before epic_intr_normal()
 */
static __inline void
epic_rx_done __P((
	epic_softc_t *sc ))
{
        int i = 0;
	u_int16_t len;
	struct epic_rx_buffer *buf;
	struct epic_rx_desc *desc;
	struct mbuf *m;
	struct ether_header *eh;

	while( !(sc->rx_desc[sc->cur_rx].status & 0x8000) && \
		i++ < RX_RING_SIZE ) {

		buf = sc->rx_buffer + sc->cur_rx;
		desc = sc->rx_desc + sc->cur_rx;

		/* Switch to next descriptor */
		sc->cur_rx = (sc->cur_rx+1) % RX_RING_SIZE;

		/* Check for errors, this should happend */
		/* only if SAVE_ERRORED_PACKETS is set, */
		/* normaly rx errors generate RXE interrupt */
		if( !(desc->status & 1) ) {
			dprintf(("\ntx%d: Rx error status: 0x%x",sc->unit,desc->status));
			sc->epic_if.if_ierrors++;
			desc->status = 0x8000;
			continue;
		}

		/* Save packet length and mbuf contained packet */ 
		len = desc->rxlength - ETHER_CRC_LEN;
		m = buf->mbuf;

		/* Try to get mbuf cluster */
		EPIC_MGETCLUSTER( buf->mbuf );
		if( NULL == buf->mbuf ) { 
			printf("\ntx%d: cannot allocate mbuf cluster",sc->unit);
			buf->mbuf = m;
			desc->status = 0x8000;
			sc->epic_if.if_ierrors++;
			continue;
		}

		/* Point to new mbuf, and give descriptor to chip */
		desc->bufaddr = vtophys( mtod( buf->mbuf, caddr_t ) );
		desc->status = 0x8000;
		
		/* First mbuf in packet holds the ethernet and packet headers */
		eh = mtod( m, struct ether_header * );
		m->m_pkthdr.rcvif = &(sc->epic_if);
		m->m_pkthdr.len = m->m_len = len;

#if NBPFILTER > 0
		/* Give mbuf to BPFILTER */
		if( sc->epic_if.if_bpf ) bpf_mtap( &sc->epic_if, m );

		/* Accept only our packets, broadcasts and multicasts */
		if( (eh->ether_dhost[0] & 1) == 0 &&
		    bcmp(eh->ether_dhost,sc->epic_ac.ac_enaddr,ETHER_ADDR_LEN)){
			m_freem(m);
			continue;
		}
#endif

		/* Second mbuf holds packet ifself */
		m->m_pkthdr.len = m->m_len = len - sizeof(struct ether_header);
		m->m_data += sizeof( struct ether_header );

		/* Give mbuf to OS */
		ether_input(&sc->epic_if, eh, m);

		/* Successfuly received frame */
		sc->epic_if.if_ipackets++;
        }

	return;
}

/*
 * Synopsis: Do last phase of transmission. I.e. if desc is 
 * transmitted, decrease pending_txs counter, free mbuf contained
 * packet, switch to next descriptor and repeat until no packets
 * are pending or descriptro is not transmitted yet.
 */
static __inline void
epic_tx_done __P(( 
    register epic_softc_t *sc ))
{
	struct epic_tx_buffer *buf;
	struct epic_tx_desc *desc;
	u_int16_t status;

	while( sc->pending_txs > 0 ){
		buf = sc->tx_buffer + sc->dirty_tx;
		desc = sc->tx_desc + sc->dirty_tx;
		status = desc->status;

		/* If packet is not transmitted, thou followed */
		/* packets are not transmitted too */
		if( status & 0x8000 ) break;

		/* Packet is transmitted. Switch to next and */
		/* free mbuf */
		sc->pending_txs--;
		sc->dirty_tx = (sc->dirty_tx + 1) % TX_RING_SIZE;
		m_freem( buf->mbuf );
		buf->mbuf = NULL;

		/* Check for errors and collisions */
		if( status & 0x0001 ) sc->epic_if.if_opackets++;
		else sc->epic_if.if_oerrors++;
		sc->epic_if.if_collisions += (status >> 8) & 0x1F;
#if defined(EPIC_DEBUG)
		if( (status & 0x1001) == 0x1001 ) 
			dprintf(("\ntx%d: frame not transmitted due collisions",sc->unit));
#endif
	}

	if( sc->pending_txs < TX_RING_SIZE ) 
		sc->epic_if.if_flags &= ~IFF_OACTIVE;
}

/*
 * Interrupt function
 *
 * splimp() assumed to be done 
 */
static void
epic_intr_normal(
    void *arg)
{
	epic_softc_t * sc = (epic_softc_t *) arg;
        int status,i=4;

do {
	status = CSR_READ_4( sc, INTSTAT );
	CSR_WRITE_4( sc, INTSTAT, status );

	if( status & (INTSTAT_RQE|INTSTAT_RCC|INTSTAT_OVW) ) {
		epic_rx_done( sc );
		if( status & (INTSTAT_RQE|INTSTAT_OVW) ){
#if defined(EPIC_DEBUG)
			if( status & INTSTAT_OVW ) 
				printf("\ntx%d: Rx buffer overflowed",sc->unit);
			if( status & INTSTAT_RQE ) 
				printf("\ntx%d: Rx FIFO overflowed",sc->unit);
			if( sc->epic_if.if_flags & IFF_DEBUG ) 
				epic_dump_state(sc);
#endif
			if( !(CSR_READ_4( sc, COMMAND ) & COMMAND_RXQUEUED) )
				CSR_WRITE_4( sc, COMMAND, COMMAND_RXQUEUED );
			sc->epic_if.if_ierrors++;
		}
	}

	if( status & (INTSTAT_TXC|INTSTAT_TCC|INTSTAT_TQE) ) {
		epic_tx_done( sc );
#if defined(EPIC_DEBUG)
		if( (status & (INTSTAT_TQE | INTSTAT_TCC)) && (sc->pending_txs > 1) )
			printf("\ntx%d: %d packets pending after TQE/TCC",sc->unit,sc->pending_txs);
#endif
		if( !(sc->epic_if.if_flags & IFF_OACTIVE) && sc->epic_if.if_snd.ifq_head )
			epic_ifstart( &sc->epic_if );
	}

	if( (status & INTSTAT_GP2) && (QS6612_OUI == sc->phyid) ) {
		u_int32_t phystatus;

		phystatus = PHY_READ_2( sc, QS6612_INTSTAT );

		if( phystatus & INTSTAT_AN_COMPLETE ) {
			u_int32_t bmcr;
			if( epic_autoneg(sc) == EPIC_FULL_DUPLEX ) {
				bmcr = BMCR_FULL_DUPLEX | PHY_READ_2( sc, DP83840_BMCR );
				CSR_WRITE_4( sc, TXCON, TXCON_FULL_DUPLEX | TXCON_DEFAULT );
			} else {
				/* Default to half-duplex */
				bmcr = ~BMCR_FULL_DUPLEX & PHY_READ_2( sc, DP83840_BMCR );
				CSR_WRITE_4( sc, TXCON, TXCON_DEFAULT );
			}

			/* There is apparently QS6612 chip bug: */
			/* BMCR_FULL_DUPLEX flag is not updated by */
			/* autonegotiation process, so update it by hands */
			/* so we can rely on it in epic_ifmedia_status() */
			PHY_WRITE_2( sc, DP83840_BMCR, bmcr );
		}

		PHY_READ_2(sc, DP83840_BMSR);
		if( !(PHY_READ_2(sc, DP83840_BMSR) & BMSR_LINK_STATUS) ) {
			dprintf(("\ntx%d: WARNING! link down",sc->unit));
			sc->flags |= EPIC_LINK_DOWN;
		} else {
			dprintf(("\ntx%d: link up",sc->unit));
			sc->flags &= ~EPIC_LINK_DOWN;
		}

		/* We should clear GP2 int again after we clear it on PHY */
		CSR_WRITE_4( sc, INTSTAT, INTSTAT_GP2 ); 
	}

	/* Check for errors */
	if( status & (INTSTAT_FATAL|INTSTAT_PMA|INTSTAT_PTA|INTSTAT_APE|INTSTAT_DPE|INTSTAT_TXU|INTSTAT_RXE) ){
		if( status & (INTSTAT_FATAL|INTSTAT_PMA|INTSTAT_PTA|INTSTAT_APE|INTSTAT_DPE) ){
			printf("\ntx%d: PCI fatal error occured (%s%s%s%s)",
				sc->unit,
				(status&INTSTAT_PMA)?"PMA":"",
				(status&INTSTAT_PTA)?" PTA":"",
				(status&INTSTAT_APE)?" APE":"",
				(status&INTSTAT_DPE)?" DPE":""
			);

			epic_dump_state(sc);

			epic_stop(sc);
			epic_init(sc);
		
			return;
		}

		if (status & INTSTAT_RXE) {
			dprintf(("\ntx%d: CRC/Alignment error",sc->unit));
			sc->epic_if.if_ierrors++;
		}

		/* Tx FIFO underflow. Should not happend if */
		/* we don't use early tx, handle it anyway */
		if (status & INTSTAT_TXU) {
			dprintf(("\ntx%d: Tx underrun error",sc->unit));
			sc->epic_if.if_oerrors++;

			/* Restart the transmit process. */
			CSR_WRITE_4(sc, COMMAND, COMMAND_TXUGO | COMMAND_TXQUEUED);
		}
	}

} while( i-- && (CSR_READ_4(sc, INTSTAT) & INTSTAT_INT_ACTV) );

	/* If no packets are pending, thus no timeouts */
	if( sc->pending_txs == 0 )
		sc->epic_if.if_timer = 0;

	return;
}

/*
 * Synopsis: This one is called if packets wasn't transmitted
 * during timeout. Try to deallocate transmitted packets, and 
 * if success continue to work.
 *
 * splimp() invoked here
 */
static void
epic_ifwatchdog __P((
    struct ifnet *ifp))
{
	epic_softc_t *sc = ifp->if_softc;
	int x;

	x = splimp();

	printf("\ntx%d: device timeout %d packets, ", sc->unit,sc->pending_txs);

	/* Try to finish queued packets */
	epic_tx_done( sc );

	/* If not successful */
	if( sc->pending_txs > 0 ){
#if defined(EPIC_DEBUG)
		if( sc->epic_if.if_flags & IFF_DEBUG ) epic_dump_state(sc);
#endif
		ifp->if_oerrors+=sc->pending_txs;

		/* Reinitialize board */
		printf("reinitialization");
		epic_stop(sc);
		epic_init(sc);

	} else 
		printf("seems we can continue normaly");

	/* Start output */
	if( sc->epic_if.if_snd.ifq_head ) epic_ifstart(&sc->epic_if);

	splx(x);
}

/*
 * Synopsis: Check if PCI id corresponds with board id.
 */
static char*
epic_pci_probe(
    pcici_t config_id,
    pcidi_t device_id)
{
	if( PCI_VENDORID(device_id) != SMC_VENDORID )
		return NULL;

	if( PCI_CHIPID(device_id) == CHIPID_83C170 )
		return "SMC 83c170";

	return NULL;
}

/*
 * Synopsis: Allocate memory for softc, descriptors and frag lists.
 * Connect to interrupt, and get memory/io address of card registers.
 * Preinitialize softc structure, attach to if manager, ifmedia manager
 * and bpf. Read media configuration and etc.
 *
 * splimp() invoked here
 */
static void
epic_pci_attach(
    pcici_t config_id,
    int unit)
{
	struct ifnet * ifp;
	epic_softc_t *sc;
#if defined(EPIC_USEIOSPACE)
	u_int32_t iobase;
#else
	caddr_t	pmembase;
#endif
	int i,k,s,tmp;
	u_int32_t pool;

	/* Allocate memory for softc, hardware descriptors and frag lists */
	sc = (epic_softc_t *) malloc(
		sizeof(epic_softc_t) +
		sizeof(struct epic_frag_list)*TX_RING_SIZE +
		sizeof(struct epic_rx_desc)*RX_RING_SIZE + 
		sizeof(struct epic_tx_desc)*TX_RING_SIZE + PAGE_SIZE,
		M_DEVBUF, M_NOWAIT);

	if (sc == NULL)	return;

	/* Align pool on PAGE_SIZE */
	pool = ((u_int32_t)sc) + sizeof(epic_softc_t);
	pool = (pool + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	/* Preinitialize softc structure */
    	bzero(sc, sizeof(epic_softc_t));		
	sc->unit = unit;

	/* Fill ifnet structure */
	ifp = &sc->epic_if;
	ifp->if_unit = unit;
	ifp->if_name = "tx";
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST|IFF_SIMPLEX|IFF_MULTICAST;
	ifp->if_ioctl = epic_ifioctl;
	ifp->if_start = epic_ifstart;
	ifp->if_watchdog = epic_ifwatchdog;
	ifp->if_init = (if_init_f_t*)epic_init;
	ifp->if_timer = 0;
	ifp->if_output = ether_output;

	/* Get iobase or membase */
#if defined(EPIC_USEIOSPACE)
	if (!pci_map_port(config_id, PCI_CBIO,(u_short *) &(sc->iobase))) {
		printf("tx%d: cannot map port\n",unit);
		free(sc, M_DEVBUF);
		return;
	}
#else
	if (!pci_map_mem(config_id, PCI_CBMA,(vm_offset_t *) &(sc->csr),(vm_offset_t *) &pmembase)) {
		printf("tx%d: cannot map memory\n",unit); 
		free(sc, M_DEVBUF);
		return;
	}
#endif

	sc->tx_flist = (void *)pool;
	pool += sizeof(struct epic_frag_list)*TX_RING_SIZE;
	sc->rx_desc = (void *)pool;
	pool += sizeof(struct epic_rx_desc)*RX_RING_SIZE;
	sc->tx_desc = (void *)pool;

	/* Bring the chip out of low-power mode. */
	CSR_WRITE_4( sc, GENCTL, 0x0000 );

	/* Magic?!  If we don't set this bit the MII interface won't work. */
	CSR_WRITE_4( sc, TEST1, 0x0008 );

	/* Read mac address from EEPROM */
	for (i = 0; i < ETHER_ADDR_LEN / sizeof(u_int16_t); i++)
		((u_int16_t *)sc->epic_macaddr)[i] = epic_read_eeprom(sc,i);

	/* Display ethernet address ,... */
	printf("tx%d: address %02x:%02x:%02x:%02x:%02x:%02x,",sc->unit,
		sc->epic_macaddr[0],sc->epic_macaddr[1],sc->epic_macaddr[2],
		sc->epic_macaddr[3],sc->epic_macaddr[4],sc->epic_macaddr[5]);

	/* board type and ... */
	printf(" type ");
	for(i=0x2c;i<0x32;i++) {
		tmp = epic_read_eeprom( sc, i );
		if( ' ' == (u_int8_t)tmp ) break;
		printf("%c",(u_int8_t)tmp);
		tmp >>= 8;
		if( ' ' == (u_int8_t)tmp ) break;
		printf("%c",(u_int8_t)tmp);
	}

	/* Read current media config and display it too */
	i = PHY_READ_2( sc, DP83840_BMCR );
#if defined(_NET_IF_MEDIA_H_)
	tmp = IFM_ETHER;
#endif
	if( i & BMCR_AUTONEGOTIATION ){
		printf(", Auto-Neg ");

		/* To avoid bug in QS6612 read LPAR enstead of BMSR */
		i = PHY_READ_2( sc, DP83840_LPAR );
		if( i & (ANAR_100_TX|ANAR_100_TX_FD) ) printf("100Mbps ");
		else printf("10Mbps ");
		if( i & (ANAR_10_FD|ANAR_100_TX_FD) ) printf("FD");
#if defined(_NET_IF_MEDIA_H_)
		tmp |= IFM_AUTO;
#endif
	} else {
#if !defined(_NET_IF_MEDIA_H_)
		ifp->if_flags |= IFF_LINK0;
#endif
		if( i & BMCR_100MBPS ) {
			printf(", 100Mbps ");
#if defined(_NET_IF_MEDIA_H_)
			tmp |= IFM_100_TX;
#else
			ifp->if_flags |= IFF_LINK2;
#endif
		} else {
			printf(", 10Mbps ");
#if defined(_NET_IF_MEDIA_H_)
			tmp |= IFM_10_T;
#endif
		}
		if( i & BMCR_FULL_DUPLEX ) {
			printf("FD");
#if defined(_NET_IF_MEDIA_H_)
			tmp |= IFM_FDX;
#else
			ifp->if_flags |= IFF_LINK1;
#endif
		}
	}

	/* Init ifmedia interface */
#if defined(SIOCSIFMEDIA) && !defined(EPIC_NOIFMEDIA)
	ifmedia_init(&sc->ifmedia,0,epic_ifmedia_change,epic_ifmedia_status);
	ifmedia_add(&sc->ifmedia,IFM_ETHER|IFM_10_T,0,NULL);
	ifmedia_add(&sc->ifmedia,IFM_ETHER|IFM_10_T|IFM_LOOP,0,NULL);
	ifmedia_add(&sc->ifmedia,IFM_ETHER|IFM_10_T|IFM_FDX,0,NULL);
	ifmedia_add(&sc->ifmedia,IFM_ETHER|IFM_100_TX,0,NULL);
	ifmedia_add(&sc->ifmedia,IFM_ETHER|IFM_100_TX|IFM_LOOP,0,NULL);
	ifmedia_add(&sc->ifmedia,IFM_ETHER|IFM_100_TX|IFM_FDX,0,NULL);
	ifmedia_add(&sc->ifmedia,IFM_ETHER|IFM_AUTO,0,NULL);
	ifmedia_add(&sc->ifmedia,IFM_ETHER|IFM_LOOP,0,NULL);
	ifmedia_set(&sc->ifmedia, tmp);
#endif

	/* Identify PHY */
	sc->phyid = PHY_READ_2(sc, DP83840_PHYIDR1 )<<6;
	sc->phyid|= (PHY_READ_2( sc, DP83840_PHYIDR2 )>>10)&0x3F;
	if( QS6612_OUI != sc->phyid ) printf("tx%d: WARNING! phy unknown (0x%x), ",sc->unit,sc->phyid);

	s = splimp();

	/* Map interrupt */
	if( !pci_map_int(config_id, epic_intr_normal, (void*)sc, &net_imask) ) {
		printf("tx%d: couldn't map interrupt\n",unit);
		free(sc, M_DEVBUF);
		return;
	}

	/* Set shut down routine to stop DMA processes on reboot */
	at_shutdown(epic_shutdown, sc, SHUTDOWN_POST_SYNC);

	/*  Attach to if manager */
	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPFILTER > 0
	bpfattach(ifp,DLT_EN10MB, sizeof(struct ether_header));
#endif

	splx(s);

	printf("\n");

	return;
}

#if defined(SIOCSIFMEDIA) && !defined(EPIC_NOIFMEDIA)
static int
epic_ifmedia_change __P((
    struct ifnet * ifp))
{
	if (IFM_TYPE(((epic_softc_t *)(ifp->if_softc))->ifmedia.ifm_media) != IFM_ETHER)
        	return (EINVAL);

	epic_set_media_speed( ifp->if_softc );

	return 0;
}

static void
epic_ifmedia_status __P((
    struct ifnet * ifp,
    struct ifmediareq *ifmr))
{
	epic_softc_t *sc = ifp->if_softc;
	u_int32_t	bmcr;
	u_int32_t	bmsr;

	bmcr = PHY_READ_2( sc, DP83840_BMCR );

	PHY_READ_2( sc, DP83840_BMSR );
	bmsr = PHY_READ_2( sc, DP83840_BMSR );

	ifmr->ifm_active = IFM_ETHER;
	ifmr->ifm_status = IFM_AVALID;

	if( !(bmsr & BMSR_LINK_STATUS) ) { 
		ifmr->ifm_active |= (bmcr&BMCR_AUTONEGOTIATION)?IFM_AUTO:IFM_NONE;
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;
	ifmr->ifm_active |= (bmcr&BMCR_100MBPS)?IFM_100_TX:IFM_10_T;
	ifmr->ifm_active |= (bmcr&BMCR_FULL_DUPLEX)?IFM_FDX:0;
	ifmr->ifm_active |= ((CSR_READ_4(sc,TXCON)&TXCON_LOOPBACK_MODE)==TXCON_LOOPBACK_MODE_INT)?IFM_LOOP:0;
}
#endif

/*
 * Reset chip, PHY, allocate rings
 * 
 * splimp() invoked here
 */
static int 
epic_init __P((
    epic_softc_t * sc))
{       
	struct ifnet *ifp = &sc->epic_if;
	int i,s;
 
	s = splimp();

	/* Soft reset the chip */
	CSR_WRITE_4( sc, GENCTL, GENCTL_SOFT_RESET );

	/* Reset takes 15 pci ticks which depends on processor speed */
	DELAY(1);

	/* Wake up */
	CSR_WRITE_4( sc, GENCTL, 0 );

	/* ?????? */
	CSR_WRITE_4( sc, TEST1, 0x0008);

	/* Initialize rings */
	if( epic_init_rings( sc ) ) {
		printf("\ntx%d: failed to initialize rings",sc->unit);
		splx(s);
		return -1;
	}	

	/* Give rings to EPIC */
	CSR_WRITE_4( sc, PRCDAR, vtophys( sc->rx_desc ) );
	CSR_WRITE_4( sc, PTCDAR, vtophys( sc->tx_desc ) );

	/* Put node address to EPIC */
	CSR_WRITE_4( sc, LAN0, ((u_int16_t *)sc->epic_macaddr)[0] );
        CSR_WRITE_4( sc, LAN1, ((u_int16_t *)sc->epic_macaddr)[1] );
	CSR_WRITE_4( sc, LAN2, ((u_int16_t *)sc->epic_macaddr)[2] );

#if defined(EARLY_TX)
	/* Set transmit threshold */
	CSR_WRITE_4( sc, ETXTHR, TRANSMIT_THRESHOLD );
#endif

	CSR_WRITE_4( sc, IPG, 0x1010 );

	/* Compute and set RXCON. */
	epic_set_rx_mode( sc );

	/* Set media speed mode */
	epic_init_phy( sc );
	epic_set_media_speed( sc );

	/* Set multicast table */
	epic_set_mc_table( sc );

	/* Enable interrupts by setting the interrupt mask. */
	CSR_WRITE_4( sc, INTMASK,
		INTSTAT_RCC | INTSTAT_RQE | INTSTAT_OVW | INTSTAT_RXE |
		INTSTAT_TXC | INTSTAT_TCC | INTSTAT_TQE | INTSTAT_TXU |
		INTSTAT_FATAL |
		((QS6612_OUI == sc->phyid)?INTSTAT_GP2:0) );

	/* Enable interrupts,  set for PCI read multiple and etc */
	CSR_WRITE_4( sc, GENCTL,
		GENCTL_ENABLE_INTERRUPT | GENCTL_MEMORY_READ_MULTIPLE |
		GENCTL_ONECOPY | GENCTL_RECEIVE_FIFO_THRESHOLD64 );

	/* Mark interface running ... */
	if( ifp->if_flags & IFF_UP ) ifp->if_flags |= IFF_RUNNING;
	else ifp->if_flags &= ~IFF_RUNNING;

	/* ... and free */
	ifp->if_flags &= ~IFF_OACTIVE;

	/* Start Rx process */
	epic_start_activity(sc);

	splx(s);
	return 0;
}

/*
 * Synopsis: calculate and set Rx mode
 */
static void
epic_set_rx_mode(
    epic_softc_t * sc)
{
	u_int32_t flags = sc->epic_if.if_flags;
        u_int32_t rxcon = RXCON_DEFAULT | RXCON_RECEIVE_MULTICAST_FRAMES | RXCON_RECEIVE_BROADCAST_FRAMES;

	rxcon |= (flags & IFF_PROMISC)?RXCON_PROMISCUOUS_MODE:0;

	CSR_WRITE_4( sc, RXCON, rxcon );

	return;
}

/*
 * Synopsis: Reset PHY and do PHY-special initialization:
 */
static void
epic_init_phy __P((
    epic_softc_t * sc))
{
	u_int32_t i;

	/* Reset PHY */
	PHY_WRITE_2( sc, DP83840_BMCR, BMCR_RESET );
	for(i=0;i<0x100000;i++)
		if( !(PHY_READ_2( sc, DP83840_BMCR ) & BMCR_RESET) ) break;

	if( PHY_READ_2( sc, DP83840_BMCR ) & BMCR_RESET )
		printf("\ntx%d: WARNING! cannot reset PHY",sc->unit);

	switch( sc->phyid ){
	case QS6612_OUI:
		/* Init QS6612 and EPIC to generate interrupt when AN complete*/
		CSR_WRITE_4( sc, NVCTL, NVCTL_GP1_OUTPUT_ENABLE );
		PHY_READ_2( sc, QS6612_INTSTAT );
		PHY_WRITE_2( sc, QS6612_INTMASK, INTMASK_THUNDERLAN | INTSTAT_AN_COMPLETE | INTSTAT_LINK_STATUS );

		/* Enable QS6612 extended cable length capabilites */
		PHY_WRITE_2( sc, QS6612_MCTL, PHY_READ_2( sc,QS6612_MCTL ) | MCTL_BTEXT );
		break;
	default:
		break;
	}
}

/*
 * Synopsis: Set PHY to media type specified by IFF_LINK* flags or
 * ifmedia structure.
 */
static void
epic_set_media_speed __P((
    epic_softc_t * sc))
{
	u_int16_t media;

#if defined(_NET_IF_MEDIA_H_)
	u_int32_t tgtmedia = sc->ifmedia.ifm_cur->ifm_media;

	if( IFM_SUBTYPE(tgtmedia) != IFM_AUTO ){
		/* Set mode */
		media = (IFM_SUBTYPE(tgtmedia)==IFM_100_TX) ? BMCR_100MBPS : 0;
		media|= (tgtmedia&IFM_FDX) ? BMCR_FULL_DUPLEX : 0;

		sc->epic_if.if_baudrate = 
			(IFM_SUBTYPE(tgtmedia)==IFM_100_TX)?100000000:10000000;

		PHY_WRITE_2( sc, DP83840_BMCR, media );

		media = TXCON_DEFAULT;
		if( tgtmedia & IFM_FDX ) media |= TXCON_FULL_DUPLEX;
		else if( tgtmedia & IFM_LOOP ) media |= TXCON_LOOPBACK_MODE_INT;
		
		CSR_WRITE_4( sc, TXCON, media );
	}
#else
	struct ifnet *ifp = &sc->epic_if;

	if( ifp->if_flags & IFF_LINK0 ) {
		/* Set mode */
		media = (ifp->if_flags & IFF_LINK2) ? BMCR_100MBPS : 0;
		media|= (ifp->if_flags & IFF_LINK1) ? BMCR_FULL_DUPLEX : 0;

		sc->epic_if.if_baudrate = 
			(ifp->if_flags & IFF_LINK2)?100000000:10000000;

		PHY_WRITE_2( sc, DP83840_BMCR, media );

		media = TXCON_DEFAULT;
		media |= (ifp->if_flags&IFF_LINK2)?TXCON_FULL_DUPLEX:0;
 
		CSR_WRITE_4( sc, TXCON, media );
	}
#endif
	  else {
		sc->epic_if.if_baudrate = 100000000;

		CSR_WRITE_4( sc, TXCON, TXCON_DEFAULT );

		/* Set and restart autoneg */
		PHY_WRITE_2( sc, DP83840_BMCR,
			BMCR_AUTONEGOTIATION | BMCR_RESTART_AUTONEG );

		/* If it is not QS6612 PHY, try to get result of autoneg. */
		if( QS6612_OUI != sc->phyid ) {
			/* Wait 3 seconds for the autoneg to finish
			 * This is the recommended time from the DP83840A data
			 * sheet Section 7.1
			 */
        		DELAY(3000000);
			
			if( epic_autoneg(sc) == EPIC_FULL_DUPLEX )
				CSR_WRITE_4( sc, TXCON, TXCON_FULL_DUPLEX|TXCON_DEFAULT);
		}
		/* Else it will be done when GP2 int occured */
	}

	return;
}

/*
 * This functions get results of the autoneg processes of the phy
 * It implements the workaround that is described in section 7.2 & 7.3 of the 
 * DP83840A data sheet
 * http://www.national.com/ds/DP/DP83840A.pdf
 */
static int 
epic_autoneg(
    epic_softc_t * sc)
{
	struct ifnet *ifp = &sc->epic_if;
	u_int16_t media;
	u_int16_t i;

        /* BMSR must be read twice to update the link status bit
	 * since that bit is a latch bit
         */
	PHY_READ_2( sc, DP83840_BMSR);
	i = PHY_READ_2( sc, DP83840_BMSR);
        
        if ((i & BMSR_LINK_STATUS) && (i & BMSR_AUTONEG_COMPLETE)){
		i = PHY_READ_2( sc, DP83840_LPAR );

		if ( i & (ANAR_100_TX_FD|ANAR_10_FD) )
			return 	EPIC_FULL_DUPLEX;
		else
			return EPIC_HALF_DUPLEX;
        } else {   
		/*Auto-negotiation or link status is not 1
		  Thus the auto-negotiation failed and one
		  must take other means to fix it.
		 */

		/* ANER must be read twice to get the correct reading for the 
		 * Multiple link fault bit -- it is a latched bit
	 	 */
 		PHY_READ_2( sc, DP83840_ANER );
		i = PHY_READ_2( sc, DP83840_ANER );
	
		if ( i & ANER_MULTIPLE_LINK_FAULT ) {
			/* it can be forced to 100Mb/s Half-Duplex */
	 		media = PHY_READ_2( sc, DP83840_BMCR );
			media &= ~(BMCR_AUTONEGOTIATION | BMCR_FULL_DUPLEX);
			media |= BMCR_100MBPS;
			PHY_WRITE_2( sc, DP83840_BMCR, media );
		
			/* read BMSR again to determine link status */
			PHY_READ_2( sc, DP83840_BMSR );
			i=PHY_READ_2( sc, DP83840_BMSR );
		
			if (i & BMSR_LINK_STATUS){
				/* port is linked to the non Auto-Negotiation
				 * 100Mbs partner.
			 	 */
				return EPIC_HALF_DUPLEX;
			}
			else {
				media = PHY_READ_2( sc, DP83840_BMCR);
				media &= ~(BMCR_AUTONEGOTIATION | BMCR_FULL_DUPLEX | BMCR_100MBPS);
				PHY_WRITE_2( sc, DP83840_BMCR, media);
				PHY_READ_2( sc, DP83840_BMSR );
				i = PHY_READ_2( sc, DP83840_BMSR );

				if (i & BMSR_LINK_STATUS) {
					/*port is linked to the non
					 * Auto-Negotiation10Mbs partner
			 	 	 */
					return EPIC_HALF_DUPLEX;
				}
			}
		}
		/* If we get here we are most likely not connected
		 * so lets default it to half duplex
		 */
		return EPIC_HALF_DUPLEX;
	}
	
}

/*
 * Synopsis: This function should update multicast hash table.
 * I suppose there is a bug in chips MC filter so this function
 * only set it to receive all MC packets. The second problem is
 * that we should wait for TX and RX processes to stop before
 * reprogramming MC filter. The epic_stop_activity() and 
 * epic_start_activity() should help to do this.
 */
static void
epic_set_mc_table (
    epic_softc_t * sc)
{
	struct ifnet *ifp = &sc->epic_if;

	if( ifp->if_flags & IFF_MULTICAST ){
		CSR_WRITE_4( sc, MC0, 0xFFFF );
		CSR_WRITE_4( sc, MC1, 0xFFFF );
		CSR_WRITE_4( sc, MC2, 0xFFFF );
		CSR_WRITE_4( sc, MC3, 0xFFFF );
	}

	return;
}

static void
epic_shutdown(
    int howto,
    void *sc)
{
	epic_stop(sc);
}

/* 
 * Synopsis: Start receive process, should check that all internal chip 
 * pointers are set properly.
 */
static void
epic_start_activity __P((
    epic_softc_t * sc))
{
	/* Start rx process */
	CSR_WRITE_4( sc, COMMAND, COMMAND_RXQUEUED | COMMAND_START_RX );
}

/*
 * Synopsis: Completely stop Rx and Tx processes. If TQE is set additional
 * packet needs to be queued to stop Tx DMA.
 */
static void
epic_stop_activity __P((
    epic_softc_t * sc))
{
	int i;

	/* Stop Tx and Rx DMA */
	CSR_WRITE_4( sc, COMMAND, COMMAND_STOP_RX | COMMAND_STOP_RDMA | COMMAND_STOP_TDMA);

	/* Wait only Rx DMA */
	dprintf(("\ntx%d: waiting Rx DMA to stop",sc->unit));
	for(i=0;i<0x100000;i++)
		if( (CSR_READ_4(sc,INTSTAT)&INTSTAT_RXIDLE) == INTSTAT_RXIDLE ) break;

	if( !(CSR_READ_4(sc,INTSTAT)&INTSTAT_RXIDLE) ) 
		printf("\ntx%d: can't stop RX DMA",sc->unit);

	/* May need to queue one more packet if TQE */
	if( (CSR_READ_4( sc, INTSTAT ) & INTSTAT_TQE) &&
	    !(CSR_READ_4( sc, INTSTAT ) & INTSTAT_TXIDLE) ){
		dprintf(("\ntx%d: queue last packet",sc->unit));

		/* Turn it to loopback mode */	
		CSR_WRITE_4( sc, TXCON, TXCON_DEFAULT|TXCON_LOOPBACK_MODE_INT );

		sc->tx_desc[sc->cur_tx].bufaddr = vtophys( sc );
		sc->tx_desc[sc->cur_tx].buflength = ETHER_MIN_LEN-ETHER_CRC_LEN;
		sc->tx_desc[sc->cur_tx].control = 0x14;
		sc->tx_desc[sc->cur_tx].txlength = ETHER_MIN_LEN-ETHER_CRC_LEN;
		sc->tx_desc[sc->cur_tx].status = 0x8000;

		CSR_WRITE_4( sc, COMMAND, COMMAND_TXQUEUED );

		dprintf(("\ntx%d: waiting Tx DMA to stop",sc->unit));
		/* Wait TX DMA to stop */
		for(i=0;i<0x100000;i++)
			if( (CSR_READ_4(sc,INTSTAT)&INTSTAT_TXIDLE) == INTSTAT_TXIDLE ) break;

		if( !(CSR_READ_4(sc,INTSTAT)&INTSTAT_TXIDLE) )
			printf("\ntx%d: can't stop TX DMA",sc->unit);
	}
}

/*
 *  Synopsis: Shut down board and deallocates rings.
 *
 *  splimp() invoked here
 */
static void
epic_stop __P((
    epic_softc_t * sc))
{
	int i,s;

	s = splimp();

	sc->epic_if.if_timer = 0;

	/* Disable interrupts */
	CSR_WRITE_4( sc, INTMASK, 0 );
	CSR_WRITE_4( sc, GENCTL, 0 );

	/* Try to stop Rx and TX processes */
	epic_stop_activity(sc);

	/* Reset chip */
	CSR_WRITE_4( sc, GENCTL, GENCTL_SOFT_RESET );
	DELAY(1);

	/* Free memory allocated for rings */
	epic_free_rings(sc);

	/* Mark as stoped */
	sc->epic_if.if_flags &= ~IFF_RUNNING;

	splx(s);
	return;
}

/*
 * Synopsis: This function should free all memory allocated for rings.
 */ 
static void
epic_free_rings __P((
    epic_softc_t * sc))
{
	int i;

	for(i=0;i<RX_RING_SIZE;i++){
		struct epic_rx_buffer *buf = sc->rx_buffer + i;
		struct epic_rx_desc *desc = sc->rx_desc + i;
		
		desc->status = 0;
		desc->buflength = 0;
		desc->bufaddr = 0;

		if( buf->mbuf ) m_freem( buf->mbuf );
		buf->mbuf = NULL;
	}

	for(i=0;i<TX_RING_SIZE;i++){
		struct epic_tx_buffer *buf = sc->tx_buffer + i;
		struct epic_tx_desc *desc = sc->tx_desc + i;

		desc->status = 0;
		desc->buflength = 0;
		desc->bufaddr = 0;

		if( buf->mbuf ) m_freem( buf->mbuf );
		buf->mbuf = NULL;
	}
}

/*
 * Synopsis:  Allocates mbufs for Rx ring and point Rx descs to them.
 * Point Tx descs to fragment lists. Check that all descs and fraglists
 * are bounded and aligned properly.
 */
static int
epic_init_rings(epic_softc_t * sc){
	int i;
	struct mbuf *m;

	sc->cur_rx = sc->cur_tx = sc->dirty_tx = sc->pending_txs = 0;

	for (i = 0; i < RX_RING_SIZE; i++) {
		struct epic_rx_buffer *buf = sc->rx_buffer + i;
		struct epic_rx_desc *desc = sc->rx_desc + i;

		desc->status = 0;		/* Owned by driver */
		desc->next = vtophys( sc->rx_desc + ((i+1)%RX_RING_SIZE) );

		if( (desc->next & 3) || ((desc->next & 0xFFF) + sizeof(struct epic_rx_desc) > 0x1000 ) )
			printf("\ntx%d: WARNING! rx_desc is misbound or misaligned",sc->unit);

		EPIC_MGETCLUSTER( buf->mbuf );
		if( NULL == buf->mbuf ) {
			epic_free_rings(sc);
			return -1;
		}
		desc->bufaddr = vtophys( mtod(buf->mbuf,caddr_t) );

		desc->buflength = ETHER_MAX_FRAME_LEN;
		desc->status = 0x8000;			/* Give to EPIC */

	}

	for (i = 0; i < TX_RING_SIZE; i++) {
		struct epic_tx_buffer *buf = sc->tx_buffer + i;
		struct epic_tx_desc *desc = sc->tx_desc + i;

		desc->status = 0;
		desc->next = vtophys( sc->tx_desc + ( (i+1)%TX_RING_SIZE ) );

		if( (desc->next & 3) || ((desc->next & 0xFFF) + sizeof(struct epic_tx_desc) > 0x1000 ) )
			printf("\ntx%d: WARNING! tx_desc is misbound or misaligned",sc->unit);

		buf->mbuf = NULL;
		desc->bufaddr = vtophys( sc->tx_flist + i );
		if( (desc->bufaddr & 3) || ((desc->bufaddr & 0xFFF) + sizeof(struct epic_frag_list) > 0x1000 ) )
			printf("\ntx%d: WARNING! frag_list is misbound or misaligned",sc->unit);
	}

	return 0;
}

/*
 * EEPROM operation functions
 */
static void epic_write_eepromreg __P((
    epic_softc_t *sc,
    u_int8_t val))
{
	u_int16_t i;

	CSR_WRITE_1( sc, EECTL, val );

	for( i=0;i<0xFF; i++)
		if( !(CSR_READ_1( sc, EECTL ) & 0x20) ) break;

	return;
}

static u_int8_t
epic_read_eepromreg __P((
    epic_softc_t *sc))
{
	return CSR_READ_1( sc,EECTL );
}  

static u_int8_t
epic_eeprom_clock __P((
    epic_softc_t *sc,
    u_int8_t val))
{
	epic_write_eepromreg( sc, val );
	epic_write_eepromreg( sc, (val | 0x4) );
	epic_write_eepromreg( sc, val );
	
	return epic_read_eepromreg( sc );
}

static void
epic_output_eepromw __P((
    epic_softc_t * sc,
    u_int16_t val))
{
	int i;          
	for( i = 0xF; i >= 0; i--){
		if( (val & (1 << i)) ) epic_eeprom_clock( sc, 0x0B );
		else epic_eeprom_clock( sc, 3);
	}
}

static u_int16_t
epic_input_eepromw __P((
    epic_softc_t *sc))
{
	int i;
	int tmp;
	u_int16_t retval = 0;

	for( i = 0xF; i >= 0; i--) {	
		tmp = epic_eeprom_clock( sc, 0x3 );
		if( tmp & 0x10 ){
			retval |= (1 << i);
		}
	}
	return retval;
}

static int
epic_read_eeprom __P((
    epic_softc_t *sc,
    u_int16_t loc))
{
	int i;
	u_int16_t dataval;
	u_int16_t read_cmd;

	epic_write_eepromreg( sc , 3);

	if( epic_read_eepromreg( sc ) & 0x40 )
		read_cmd = ( loc & 0x3F ) | 0x180;
	else
		read_cmd = ( loc & 0xFF ) | 0x600;

	epic_output_eepromw( sc, read_cmd );

        dataval = epic_input_eepromw( sc );

	epic_write_eepromreg( sc, 1 );
	
	return dataval;
}

static u_int16_t
epic_read_phy_register __P((
    epic_softc_t *sc,
    u_int16_t loc))
{
	int i;

	CSR_WRITE_4( sc, MIICTL, ((loc << 4) | 0x0601) );

	for( i=0;i<0x1000;i++) if( !(CSR_READ_4( sc, MIICTL )&1) ) break;

	return CSR_READ_4( sc, MIIDATA );
}

static void
epic_write_phy_register __P((
    epic_softc_t * sc,
    u_int16_t loc,
    u_int16_t val))
{
	int i;

	CSR_WRITE_4( sc, MIIDATA, val );
	CSR_WRITE_4( sc, MIICTL, ((loc << 4) | 0x0602) );

	for( i=0;i<0x1000;i++) if( !(CSR_READ_4( sc, MIICTL )&2) ) break;

	return;
}

static void
epic_dump_state __P((
    epic_softc_t * sc))
{
	int j;
	struct epic_tx_desc *tdesc;
	struct epic_rx_desc *rdesc;
	printf("\ntx%d: cur_rx: %d, pending_txs: %d, dirty_tx: %d, cur_tx: %d",
		sc->unit,sc->cur_rx,sc->pending_txs,sc->dirty_tx,sc->cur_tx);
	printf("\ntx%d: COMMAND: 0x%08x, INTSTAT: 0x%08x",sc->unit,CSR_READ_4(sc,COMMAND),CSR_READ_4(sc,INTSTAT));
	printf("\ntx%d: PRCDAR: 0x%08x, PTCDAR: 0x%08x",sc->unit,CSR_READ_4(sc,PRCDAR),CSR_READ_4(sc,PTCDAR));
	printf("\ntx%d: dumping rx descriptors",sc->unit);
	for(j=0;j<RX_RING_SIZE;j++){
		rdesc = sc->rx_desc + j;
		printf("\ndesc%d: %4d 0x%04x, 0x%08x, %4d, 0x%08x",
			j,
			rdesc->rxlength,rdesc->status,
			rdesc->bufaddr,
			rdesc->buflength,
			rdesc->next
		);
	}
	printf("\ntx%d: dumping tx descriptors",sc->unit);
	for(j=0;j<TX_RING_SIZE;j++){
		tdesc = sc->tx_desc + j;
		printf("\ndesc%d: %4d 0x%04x, 0x%08x, 0x%04x %4d, 0x%08x, mbuf: 0x%08x",
			j,
			tdesc->txlength,tdesc->status,
			tdesc->bufaddr,
			tdesc->control,tdesc->buflength,
			tdesc->next,
			sc->tx_buffer[j].mbuf
		);
	}
}
#endif /* NPCI > 0 */
