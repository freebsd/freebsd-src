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
 * version: stable-165
 *
 */

/*
 * EtherPower II 10/100  Fast Ethernet (tx0)
 * (aka SMC9432TX based on SMC83c170 EPIC chip)
 *
 * Written by Semen Ustimenko.
 *
 * TODO:
 *	Add IFF_MULTICAST support
 *	Fix serious collision counter behaviour
 *	Fix RX_TO_MBUF option
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
 */

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
#include <net/if_mib.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <vm/vm.h>
#include <vm/pmap.h>

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
			if ((ifp->if_flags & IFF_RUNNING) == 0)
				epic_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				epic_stop(sc);
				ifp->if_flags &= ~IFF_RUNNING;
			}
		}

		/* Update RXCON register */
		epic_set_rx_mode( sc );

		/* Update SPEED */
		epic_set_media_speed( sc );

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
 * Ifstart function
 */
static void
epic_ifstart(struct ifnet * const ifp){
	epic_softc_t *sc = ifp->if_softc;

	while( sc->pending_txs < TX_RING_SIZE  ){
		int entry = sc->cur_tx % TX_RING_SIZE;
		struct epic_tx_buffer * buf = sc->tx_buffer + entry;
		struct mbuf *m,*m0;
		int len;

		if( buf->desc.status ) break;

		IF_DEQUEUE( &(sc->epic_if.if_snd), m );

		if( NULL == m ) return;

		m0 = m;

		for (len = 0; m != 0; m = m->m_next) {
			bcopy(mtod(m, caddr_t), buf->data + len, m->m_len);
			len += m->m_len;
		}

		buf->desc.txlength = max(len,ETHER_MIN_LEN-ETHER_CRC_LEN);
		buf->desc.control = 0x14;	/* Interrupt when done */
		buf->desc.status = 0x8000;	/* Pass ownership to the chip */

		/* Set watchdog timer */
		ifp->if_timer = 3;

#if NBPFILTER > 0
		if( ifp->if_bpf ) bpf_mtap( ifp, m0 );
#endif

		m_freem( m0 );

		/* Trigger an immediate transmit demand. */
		outl(sc->iobase + COMMAND, 0x0004);

		sc->cur_tx = ( sc->cur_tx + 1 ) % TX_RING_SIZE;
		sc->pending_txs++;
	}

	sc->epic_if.if_flags |= IFF_OACTIVE;

	return;
	
}

/*
 *  IFWATCHDOG function
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
 */
static void
epic_intr_normal(
    void *arg)
{
	epic_softc_t * sc = (epic_softc_t *) arg;
	int iobase = sc->iobase;
        int status;
	int i;

	status = inl(iobase + INTSTAT);

	/* Acknowledge all of the current interrupt sources ASAP. */
	outl( iobase + INTSTAT, status & 0x0000ffff);

	if( status & (INTSTAT_RQE|INTSTAT_HCC|INTSTAT_RCC|INTSTAT_OVW) )
		epic_rx_done( sc );

	if( status & (INTSTAT_TXC|INTSTAT_TQE|INTSTAT_TCC|INTSTAT_TXU) )
		epic_tx_done( sc );

/*
	if( status & INTSTAT_GP2 ){
		printf("tx%d: GP2 int occured\n",sc->unit);
		epic_read_phy_register(sc->iobase,DP83840_BMSR);
		epic_read_phy_register(sc->iobase,DP83840_BMCR);
	}
*/
	if( status & (INTSTAT_FATAL|INTSTAT_PMA|INTSTAT_PTA|INTSTAT_APE|INTSTAT_DPE) ){
		printf("tx%d: PCI fatal error occured (%s%s%s%s)",
			sc->unit,
			(status&INTSTAT_PMA)?"PMA":"",
			(status&INTSTAT_PTA)?" PTA":"",
			(status&INTSTAT_APE)?" APE":"",
			(status&INTSTAT_DPE)?" DPE":"");
	}

        /* UPDATE statistics */
	if (status & (INTSTAT_CNT | INTSTAT_TXU | INTSTAT_OVW | INTSTAT_RXE)) {
		/*
		 * update dot3 Rx statistics
		 */
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
		outl(iobase + INTSTAT, status & 0x7f18);
	}

	/* If no packets are pending, thus no timeouts */
	if( sc->pending_txs == 0 )
		sc->epic_if.if_timer = 0;

	/* We should clear all interrupt sources. */
	outl(iobase + INTSTAT, 0xffffffff );

	return;
}

