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
 * version: stable-166
 *
 */

/*
 * EtherPower II 10/100  Fast Ethernet (tx0)
 * (aka SMC9432TX based on SMC83c170 EPIC chip)
 *
 * Written by Semen Ustimenko.
 *
 * TODO:
 *	Fix TX_FRAG_LIST option
 *	Rewrite autonegotiation to remove DELAY(300000)
 *	
 * stable-140:
 *	first stable version
 *
 * stable-160:
 *	added BPF support
 *	fixed several bugs
 *
 * stable-161:
 *	fixed BPF support
 *	fixed several bugs
 *
 * stable-162:
 *	fixed IFF_PROMISC mode support
 *	added speed info displayed at startup (MII info)
 *
 * stable-163:
 *	added media control code
 *
 * stable-164:
 *	fixed some bugs
 *
 * stable-165:
 *	fixed media control code
 *
 * stable-166:
 *	fixed RX_TO_MBUF option and set as default
 *	fixed bug caused ``tx0: device timeout 1 packets'' in 100Mbps mode
 *	implemented fragment list transmit method (TX_FRAG_LIST) (BUGGY)
 *	applyed patch to autoneg fullduplex modes ( Thank to Steve Bauer )
 *	added more coments, removed some debug printfs
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <sys/sockio.h>
#else
#include <sys/ioctl.h>
#endif
#include <machine/clock.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_mib.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/netisr.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>
#include <machine/clock.h>

#include "pci.h"
#if NPCI > 0
#include <pci/pcivar.h>
#include <pci/smc83c170.h>
#endif

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

/*
 * Global variables
 */
static u_long epic_pci_count;
static epic_softc_t * epics[EPIC_MAX_DEVICES];
struct pci_device txdevice = { 
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
epic_ifioctl(register struct ifnet * ifp, int command, caddr_t data){
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
				ifp->if_flags &= ~IFF_RUNNING;
				break;
			}
		}

		/* Handle IFF_PROMISC flag */
		epic_set_rx_mode(sc);

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

	while( sc->pending_txs < TX_RING_SIZE  ){
		int entry = sc->cur_tx % TX_RING_SIZE;
		struct epic_tx_buffer * buf = sc->tx_buffer + entry;
		struct mbuf *m,*m0;
		int len;

		/* If descriptor is busy, set IFF_OACTIVE and exit */
		if( buf->desc.status & 0x8000 ) break;

		/* Get next packet to send */
		IF_DEQUEUE( &(sc->epic_if.if_snd), m );

		/* If no more mbuf's to send, return */
		if( NULL == m ) return;
		/* Save mbuf header */
		m0 = m;

#if defined(TX_FRAG_LIST)
		if( buf->mbuf ) m_freem( buf->mbuf );
		buf->mbuf = m;
		buf->flist.numfrags = 0;

		for(len=0;(m0!=0)&&(buf->flist.numfrags<63);m0=m0->m_next) {
			buf->flist.frag[buf->flist.numfrags].fraglen = 
				m0->m_len; 
			buf->flist.frag[buf->flist.numfrags].fragaddr = 
				vtophys( mtod(m0, caddr_t) );
			len += m0->m_len;
			buf->flist.numfrags++;
		}

		/* Does not generate TXC unless ring is full more then a half */
		buf->desc.control =
			(sc->pending_txs>TX_RING_SIZE/2)?0x05:0x01;
#else
		for (len = 0; m0 != 0; m0 = m0->m_next) {
			bcopy(mtod(m0, caddr_t), buf->data + len, m0->m_len);
			len += m0->m_len;
		}

		/* Does not generate TXC unless ring is full more then a half */
		buf->desc.control =
			(sc->pending_txs>TX_RING_SIZE/2)?0x14:0x10;
#endif

		/* Packet should be at least ETHER_MIN_LEN */
		buf->desc.txlength = max(len,ETHER_MIN_LEN-ETHER_CRC_LEN);

		/* Pass ownership to the chip */
		buf->desc.status = 0x8000;

		/* Set watchdog timer */
		ifp->if_timer = 2;

#if NBPFILTER > 0
		if( ifp->if_bpf ) bpf_mtap( ifp, m );
#endif

#if !defined(TX_FRAG_LIST)
		/* We don't need mbuf anyway */
		m_freem( m );
#endif
		/* Trigger an immediate transmit demand. */
		outl( sc->iobase + COMMAND, COMMAND_TXQUEUED );

		/* Packet queued successful */
		sc->pending_txs++;

		/* Switch to next descriptor */
		sc->cur_tx = ( sc->cur_tx + 1 ) % TX_RING_SIZE;
	}

	sc->epic_if.if_flags |= IFF_OACTIVE;

	return;
	
}

/*
 *  IFWATCHDOG function
 *
 * splimp() invoked here
 */
static void
epic_ifwatchdog(
    struct ifnet *ifp)
{
	epic_softc_t *sc = ifp->if_softc;
	int x;
	int i;

	x = splimp();

	printf("tx%d: device timeout %d packets\n",
			sc->unit,sc->pending_txs);

	ifp->if_oerrors+=sc->pending_txs;

	epic_stop(sc);
	epic_init(sc);

	epic_ifstart(&sc->epic_if);

	splx(x);
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
	int iobase = sc->iobase;
        int status;

	status = inl(iobase + INTSTAT);

	if( status & (INTSTAT_RQE|INTSTAT_HCC|INTSTAT_RCC) ) {
		epic_rx_done( sc );
		outl( iobase + INTSTAT,
			status & (INTSTAT_RQE|INTSTAT_HCC|INTSTAT_RCC) ); 
	}

	if( status & (INTSTAT_TXC|INTSTAT_TCC) ) {
		epic_tx_done( sc );
		outl( iobase + INTSTAT,
			status & (INTSTAT_TXC|INTSTAT_TCC) );
	}

	if( (status & INTSTAT_TQE) && !(sc->epic_if.if_flags & IFF_OACTIVE) ) {
		epic_ifstart( &sc->epic_if );
		outl( iobase + INTSTAT, INTSTAT_TQE );
	}

#if 0
	if( status & INTSTAT_GP2 ){
		printf("tx%d: GP2 int occured\n",sc->unit);
		epic_read_phy_register(sc->iobase,DP83840_BMSR);
		epic_read_phy_register(sc->iobase,DP83840_BMCR);
		outl( iobase + INTSTAT, INTSTAT_GP2 );
	}
#endif

	if( status & (INTSTAT_FATAL|INTSTAT_PMA|INTSTAT_PTA|INTSTAT_APE|INTSTAT_DPE) ){
		int j;
		struct epic_tx_buffer * buf;

		printf("tx%d: PCI fatal error occured (%s%s%s%s)\n",
			sc->unit,
			(status&INTSTAT_PMA)?"PMA":"",
			(status&INTSTAT_PTA)?" PTA":"",
			(status&INTSTAT_APE)?" APE":"",
			(status&INTSTAT_DPE)?" DPE":"");

#if defined(EPIC_DEBUG)
		printf("tx%d: dumping descriptors\n",sc->unit);
		for(j=0;j<TX_RING_SIZE;j++){
			buf = sc->tx_buffer + j;
			printf("desc%d: %d %04x, %08x, %04x %d, %08x\n",
				j,
				buf->desc.txlength,buf->desc.status,
				buf->desc.bufaddr,
				buf->desc.control,buf->desc.buflength,
				buf->desc.next
			);
		}
#endif
		epic_stop(sc);
		epic_init(sc);
		
		return;
	}

        /* UPDATE statistics */
	if (status & (INTSTAT_CNT | INTSTAT_TXU | INTSTAT_OVW | INTSTAT_RXE)) {

		/* update dot3 Rx statistics */
		sc->dot3stats.dot3StatsMissedFrames += inb(iobase + MPCNT);
		sc->dot3stats.dot3StatsFrameTooLongs += inb(iobase + ALICNT);
		sc->dot3stats.dot3StatsFCSErrors += inb(iobase + CRCCNT);

		/* Update if Rx statistics */
		if (status & (INTSTAT_OVW | INTSTAT_RXE))
			sc->epic_if.if_ierrors++;

		/* Tx FIFO underflow. */
		if (status & INTSTAT_TXU) {
			/* Inc. counters */
			sc->dot3stats.dot3StatsInternalMacTransmitErrors++;
			sc->epic_if.if_oerrors++;

			/* Restart the transmit process. */
			outl(iobase + COMMAND, COMMAND_TXUGO);
		}

		/* Clear all error sources. */
		outl(iobase + INTSTAT,
		      status&(INTSTAT_CNT|INTSTAT_TXU|INTSTAT_OVW|INTSTAT_RXE));

	}

	/* If no packets are pending, thus no timeouts */
	if( sc->pending_txs == 0 )
		sc->epic_if.if_timer = 0;

	return;
}

/*
 *
 * splimp() invoked before epic_intr_normal()
 */