void
epic_rx_done(
	epic_softc_t *sc )
{
        int i = 0;
	u_int16_t len;
	struct epic_rx_buffer *	buf;
	struct mbuf *m;
	struct ether_header *eh;

	while( !(sc->rx_buffer[sc->cur_rx].desc.status & 0x8000) && \
		i++ < RX_RING_SIZE ){
		int stt;

		buf = sc->rx_buffer + sc->cur_rx;
		stt = buf->desc.status;

#if defined(EPIC_DEBUG)
		printf("tx%d: ",sc->unit);
		if(stt&1)printf("rsucc");
		else printf("     ");
		if(stt&2)printf(" faer");
		else printf("     ");
		if(stt&4)printf(" crer");
		else printf("     ");
		if(stt&8)printf(" mpac");
		else printf("     ");
		if(stt&16)printf(" mcas");
		else printf("     ");
		if(stt&32)printf(" bcas\n");
		else printf("     \n");
#endif

		if( !(buf->desc.status&1) ){
			sc->epic_if.if_ierrors++;
			goto rxerror;
		}

		len = buf->desc.rxlength - ETHER_CRC_LEN;

#if !defined(RX_TO_MBUF)
		/*
		 * Copy data to new allocated mbuf
		 */
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if( NULL == m ) goto rxerror;
		if( (len+2) > MHLEN ){
			MCLGET(m,M_DONTWAIT);
			if( NULL == (m->m_flags & M_EXT) ){
				m_freem( m );
				goto rxerror;
			}
		}	
		m->m_data += 2;

		memcpy( mtod(m,void*), buf->data, len );
#else
		m = buf->mbuf;

		buf->mbuf = NULL;

		MGETHDR(buf->mbuf,M_DONTWAIT,MT_DATA);
		if( NULL == buf->mbuf )			/* XXXXX: to panic */
			panic("tx: low mbufs");		/* or not to panic?*/
		MCLGET(buf->mbuf,M_DONTWAIT);
		if( NULL == buf->mbuf )
			panic("tx: low mbufs");

		buf->data = mtod( buf->mbuf, caddr_t );
		buf->desc.bufaddr = vtophys( buf->data );
		buf->desc.status = 0x8000;
#endif

		/*
		 * First mbuf in packet holds the
		 * ethernet and packet headers
		 */
		eh = mtod( m, struct ether_header * );
		m->m_pkthdr.rcvif = &(sc->epic_if);
		m->m_pkthdr.len = len;
		m->m_len = len;

#if NBPFILTER > 0
		if( sc->epic_if.if_bpf ) bpf_mtap( &sc->epic_if, m );

		/* Accept only our packets, broadcasts and multicasts */
		if( (eh->ether_dhost[0] & 1) == 0 &&
		    bcmp(eh->ether_dhost,sc->epic_ac.ac_enaddr,ETHER_ADDR_LEN)){
			m_freem(m);
			goto rxerror;
		}
#endif
		m->m_pkthdr.len = len - sizeof(struct ether_header);
		m->m_len = len - sizeof( struct ether_header );
		m->m_data += sizeof( struct ether_header );

		ether_input(&sc->epic_if, eh, m);

		sc->epic_if.if_ipackets++;

rxerror:
		/*
		 * Mark descriptor as free
		 */
		buf->desc.rxlength = 0;
		buf->desc.status = 0x8000;

		sc->cur_rx = (sc->cur_rx+1) % RX_RING_SIZE;
        }

	epic_ifstart( &sc->epic_if );

	outl( sc->iobase + INTSTAT, INTSTAT_RCC );
}

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