void
epic_rx_done(
	epic_softc_t *sc )
{
        int i = 0;
	u_int16_t len;
	struct epic_rx_buffer *	buf;
	struct mbuf *m;
#if defined(RX_TO_MBUF)
	struct mbuf *m0;
#endif
	struct ether_header *eh;
	int stt;


	while( !(sc->rx_buffer[sc->cur_rx].desc.status & 0x8000) && \
		i++ < RX_RING_SIZE ){

		buf = sc->rx_buffer + sc->cur_rx;

		stt = buf->desc.status;

		/* Check for errors */
		if( !(buf->desc.status&1) ) {
			sc->epic_if.if_ierrors++;
			goto rxerror;
		}

		/* This is received frame actual length */ 
		len = buf->desc.rxlength - ETHER_CRC_LEN;

#if !defined(RX_TO_MBUF)
		/* Allocate mbuf to pass to OS */
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if( NULL == m ){
			printf("tx%d: cannot allocate mbuf header\n",sc->unit);
			sc->epic_if.if_ierrors++;
			goto rxerror;
		}
		if( len > MHLEN ){
			MCLGET(m,M_DONTWAIT);
			if( NULL == (m->m_flags & M_EXT) ){
				printf("tx%d: cannot allocate mbuf cluster\n",
					sc->unit);
				m_freem( m );
				sc->epic_if.if_ierrors++;
				goto rxerror;
			}
		}	

		/* Copy packet to new allocated mbuf */
		memcpy( mtod(m,void*), buf->data, len );

#else /* RX_TO_MBUF */

		/* Try to allocate mbuf cluster */
		MGETHDR(m0,M_DONTWAIT,MT_DATA);
		if( NULL == m0 ) {
			printf("tx%d: cannot allocate mbuf header/n",sc->unit);
			sc->epic_if.if_ierrors++;
			goto rxerror;
		}
		MCLGET(m0,M_DONTWAIT);
		if( NULL == (m0->m_flags & M_EXT) ){
			printf("tx%d: cannot allocate mbuf cluster/n",sc->unit);
			m_freem(m0);
			sc->epic_if.if_ierrors++;
			goto rxerror;
		}

		/* Swap new allocated mbuf with mbuf, containing packet */
		m = buf->mbuf;
		buf->mbuf = m0;

		/* Insert new allocated mbuf into device queue */
		buf->data = mtod( buf->mbuf, caddr_t );
		buf->desc.bufaddr = vtophys( buf->data );

#endif
		
		/* First mbuf in packet holds the ethernet and packet headers */
		eh = mtod( m, struct ether_header * );
		m->m_pkthdr.rcvif = &(sc->epic_if);
		m->m_pkthdr.len = len;
		m->m_len = len;

#if NBPFILTER > 0
		/* Give mbuf to BPFILTER */
		if( sc->epic_if.if_bpf ) bpf_mtap( &sc->epic_if, m );

		/* Accept only our packets, broadcasts and multicasts */
		if( (eh->ether_dhost[0] & 1) == 0 &&
		    bcmp(eh->ether_dhost,sc->epic_ac.ac_enaddr,ETHER_ADDR_LEN)){
			m_freem(m);
			goto rxerror;
		}
#endif

		/* Second mbuf holds packet ifself */
		m->m_pkthdr.len = len - sizeof(struct ether_header);
		m->m_len = len - sizeof( struct ether_header );
		m->m_data += sizeof( struct ether_header );

		/* Give mbuf to OS */
		ether_input(&sc->epic_if, eh, m);

		/* Successfuly received frame */
		sc->epic_if.if_ipackets++;

rxerror:
		/* Mark current descriptor as free */
		buf->desc.rxlength = 0;
		buf->desc.status = 0x8000;

		/* Switch to next descriptor */
		sc->cur_rx = (sc->cur_rx+1) % RX_RING_SIZE;
        }

	return;
}

/*
 *
 * splimp() invoked before epic_intr_normal()
 */
void
epic_tx_done( epic_softc_t *sc ){
	int i = 0;
	u_int32_t if_flags=~0;
	int coll;
	u_int16_t stt;

	while( i++ < TX_RING_SIZE ){
		struct epic_tx_buffer *buf = sc->tx_buffer + sc->dirty_tx;
		u_int16_t len = buf->desc.txlength;
		stt =  buf->desc.status;

		if( stt & 0x8000 )
			break;	/* following packets are not Txed yet */

		if( stt == 0 ){
			if_flags = ~IFF_OACTIVE;
			break;
		}

		sc->pending_txs--;		/* packet is finished */
		sc->dirty_tx = (sc->dirty_tx + 1) % TX_RING_SIZE;

		coll = (stt >> 8) & 0xF;	/* number of collisions*/

		if( stt & 0x0001 ){
			sc->epic_if.if_opackets++;
		} else {
			if(stt & 0x0008)
				sc->dot3stats.dot3StatsCarrierSenseErrors++;

			if(stt & 0x1050)
				sc->dot3stats.dot3StatsInternalMacTransmitErrors++;

			if(stt & 0x1000) coll = 16;

			sc->epic_if.if_oerrors++;
		} 

		if(stt & 0x0002)	/* What does it mean? */
			sc->dot3stats.dot3StatsDeferredTransmissions++;

		sc->epic_if.if_collisions += coll;

		switch( coll ){
		case 0:
			break;
		case 16:
			sc->dot3stats.dot3StatsExcessiveCollisions++;
			sc->dot3stats.dot3StatsCollFrequencies[15]++;
			break;
		case 1:
			sc->dot3stats.dot3StatsSingleCollisionFrames++;
			sc->dot3stats.dot3StatsCollFrequencies[0]++;
			break;
		default:
			sc->dot3stats.dot3StatsMultipleCollisionFrames++;
			sc->dot3stats.dot3StatsCollFrequencies[coll-1]++;
			break;
		}

		buf->desc.status = 0;
		buf->desc.txlength = 0;

#if defined(TX_FRAG_LIST)
		buf->flist.numfrags = 0;
		m_freem( buf->mbuf );
		buf->mbuf = NULL;
#endif

		if_flags = ~IFF_OACTIVE;
	}

	sc->epic_if.if_flags &= if_flags;

	if( !(sc->epic_if.if_flags & IFF_OACTIVE) )
		epic_ifstart( &sc->epic_if );

}

/*
 * Probe function
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
 * PCI_Attach function
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
	u_int32_t iobase;
	u_int32_t irq;
	u_int32_t phyid;
	int i,s;
	int phy, phy_idx;

	/*
	 * Get iobase and irq level
	 */
	irq = PCI_CONF_READ(PCI_CFIT) & (0xFF);
	if (!pci_map_port(config_id, PCI_CBIO,(u_short *) &iobase))
		return;

	/*
	 * Allocate and preinitialize softc structure
	 */
	sc = (epic_softc_t *) malloc(sizeof(epic_softc_t), M_DEVBUF, M_NOWAIT);
	if (sc == NULL)	return;
	epics[ unit ] = sc;

	/*
	 * Zero softc structure
	 */
    	bzero(sc, sizeof(epic_softc_t));		

	/*
	 * Initialize softc
	 */
	sc->unit = unit;
	sc->iobase = iobase;
	sc->irq = irq;

	/* Bring the chip out of low-power mode. */
	outl( iobase + GENCTL, 0x0000 );

	/* Magic?!  If we don't set this bit the MII interface won't work. */
	outl( iobase + TEST1, 0x0008 );

	/* Read mac address (may be better is read from EEPROM?) */
	for (i = 0; i < ETHER_ADDR_LEN / sizeof( u_int16_t); i++)
		((u_int16_t *)sc->epic_macaddr)[i] = inw(iobase + LAN0 + i*4);

	/* Display some info */
	printf("tx%d: address %02x:%02x:%02x:%02x:%02x:%02x,",sc->unit,
		sc->epic_macaddr[0],sc->epic_macaddr[1],sc->epic_macaddr[2],
		sc->epic_macaddr[3],sc->epic_macaddr[4],sc->epic_macaddr[5]);


	s = splimp();

	/* Map interrupt */
	if( !pci_map_int(config_id, epic_intr_normal, (void*)sc, &net_imask) ) {
		printf("tx%d: couldn't map interrupt\n",unit);
		epics[ unit ] = NULL;
		free(sc, M_DEVBUF);
		return;
	}

	/* Fill ifnet structure */
	ifp = &sc->epic_if;

	ifp->if_unit = unit;
	ifp->if_name = "tx";
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST|IFF_SIMPLEX|IFF_MULTICAST|IFF_ALLMULTI;
	ifp->if_ioctl = epic_ifioctl;
	ifp->if_start = epic_ifstart;
	ifp->if_watchdog = epic_ifwatchdog;
	ifp->if_init = (if_init_f_t*)epic_init;
	ifp->if_timer = 0;
	ifp->if_output = ether_output;
	ifp->if_linkmib = &sc->dot3stats;
	ifp->if_linkmiblen = sizeof(struct ifmib_iso_8802_3);

	sc->dot3stats.dot3StatsEtherChipSet =
				DOT3CHIPSET(dot3VendorSMC,
					    dot3ChipSetSMC83c170);

	sc->dot3stats.dot3Compliance = DOT3COMPLIANCE_COLLS;

	printf(" type SMC9432TX");
	
	i = epic_read_phy_register(iobase, DP83840_BMCR);

	if( i & BMCR_AUTONEGOTIATION ){
		printf(" [Auto-Neg.");

		if( i & BMCR_100MBPS ) printf(" 100Mbps");
		else printf(" 10Mbps");

		if( i & BMCR_FULL_DUPLEX ) printf(" FD");

		printf("]\n");

		if( i & BMCR_FULL_DUPLEX )
			 printf("tx%d: WARNING! FD autonegotiated, not supported\n",sc->unit);

	} else {
		ifp->if_flags |= IFF_LINK0;
		if( i & BMCR_100MBPS ) {
			printf(" [100Mbps");
			ifp->if_flags |= IFF_LINK2;
		} else printf(" [10Mbps");

		if( i & BMCR_FULL_DUPLEX ) {
			printf(" FD");
			ifp->if_flags |= IFF_LINK1;
		}
		printf("]\n");
	}