#if defined(EPIC_DEBUG)
		printf("tx%d: ",sc->unit);
		if(stt&1) printf(" succ");
		else printf("     ");
		if(stt&2) printf(" ndef");
		else printf("     ");
		if(stt&4) printf(" coll");
		else printf("     ");
		if(stt&8) printf(" urun");
		else printf("     ");
		if(stt&16) printf(" cdhb");
		else printf("     ");
		if(stt&32) printf(" oowc");
		else printf("     ");
		if(stt&64) printf(" deff");
		else printf("     ");
		printf(" %d\n",(stt>>8)&0xF);
#endif

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

		if_flags = ~IFF_OACTIVE;
	}

	sc->epic_if.if_flags &= if_flags;

	outl( sc->iobase + INTSTAT, INTSTAT_TCC );

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
	int i;
	int s;
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
		printf("tx%d: WARNING! no link estabilished/n",sc->unit);

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
 */
static void
epic_init(
    epic_softc_t * sc)
{       
	struct ifnet *ifp = &sc->epic_if;
	int iobase = sc->iobase;
	int i;

	/* Soft reset the chip. */
	outl(iobase + GENCTL, GENCTL_SOFT_RESET );

	/* Reset takes 15 ticks */
	for(i=0;i<0x100;i++);

	/* Wake up */
	outl( iobase + GENCTL, 0 );

	/* ?????? */
	outl( iobase + TEST1, 0x0008);

	/* Initialize rings or reinitialize */
	epic_init_rings( sc );

	/* Put node address to EPIC */
	outl( iobase + LAN0 + 0x0, ((u_int16_t *)sc->epic_macaddr)[0] );
        outl( iobase + LAN0 + 0x4, ((u_int16_t *)sc->epic_macaddr)[1] );
	outl( iobase + LAN0 + 0x8, ((u_int16_t *)sc->epic_macaddr)[2] );

	/* Enable interrupts,  set for PCI read multiple and etc */
	outl( iobase + GENCTL,
		GENCTL_ENABLE_INTERRUPT | GENCTL_MEMORY_READ_MULTIPLE |
		GENCTL_ONECOPY | GENCTL_RECEIVE_FIFO_THRESHOLD128 );

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

	return;
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

#if defined(EPIC_DEBUG)
		printf("tx%d: %dMbps %s\n",sc->unit,
			(ifp->if_flags&IFF_LINK2)?100:10,
			(ifp->if_flags&IFF_LINK1)?"full-duplex":"half-duplex" );
#endif
	} else {
		/* If autoneg is set, IFF_LINK flags are meaningless */
		ifp->if_flags &= ~(IFF_LINK0|IFF_LINK1|IFF_LINK2);
		ifp->if_baudrate = 100000000;

		outl( sc->iobase + TXCON, TXCON_DEFAULT );

		/* Does not allow to autoneg fullduplex modes */
		media = epic_read_phy_register( sc->iobase, DP83840_ANAR );
		media &= ~(ANAR_100|ANAR_100_FD|ANAR_10_FD|ANAR_10);
		media |= ANAR_100|ANAR_10;
		epic_write_phy_register( sc->iobase, DP83840_ANAR, media );

		/* Set and restart autoneg */
		epic_write_phy_register( sc->iobase, DP83840_BMCR,
			BMCR_AUTONEGOTIATION | BMCR_RESTART_AUTONEG );

#if defined(EPIC_DEBUG)
		printf("tx%d: Autonegotiation\n",sc->unit);
#endif
	}

	return;
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
#if defined(EPIC_DEBUG)
		if( !(ifp->if_flags & IFF_ALLMULTI) )
			printf("tx%d: WARNING! only receive all multicasts mode supported\n",sc->unit);