#if defined(EPIC_DEBUG)
	printf("tx%d: PHY id: (",sc->unit);
	i=epic_read_phy_register(iobase,DP83840_PHYIDR1);
	printf("%04x:",i);
	phyid=i<<6;
	i=epic_read_phy_register(iobase,DP83840_PHYIDR2);
	printf("%04x)",i);
	phyid|=((i>>10)&0x3F);
	printf(" %08x, rev %x, mod %x\n",phyid,(i)&0xF, (i>>4)&0x3f);
#endif

	epic_read_phy_register(iobase,DP83840_BMSR);
	epic_read_phy_register(iobase,DP83840_BMSR);
	epic_read_phy_register(iobase,DP83840_BMSR);
	i=epic_read_phy_register(iobase,DP83840_BMSR);

	if( !(i & BMSR_LINK_STATUS) )
		printf("tx%d: WARNING! no link estabilished\n",sc->unit);

	/*
	 *  Attach to if manager
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPFILTER > 0
	bpfattach(ifp,DLT_EN10MB, sizeof(struct ether_header));
#endif

	splx(s);

	return;
}

/*
 * IFINIT function
 * 
 * splimp() invoked here
 */
static int 
epic_init(
    epic_softc_t * sc)
{       
	struct ifnet *ifp = &sc->epic_if;
	int iobase = sc->iobase;
	int i,s;
 
	s = splimp();

	/* Soft reset the chip. */
	outl(iobase + GENCTL, GENCTL_SOFT_RESET );

	/* Reset takes 15 ticks */
	for(i=0;i<0x100;i++);

	/* Wake up */
	outl( iobase + GENCTL, 0 );

	/* ?????? */
	outl( iobase + TEST1, 0x0008);

	/* Initialize rings */
	if( -1 == epic_init_rings( sc ) ) {
		printf("tx%d: failed to initialize rings\n",sc->unit);
		epic_free_rings( sc );
		splx(s);
		return -1;
	}	

	/* Put node address to EPIC */
	outl( iobase + LAN0 + 0x0, ((u_int16_t *)sc->epic_macaddr)[0] );
        outl( iobase + LAN0 + 0x4, ((u_int16_t *)sc->epic_macaddr)[1] );
	outl( iobase + LAN0 + 0x8, ((u_int16_t *)sc->epic_macaddr)[2] );

	/* Enable interrupts,  set for PCI read multiple and etc */
	outl( iobase + GENCTL,
		GENCTL_ENABLE_INTERRUPT | GENCTL_MEMORY_READ_MULTIPLE |
		GENCTL_ONECOPY | GENCTL_RECEIVE_FIFO_THRESHOLD64 );

	/* Set transmit threshold */
	outl( iobase + ETXTHR, 0x40 );

	/* Compute and set RXCON. */
	epic_set_rx_mode( sc );

	/* Set MII speed mode */
	epic_set_media_speed( sc );

	/* Set multicast table */
	epic_set_mc_table( sc );

	/* Enable interrupts by setting the interrupt mask. */
	outl( iobase + INTMASK,
		INTSTAT_RCC | INTSTAT_RQE | INTSTAT_OVW | INTSTAT_RXE |
		INTSTAT_TXC | INTSTAT_TCC | INTSTAT_TQE | INTSTAT_TXU |
		INTSTAT_CNT | /*INTSTAT_GP2 |*/ INTSTAT_FATAL |
		INTSTAT_PTA | INTSTAT_PMA | INTSTAT_APE | INTSTAT_DPE );

	/* Start rx process */
	outl( iobase + COMMAND, COMMAND_RXQUEUED | COMMAND_START_RX );

	/* Mark interface running ... */
	if( ifp->if_flags & IFF_UP ) ifp->if_flags |= IFF_RUNNING;
	else ifp->if_flags &= ~IFF_RUNNING;

	/* ... and free */
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);
	return 0;
}

/*
 * This function should set EPIC's registers according IFF_* flags
 */
static void
epic_set_rx_mode(
    epic_softc_t * sc)
{
	struct ifnet *ifp = &sc->epic_if;
        u_int16_t rxcon = 0;

#if NBPFILTER > 0
	if( sc->epic_if.if_flags & IFF_PROMISC )
		rxcon |= RXCON_PROMISCUOUS_MODE;
#endif

	if( sc->epic_if.if_flags & IFF_BROADCAST )
		rxcon |= RXCON_RECEIVE_BROADCAST_FRAMES;

	if( sc->epic_if.if_flags & IFF_MULTICAST )
		rxcon |= RXCON_RECEIVE_MULTICAST_FRAMES;

	outl( sc->iobase + RXCON, rxcon );

	return;
}

/*
 * This function should set MII to mode specified by IFF_LINK* flags
 */
static void
epic_set_media_speed(
    epic_softc_t * sc)
{
	struct ifnet *ifp = &sc->epic_if;
	u_int16_t media;
	u_int32_t i;

	/* Set media speed */           
	if( ifp->if_flags & IFF_LINK0 ){
		/* Allow only manual fullduplex modes */
		media = epic_read_phy_register( sc->iobase, DP83840_ANAR );
		media |= ANAR_100|ANAR_10|ANAR_100_FD|ANAR_10_FD;
		epic_write_phy_register( sc->iobase, DP83840_ANAR, media );

		/* Set mode */
		media = (ifp->if_flags&IFF_LINK2)?BMCR_100MBPS:0;
		media |= (ifp->if_flags&IFF_LINK1)?BMCR_FULL_DUPLEX:0;
		epic_write_phy_register( sc->iobase, DP83840_BMCR, media );

		ifp->if_baudrate =
			(ifp->if_flags&IFF_LINK2)?100000000:10000000;

		outl( sc->iobase + TXCON,(ifp->if_flags&IFF_LINK1)?TXCON_LOOPBACK_MODE_FULL_DUPLEX|TXCON_DEFAULT:TXCON_DEFAULT );

	} else {
		/* If autoneg is set, IFF_LINK flags are meaningless */
		ifp->if_flags &= ~(IFF_LINK0|IFF_LINK1|IFF_LINK2);
		ifp->if_baudrate = 100000000;

		outl( sc->iobase + TXCON, TXCON_DEFAULT );

		/* Did it autoneg full duplex? */
		if (epic_autoneg(sc) == EPIC_FULL_DUPLEX)
			outl( sc->iobase + TXCON,
				TXCON_LOOPBACK_MODE_FULL_DUPLEX|TXCON_DEFAULT);
	}

	return;
}