#endif
		outl( sc->iobase + MC0, 0xFFFF );
		outl( sc->iobase + MC1, 0xFFFF );
		outl( sc->iobase + MC2, 0xFFFF );
		outl( sc->iobase + MC3, 0xFFFF );
	}

	return;
}

/*
 *  This function should completely stop rx and tx processes
 */
static void
epic_stop(
    epic_softc_t * sc)
{
	int iobase = sc->iobase;

	outl( iobase + INTMASK, 0 );
	outl( iobase + GENCTL, 0 );
	outl( iobase + COMMAND,
		COMMAND_STOP_RX | COMMAND_STOP_TDMA | COMMAND_STOP_TDMA );

	sc->epic_if.if_timer = 0;

}

/*
 * Initialize Rx ad Tx rings and give them to EPIC
 *
 * If RX_TO_MBUF option is enabled, mbuf cluster is allocated instead of
 * static buffer.
 */
static void
epic_init_rings(epic_softc_t * sc){
	int i;
	struct mbuf *m;

	sc->cur_rx = sc->cur_tx = sc->dirty_tx = sc->pending_txs = 0;

	for (i = 0; i < RX_RING_SIZE; i++) {
		struct epic_rx_buffer *buf = sc->rx_buffer + i;

		buf->desc.status = 0x0000;		/* Owned by Epic chip */
		buf->desc.buflength = 0;
		buf->desc.bufaddr = 0;
		buf->desc.next = vtophys(&(sc->rx_buffer[(i+1)%RX_RING_SIZE].desc) );

		buf->data = NULL;

#if !defined(RX_TO_MBUF)
		if( buf->pool ){
			free( buf->pool, M_DEVBUF );
			buf->pool = buf->data = 0;
		}
		buf->pool = malloc(ETHER_MAX_FRAME_LEN, M_DEVBUF, M_NOWAIT);
		if( buf->pool == NULL ){
			printf("tx%d: malloc failed\n",sc->unit);
			continue;
		}
		buf->data = (caddr_t)buf->pool;
#else
		if( buf->mbuf ){
			m_freem( buf->mbuf );
			buf->mbuf = NULL;
		}
		MGETHDR(buf->mbuf,M_DONTWAIT,MT_DATA);
		if( NULL == buf->mbuf ) continue;
		MCLGET(buf->mbuf,M_DONTWAIT);
		if( NULL == (buf->mbuf->m_flags & M_EXT) ){
			m_freem( buf->mbuf );
			continue;
		}

		buf->data = mtod( buf->mbuf, caddr_t );
#endif
		buf->desc.bufaddr = vtophys( buf->data );
		buf->desc.buflength = ETHER_MAX_FRAME_LEN;
		buf->desc.status = 0x8000;

	}

	for (i = 0; i < TX_RING_SIZE; i++) {
		struct epic_tx_buffer *buf = sc->tx_buffer + i;

		buf->desc.status = 0x0000;
		buf->desc.buflength = 0;
		buf->desc.bufaddr = 0;
		buf->desc.control = 0;
		buf->desc.next = vtophys(&(sc->tx_buffer[(i+1)%TX_RING_SIZE].desc) );

		if( buf->pool ){
			free( buf->pool, M_DEVBUF );
			buf->pool = buf->data = 0;
		}

		buf->pool = malloc(ETHER_MAX_FRAME_LEN, M_DEVBUF, M_NOWAIT);

		if( buf->pool == NULL ){
			printf("tx%d: malloc failed\n",sc->unit);
			continue;
		}

		buf->data = (caddr_t)buf->pool;

		buf->desc.bufaddr = vtophys( buf->data );
		buf->desc.buflength = ETHER_MAX_FRAME_LEN;
	}

	/* Give rings to EPIC */
	outl( sc->iobase + PRCDAR, vtophys(&(sc->rx_buffer[0].desc)) );
	outl( sc->iobase + PTCDAR, vtophys(&(sc->tx_buffer[0].desc)) );

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

#endif /* NPCI > 0 */