/*
 * This functions controls the autoneg processes of the phy
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

	media = epic_read_phy_register( sc->iobase, DP83840_ANAR );
	media |= ANAR_100|ANAR_100_FD|ANAR_10|ANAR_10_FD;
	epic_write_phy_register( sc->iobase, DP83840_ANAR, media );

	/* Set and restart autoneg */
	epic_write_phy_register( sc->iobase, DP83840_BMCR,
		BMCR_AUTONEGOTIATION | BMCR_RESTART_AUTONEG );

        /* Wait 3 seconds for the autoneg to finish
	 * This is the recommended time from the DP83840A data sheet
	 * Section 7.1
         */
        DELAY(3000000);

	epic_read_phy_register( sc->iobase, DP83840_BMSR);

        /* BMSR must be read twice to update the link status bit/
	 * since that bit is a latch bit
         */
	i = epic_read_phy_register( sc->iobase, DP83840_BMSR);
        
        if ((i & BMSR_LINK_STATUS) && ( i & BMSR_AUTONEG_COMPLETE)){
		i = epic_read_phy_register( sc->iobase, DP83840_PAR);

		if ( i & PAR_FULL_DUPLEX )
			return 	EPIC_FULL_DUPLEX;
		else
			return EPIC_HALF_DUPLEX;
        } 
	else {   /*Auto-negotiation or link status is not 1
		   Thus the auto-negotiation failed and one
		   must take other means to fix it.
		  */

		/* ANER must be read twice to get the correct reading for the 
		 * Multiple link fault bit -- it is a latched bit
	 	 */
 		epic_read_phy_register (sc->iobase, DP83840_ANER);

		i = epic_read_phy_register (sc->iobase, DP83840_ANER);
	
		if ( i & ANER_MULTIPLE_LINK_FAULT ) {
			/* it can be forced to 100Mb/s Half-Duplex */
	 		media = epic_read_phy_register(sc->iobase,DP83840_BMCR);
			media &= ~(BMCR_AUTONEGOTIATION | BMCR_FULL_DUPLEX);
			media |= BMCR_100MBPS;
			epic_write_phy_register(sc->iobase,DP83840_BMCR,media);
		
			/* read BMSR again to determine link status */
			epic_read_phy_register(sc->iobase, DP83840_BMSR);
			i=epic_read_phy_register( sc->iobase, DP83840_BMSR);
		
			if (i & BMSR_LINK_STATUS){
				/* port is linked to the non Auto-Negotiation
				 * 100Mbs partner.
			 	 */
				return EPIC_HALF_DUPLEX;
			}
			else {
				media = epic_read_phy_register (sc->iobase, DP83840_BMCR);
				media &= !(BMCR_AUTONEGOTIATION | BMCR_FULL_DUPLEX | BMCR_100MBPS);
				epic_write_phy_register(sc->iobase, DP83840_BMCR, media);
				epic_read_phy_register(sc->iobase, DP83840_BMSR);
				i=epic_read_phy_register( sc->iobase, DP83840_BMSR);

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
 * This function sets EPIC multicast table
 */
static void
epic_set_mc_table(
    epic_softc_t * sc)
{
	struct ifnet *ifp = &sc->epic_if;

	if( ifp->if_flags & IFF_MULTICAST ){
		outl( sc->iobase + MC0, 0xFFFF );
		outl( sc->iobase + MC1, 0xFFFF );
		outl( sc->iobase + MC2, 0xFFFF );
		outl( sc->iobase + MC3, 0xFFFF );
	}

	return;
}

/*
 *  This function should completely stop rx and tx processes
 *  
 *  splimp() invoked here
 */
static void
epic_stop(
    epic_softc_t * sc)
{
	int iobase = sc->iobase;
	int i,s;

	s = splimp();
	sc->epic_if.if_timer = 0;

	/* Disable interrupts, stop processes */
	outl( iobase + INTMASK, 0 );
	outl( iobase + GENCTL, 0 );
	outl( iobase + COMMAND,
		COMMAND_STOP_RX | COMMAND_STOP_RDMA | COMMAND_STOP_TDMA );

	/* Wait RX and TX DMA to stop */
	for(i=0;i<0x100000;i++){
		if( (inl(iobase+INTSTAT)&(INTSTAT_RXIDLE|INTSTAT_TXIDLE)) ==
			(INTSTAT_RXIDLE|INTSTAT_TXIDLE) ) break;
	}
	
	if( !(inl(iobase+INTSTAT)&INTSTAT_RXIDLE) )
		printf("tx%d: can't stop RX DMA\n",sc->unit);

	if( !(inl(iobase+INTSTAT)&INTSTAT_TXIDLE) )
		printf("tx%d: can't stop TX DMA\n",sc->unit);

	/* Reset chip */
	outl( iobase + GENCTL, GENCTL_SOFT_RESET );
	for(i=0;i<0x100;i++);

	/* Free memory allocated for rings */
	epic_free_rings( sc );

	splx(s);

}

/*
 * This function should free all allocated for rings memory.
 * NB: The DMA processes must be stopped.
 *
 * splimp() assumed to be done
 */ 
static void
epic_free_rings(epic_softc_t * sc){
	int i;

	for(i=0;i<RX_RING_SIZE;i++){
		struct epic_rx_buffer *buf = sc->rx_buffer + i;
		
		buf->desc.status = 0;
		buf->desc.buflength = 0;
		buf->desc.bufaddr = 0;
		buf->data = NULL;

#if defined(RX_TO_MBUF)
		if( buf->mbuf ) m_freem( buf->mbuf );
		buf->mbuf = NULL;
#else
		if( buf->data ) free( buf->data, M_DEVBUF );
		buf->data = NULL;
#endif
	}

	for(i=0;i<TX_RING_SIZE;i++){
		struct epic_tx_buffer *buf = sc->tx_buffer + i;

		buf->desc.status = 0;
		buf->desc.buflength = 0;
		buf->desc.bufaddr = 0;

#if defined(TX_FRAG_LIST)
		if( buf->mbuf ) m_freem( buf->mbuf );
		buf->mbuf = NULL;
#else
		if( buf->data ) free( buf->data, M_DEVBUF );
		buf->data = NULL;
#endif
	}
}

/*
 * Initialize Rx and Tx rings and give them to EPIC
 *
 * If RX_TO_MBUF option is enabled, mbuf cluster is allocated instead of
 * static buffer for RX ringi element.
 * If TX_FRAG_LIST option is enabled, nothig is done, except chaining
 * descriptors to ring and point them to static fraglists.
 *
 * splimp() assumed to be done
 */
static int
epic_init_rings(epic_softc_t * sc){
	int i;
	struct mbuf *m;

	sc->cur_rx = sc->cur_tx = sc->dirty_tx = sc->pending_txs = 0;

	for (i = 0; i < RX_RING_SIZE; i++) {
		struct epic_rx_buffer *buf = sc->rx_buffer + i;

		buf->desc.status = 0;		/* Owned by driver */
		buf->desc.next = 
			vtophys(&(sc->rx_buffer[(i+1)%RX_RING_SIZE].desc) );

#if defined(RX_TO_MBUF)
		MGETHDR(buf->mbuf,M_DONTWAIT,MT_DATA);
		if( NULL == buf->mbuf ) return -1;
		MCLGET(buf->mbuf,M_DONTWAIT);
		if( NULL == (buf->mbuf->m_flags & M_EXT) ) return -1;

		buf->data = mtod( buf->mbuf, caddr_t );
#else
		buf->data = malloc(ETHER_MAX_FRAME_LEN, M_DEVBUF, M_NOWAIT);
		if( buf->data == NULL ) return -1;
#endif

		buf->desc.bufaddr = vtophys( buf->data );
		buf->desc.buflength = ETHER_MAX_FRAME_LEN;
		buf->desc.status = 0x8000;		/* Give to EPIC */

	}

	for (i = 0; i < TX_RING_SIZE; i++) {
		struct epic_tx_buffer *buf = sc->tx_buffer + i;

		buf->desc.status = 0;
		buf->desc.next =
			vtophys(&(sc->tx_buffer[(i+1)%TX_RING_SIZE].desc) );

#if defined(TX_FRAG_LIST)
		buf->mbuf = NULL;
		buf->desc.bufaddr = vtophys( &(buf->flist) );
#else
		/* Allocate buffer */
		buf->data = malloc(ETHER_MAX_FRAME_LEN, M_DEVBUF, M_NOWAIT);

		if( buf->data == NULL ) return -1;

		buf->desc.bufaddr = vtophys( buf->data );
		buf->desc.buflength = ETHER_MAX_FRAME_LEN;
#endif
	}

	/* Give rings to EPIC */
	outl( sc->iobase + PRCDAR, vtophys(&(sc->rx_buffer[0].desc)) );
	outl( sc->iobase + PTCDAR, vtophys(&(sc->tx_buffer[0].desc)) );

	return 0;
}

/*
 * EEPROM operation functions
 */
static void epic_write_eepromreg(u_int16_t regaddr, u_int8_t val){
	u_int16_t i;

	outb( regaddr, val );

	for( i=0;i<0xFF; i++)
		if( !(inb( regaddr ) & 0x20) ) break;

	return;
}

static u_int8_t epic_read_eepromreg(u_int16_t regaddr){
	return inb( regaddr );
}  

static u_int8_t epic_eeprom_clock( u_int16_t ioaddr, u_int8_t val ){

	epic_write_eepromreg( ioaddr + EECTL, val );
	epic_write_eepromreg( ioaddr + EECTL, (val | 0x4) );
	epic_write_eepromreg( ioaddr + EECTL, val );
	
	return epic_read_eepromreg( ioaddr + EECTL );
}

static void epic_output_eepromw(u_int16_t ioaddr, u_int16_t val){
	int i;          
	for( i = 0xF; i >= 0; i--){
		if( (val & (1 << i)) ) epic_eeprom_clock( ioaddr, 0x0B );
		else epic_eeprom_clock( ioaddr, 3);
	}
}

static u_int16_t epic_input_eepromw(u_int16_t ioaddr){
	int i;
	int tmp;
	u_int16_t retval = 0;

	for( i = 0xF; i >= 0; i--) {	
		tmp = epic_eeprom_clock( ioaddr, 0x3 );
		if( tmp & 0x10 ){
			retval |= (1 << i);
		}
	}
	return retval;
}

static int epic_read_eeprom(u_int16_t ioaddr, u_int16_t loc){
	int i;
	u_int16_t dataval;
	u_int16_t read_cmd;

	epic_write_eepromreg(ioaddr + EECTL , 3);

	if( epic_read_eepromreg(ioaddr + EECTL) & 0x40 )
		read_cmd = ( loc & 0x3F ) | 0x180;
	else
		read_cmd = ( loc & 0xFF ) | 0x600;

	epic_output_eepromw( ioaddr, read_cmd );

        dataval = epic_input_eepromw( ioaddr );

	epic_write_eepromreg( ioaddr + EECTL, 1 );
	
	return dataval;
}

static int epic_read_phy_register(u_int16_t iobase, u_int16_t loc){
	int i;

	outl( iobase + MIICTL, ((loc << 4) | 0x0601) );

	for( i=0;i<0x1000;i++) if( !(inl( iobase + MIICTL )&1) ) break;

	return inl( iobase + MIIDATA );
}

static void epic_write_phy_register(u_int16_t iobase, u_int16_t loc,u_int16_t val){
	int i;

	outl( iobase + MIIDATA, val );
	outl( iobase + MIICTL, ((loc << 4) | 0x0602) );

	for( i=0;i<0x1000;i++) if( !(inl( iobase + MIICTL )&2) ) break;

	return;
}
