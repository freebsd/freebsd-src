/*
 * Copyright (c) 1995, David Greenman
 * All rights reserved.
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
 *	$Id: if_fxp.c,v 1.35 1997/06/13 22:34:52 davidg Exp $
 */

/*
 * Intel EtherExpress Pro/100B PCI Fast Ethernet driver
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <vm/vm.h>		/* for vtophys */
#include <vm/vm_param.h>	/* for vtophys */
#include <vm/pmap.h>		/* for vtophys */
#include <machine/clock.h>	/* for DELAY */

#include <pci/pcivar.h>
#include <pci/if_fxpreg.h>

struct fxp_softc {
	struct arpcom arpcom;		/* per-interface network data */
	struct fxp_csr *csr;		/* control/status registers */
	struct fxp_cb_tx *cbl_base;	/* base of TxCB list */
	struct fxp_cb_tx *cbl_first;	/* first active TxCB in list */
	struct fxp_cb_tx *cbl_last;	/* last active TxCB in list */
	struct mbuf *rfa_headm;		/* first mbuf in receive frame area */
	struct mbuf *rfa_tailm;		/* last mbuf in receive frame area */
	struct fxp_stats *fxp_stats;	/* Pointer to interface stats */
	int tx_queued;			/* # of active TxCB's */
	int promisc_mode;		/* promiscuous mode enabled */
	int phy_primary_addr;		/* address of primary PHY */
	int phy_primary_device;		/* device type of primary PHY */
	int phy_10Mbps_only;		/* PHY is 10Mbps-only device */
};

static u_long fxp_count;

/*
 * Template for default configuration parameters.
 * See struct fxp_cb_config for the bit definitions.
 */
static u_char fxp_cb_config_template[] = {
	0x0, 0x0,		/* cb_status */
	0x80, 0x2,		/* cb_command */
	0xff, 0xff, 0xff, 0xff,	/* link_addr */
	0x16,	/*  0 */
	0x8,	/*  1 */
	0x0,	/*  2 */
	0x0,	/*  3 */
	0x0,	/*  4 */
	0x80,	/*  5 */
	0xb2,	/*  6 */
	0x3,	/*  7 */
	0x1,	/*  8 */
	0x0,	/*  9 */
	0x26,	/* 10 */
	0x0,	/* 11 */
	0x60,	/* 12 */
	0x0,	/* 13 */
	0xf2,	/* 14 */
	0x48,	/* 15 */
	0x0,	/* 16 */
	0x40,	/* 17 */
	0xf3,	/* 18 */
	0x0,	/* 19 */
	0x3f,	/* 20 */
	0x5,	/* 21 */
	0x0, 0x0
};

static inline void fxp_scb_wait	__P((struct fxp_csr *));
static char *fxp_probe		__P((pcici_t, pcidi_t));
static void fxp_attach		__P((pcici_t, int));
static void fxp_intr		__P((void *));
static void fxp_start		__P((struct ifnet *));
static int fxp_ioctl		__P((struct ifnet *, int, caddr_t));
static void fxp_init		__P((void *));
static void fxp_stop		__P((struct fxp_softc *));
static void fxp_watchdog	__P((struct ifnet *));
static int fxp_add_rfabuf	__P((struct fxp_softc *, struct mbuf *));
static void fxp_shutdown	__P((int, void *));
static int fxp_mdi_read		__P((struct fxp_csr *, int, int));
static void fxp_mdi_write	__P((struct fxp_csr *, int, int, int));
static void fxp_read_eeprom	__P((struct fxp_csr *, u_short *, int, int));


timeout_t fxp_stats_update;

static struct pci_device fxp_device = {
	"fxp",
	fxp_probe,
	fxp_attach,
	&fxp_count,
	NULL
};
DATA_SET(pcidevice_set, fxp_device);

/*
 * Set initial transmit threshold at 64 (512 bytes). This is
 * increased by 64 (512 bytes) at a time, to maximum of 192
 * (1536 bytes), if an underrun occurs.
 */
static int tx_threshold = 64;

/*
 * Number of transmit control blocks. This determines the number
 * of transmit buffers that can be chained in the CB list.
 * This must be a power of two.
 */
#define FXP_NTXCB	128

/*
 * TxCB list index mask. This is used to do list wrap-around.
 */
#define FXP_TXCB_MASK	(FXP_NTXCB - 1)

/*
 * Number of DMA segments in a TxCB. Note that this is carefully
 * chosen to make the total struct size an even power of two. It's
 * critical that no TxCB be split across a page boundry since
 * no attempt is made to allocate physically contiguous memory.
 * 
 * XXX - don't forget to change the hard-coded constant in the
 * fxp_cb_tx struct (defined in if_fxpreg.h), too!
 */
#define FXP_NTXSEG	29

/*
 * Number of receive frame area buffers. These are large so chose
 * wisely.
 */
#define FXP_NRFABUFS	32

/*
 * Wait for the previous command to be accepted (but not necessarily
 * completed).
 */
static inline void
fxp_scb_wait(csr)
	struct fxp_csr *csr;
{
	int i = 10000;

	while ((csr->scb_command & FXP_SCB_COMMAND_MASK) && --i);
}

/*
 * Return identification string if this is device is ours.
 */
static char *
fxp_probe(config_id, device_id)
	pcici_t config_id;
	pcidi_t device_id;
{
	if (((device_id & 0xffff) == FXP_VENDORID_INTEL) &&
	    ((device_id >> 16) & 0xffff) == FXP_DEVICEID_i82557)
		return ("Intel EtherExpress Pro 10/100B Ethernet");

	return NULL;
}

/*
 * Allocate data structures and attach the device.
 */
static void
fxp_attach(config_id, unit)
	pcici_t config_id;
	int unit;
{
	struct fxp_softc *sc;
	struct ifnet *ifp;
	vm_offset_t pbase;
	int s, i;
	u_short data;

	sc = malloc(sizeof(struct fxp_softc), M_DEVBUF, M_NOWAIT);
	if (sc == NULL)
		return;
	bzero(sc, sizeof(struct fxp_softc));

	s = splimp();

	/*
	 * Map control/status registers.
	 */
	if (!pci_map_mem(config_id, FXP_PCI_MMBA,
	    (vm_offset_t *)&sc->csr, &pbase)) {
		printf("fxp%d: couldn't map memory\n", unit);
		goto fail;
	}

	/*
	 * Reset to a stable state.
	 */
	sc->csr->port = FXP_PORT_SELECTIVE_RESET;
	DELAY(10);

	/*
	 * Allocate our interrupt.
	 */
	if (!pci_map_int(config_id, fxp_intr, sc, &net_imask)) {
		printf("fxp%d: couldn't map interrupt\n", unit);
		goto fail;
	}

	sc->cbl_base = malloc(sizeof(struct fxp_cb_tx) * FXP_NTXCB,
	    M_DEVBUF, M_NOWAIT);
	if (sc->cbl_base == NULL)
		goto malloc_fail;

	sc->fxp_stats = malloc(sizeof(struct fxp_stats), M_DEVBUF, M_NOWAIT);
	if (sc->fxp_stats == NULL)
		goto malloc_fail;
	bzero(sc->fxp_stats, sizeof(struct fxp_stats));

	/*
	 * Pre-allocate our receive buffers.
	 */
	for (i = 0; i < FXP_NRFABUFS; i++) {
		if (fxp_add_rfabuf(sc, NULL) != 0) {
			goto malloc_fail;
		}
	}

	/*
	 * Get info about the primary PHY
	 */
	fxp_read_eeprom(sc->csr, (u_short *)&data, 6, 1);
	sc->phy_primary_addr = data & 0xff;
	sc->phy_primary_device = (data >> 8) & 0x3f;
	sc->phy_10Mbps_only = data >> 15;

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_unit = unit;
	ifp->if_name = "fxp";
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = fxp_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = fxp_start;
	ifp->if_watchdog = fxp_watchdog;
	ifp->if_baudrate = 100000000;
	ifp->if_init = fxp_init;

	/*
	 * Read MAC address
	 */
	fxp_read_eeprom(sc->csr, (u_short *)sc->arpcom.ac_enaddr, 0, 3);
	printf("fxp%d: Ethernet address %6D", unit, sc->arpcom.ac_enaddr, ":");
	if (sc->phy_10Mbps_only)
		printf(", 10Mbps");
	printf("\n");

	/*
	 * Attach the interface.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPFILTER > 0
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif

	/*
	 * Add shutdown hook so that DMA is disabled prior to reboot. Not
	 * doing do could allow DMA to corrupt kernel memory during the
	 * reboot before the driver initializes.
	 */
	at_shutdown(fxp_shutdown, sc, SHUTDOWN_POST_SYNC);

	splx(s);
	return;

malloc_fail:
	printf("fxp%d: Failed to malloc memory\n", unit);
	(void) pci_unmap_int(config_id);
	if (sc && sc->cbl_base)
		free(sc->cbl_base, M_DEVBUF);
	if (sc && sc->fxp_stats)
		free(sc->fxp_stats, M_DEVBUF);
	/* frees entire chain */
	if (sc && sc->rfa_headm)
		m_freem(sc->rfa_headm);
fail:
	if (sc)
		free(sc, M_DEVBUF);
	splx(s);
}

/*
 * Read from the serial EEPROM. Basically, you manually shift in
 * the read opcode (one bit at a time) and then shift in the address,
 * and then you shift out the data (all of this one bit at a time).
 * The word size is 16 bits, so you have to provide the address for
 * every 16 bits of data.
 */
static void
fxp_read_eeprom(csr, data, offset, words)
	struct fxp_csr *csr;
	u_short *data;
	int offset;
	int words;
{
	u_short reg;
	int i, x;

	for (i = 0; i < words; i++) {
		csr->eeprom_control = FXP_EEPROM_EECS;
		/*
		 * Shift in read opcode.
		 */
		for (x = 3; x > 0; x--) {
			if (FXP_EEPROM_OPC_READ & (1 << (x - 1))) {
				reg = FXP_EEPROM_EECS | FXP_EEPROM_EEDI;
			} else {
				reg = FXP_EEPROM_EECS;
			}
			csr->eeprom_control = reg;
			csr->eeprom_control = reg | FXP_EEPROM_EESK;
			DELAY(1);
			csr->eeprom_control = reg;
			DELAY(1);
		}
		/*
		 * Shift in address.
		 */
		for (x = 6; x > 0; x--) {
			if ((i + offset) & (1 << (x - 1))) {
				reg = FXP_EEPROM_EECS | FXP_EEPROM_EEDI;
			} else {
				reg = FXP_EEPROM_EECS;
			}
			csr->eeprom_control = reg;
			csr->eeprom_control = reg | FXP_EEPROM_EESK;
			DELAY(1);
			csr->eeprom_control = reg;
			DELAY(1);
		}
		reg = FXP_EEPROM_EECS;
		data[i] = 0;
		/*
		 * Shift out data.
		 */
		for (x = 16; x > 0; x--) {
			csr->eeprom_control = reg | FXP_EEPROM_EESK;
			DELAY(1);
			if (csr->eeprom_control & FXP_EEPROM_EEDO)
				data[i] |= (1 << (x - 1));
			csr->eeprom_control = reg;
			DELAY(1);
		}
		csr->eeprom_control = 0;
		DELAY(1);
	}
}

/*
 * Device shutdown routine. Called at system shutdown after sync. The
 * main purpose of this routine is to shut off receiver DMA so that
 * kernel memory doesn't get clobbered during warmboot.
 */
static void
fxp_shutdown(howto, sc)
	int howto;
	void *sc;
{
	fxp_stop((struct fxp_softc *) sc);
}

/*
 * Start packet transmission on the interface.
 */
static void
fxp_start(ifp)
	struct ifnet *ifp;
{
	struct fxp_softc *sc = ifp->if_softc;
	struct fxp_csr *csr = sc->csr;
	struct fxp_cb_tx *txp;
	struct mbuf *m, *mb_head;
	int segment, first = 1;

txloop:
	/*
	 * See if we're all filled up with buffers to transmit.
	 */
	if (sc->tx_queued >= FXP_NTXCB)
		return;

	/*
	 * Grab a packet to transmit.
	 */
	IF_DEQUEUE(&ifp->if_snd, mb_head);
	if (mb_head == NULL) {
		/*
		 * No more packets to send.
		 */
		return;
	}

	/*
	 * Get pointer to next available (unused) descriptor.
	 */
	txp = sc->cbl_last->next;

	/*
	 * Go through each of the mbufs in the chain and initialize
	 * the transmit buffers descriptors with the physical address
	 * and size of the mbuf.
	 */
tbdinit:
	for (m = mb_head, segment = 0; m != NULL; m = m->m_next) {
		if (m->m_len != 0) {
			if (segment == FXP_NTXSEG)
				break;
			txp->tbd[segment].tb_addr =
			    vtophys(mtod(m, vm_offset_t));
			txp->tbd[segment].tb_size = m->m_len;
			segment++;
		}
	}
	if (m != NULL) {
		struct mbuf *mn;

		/*
		 * We ran out of segments. We have to recopy this mbuf
		 * chain first.
		 */
		MGETHDR(mn, M_DONTWAIT, MT_DATA);
		if (mn == NULL) {
			m_freem(mb_head);
			return;
		}
		if (mb_head->m_pkthdr.len > MHLEN) {
			MCLGET(mn, M_DONTWAIT);
			if ((mn->m_flags & M_EXT) == 0) {
				m_freem(mn);
				m_freem(mb_head);
				return;
			}
		}
		m_copydata(mb_head, 0, mb_head->m_pkthdr.len, mtod(mn, caddr_t));
		mn->m_pkthdr.len = mn->m_len = mb_head->m_pkthdr.len;
		m_freem(mb_head);
		mb_head = mn;
		goto tbdinit;
	}

	txp->tbd_number = segment;
	txp->mb_head = mb_head;

	/*
	 * Finish the initialization of this TxCB.
	 */
	txp->cb_status = 0;
	txp->cb_command =
	    FXP_CB_COMMAND_XMIT | FXP_CB_COMMAND_SF | FXP_CB_COMMAND_S;
	txp->tx_threshold = tx_threshold;
	
	/*
	 * Advance the end-of-list forward.
	 */
	sc->cbl_last->cb_command &= ~FXP_CB_COMMAND_S;
	sc->cbl_last = txp;

	/*
	 * Advance the beginning of the list forward if there are
	 * no other packets queued (when nothing is queued, cbl_first
	 * sits on the last TxCB that was sent out)..
	 */
	if (sc->tx_queued == 0)
		sc->cbl_first = txp;

	sc->tx_queued++;

	/*
	 * Only need to wait prior to the first resume command.
	 */
	if (first) {
		first--;
		fxp_scb_wait(csr);
	}

	/*
	 * Resume transmission if suspended.
	 */
	csr->scb_command = FXP_SCB_COMMAND_CU_RESUME;

#if NBPFILTER > 0
	/*
	 * Pass packet to bpf if there is a listener.
	 */
	if (ifp->if_bpf)
		bpf_mtap(ifp, mb_head);
#endif
	/*
	 * Set a 5 second timer just in case we don't hear from the
	 * card again.
	 */
	ifp->if_timer = 5;

	goto txloop;
}

/*
 * Process interface interrupts.
 */
static void
fxp_intr(arg)
	void *arg;
{
	struct fxp_softc *sc = arg;
	struct fxp_csr *csr = sc->csr;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	u_int8_t statack;

	while ((statack = csr->scb_statack) != 0) {
		/*
		 * First ACK all the interrupts in this pass.
		 */
		csr->scb_statack = statack;

		/*
		 * Free any finished transmit mbuf chains.
		 */
		if (statack & FXP_SCB_STATACK_CNA) {
			struct fxp_cb_tx *txp;

			for (txp = sc->cbl_first;
			    (txp->cb_status & FXP_CB_STATUS_C) != 0;
			    txp = txp->next) {
				if (txp->mb_head != NULL) {
					m_freem(txp->mb_head);
					txp->mb_head = NULL;
					sc->tx_queued--;
				}
				if (txp == sc->cbl_last)
					break;
			}
			sc->cbl_first = txp;
			ifp->if_timer = 0;
			/*
			 * Try to start more packets transmitting.
			 */
			if (ifp->if_snd.ifq_head != NULL)
				fxp_start(ifp);
		}
		/*
		 * Process receiver interrupts. If a no-resource (RNR)
		 * condition exists, get whatever packets we can and
		 * re-start the receiver.
		 */
		if (statack & (FXP_SCB_STATACK_FR | FXP_SCB_STATACK_RNR)) {
			struct mbuf *m;
			struct fxp_rfa *rfa;
rcvloop:
			m = sc->rfa_headm;
			rfa = (struct fxp_rfa *)m->m_ext.ext_buf;

			if (rfa->rfa_status & FXP_RFA_STATUS_C) {
				/*
				 * Remove first packet from the chain.
				 */
				sc->rfa_headm = m->m_next;
				m->m_next = NULL;

				/*
				 * Add a new buffer to the receive chain. If this
				 * fails, the old buffer is recycled instead.
				 */
				if (fxp_add_rfabuf(sc, m) == 0) {
					struct ether_header *eh;
					u_short total_len;

					total_len = rfa->actual_size & (MCLBYTES - 1);
					if (total_len < sizeof(struct ether_header)) {
						m_freem(m);
						goto rcvloop;
					}
					m->m_pkthdr.rcvif = ifp;
					m->m_pkthdr.len = m->m_len = total_len -
					    sizeof(struct ether_header);
					eh = mtod(m, struct ether_header *);
#if NBPFILTER > 0
					if (ifp->if_bpf) {
						bpf_tap(ifp, mtod(m, caddr_t), total_len);
						/*
						 * Only pass this packet up if it is for us.
						 */
						if ((ifp->if_flags & IFF_PROMISC) &&
						    (rfa->rfa_status & FXP_RFA_STATUS_IAMATCH) &&
						    (eh->ether_dhost[0] & 1) == 0) {
							m_freem(m);
							goto rcvloop;
						}
					}
#endif
					m->m_data += sizeof(struct ether_header);
					ether_input(ifp, eh, m);
				}
				goto rcvloop;
			}
			if (statack & FXP_SCB_STATACK_RNR) {
				fxp_scb_wait(csr);
				csr->scb_general = vtophys(sc->rfa_headm->m_ext.ext_buf);
				csr->scb_command = FXP_SCB_COMMAND_RU_START;
			}
		}
	}
}

/*
 * Update packet in/out/collision statistics. The i82557 doesn't
 * allow you to access these counters without doing a fairly
 * expensive DMA to get _all_ of the statistics it maintains, so
 * we do this operation here only once per second. The statistics
 * counters in the kernel are updated from the previous dump-stats
 * DMA and then a new dump-stats DMA is started. The on-chip
 * counters are zeroed when the DMA completes. If we can't start
 * the DMA immediately, we don't wait - we just prepare to read
 * them again next time.
 */
void
fxp_stats_update(arg)
	void *arg;
{
	struct fxp_softc *sc = arg;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct fxp_stats *sp = sc->fxp_stats;

	ifp->if_opackets += sp->tx_good;
	ifp->if_collisions += sp->tx_total_collisions;
	ifp->if_ipackets += sp->rx_good;
	ifp->if_ierrors +=
	    sp->rx_crc_errors +
	    sp->rx_alignment_errors +
	    sp->rx_rnr_errors +
	    sp->rx_overrun_errors;
	/*
	 * If any transmit underruns occured, bump up the transmit
	 * threshold by another 512 bytes (64 * 8).
	 */
	if (sp->tx_underruns) {
		ifp->if_oerrors += sp->tx_underruns;
		if (tx_threshold < 192)
			tx_threshold += 64;
	}
	/*
	 * If there is no pending command, start another stats
	 * dump. Otherwise punt for now.
	 */
	if ((sc->csr->scb_command & FXP_SCB_COMMAND_MASK) == 0) {
		/*
		 * Start another stats dump. By waiting for it to be
		 * accepted, we avoid having to do splhigh locking when
		 * writing scb_command in other parts of the driver.
		 */
		sc->csr->scb_command = FXP_SCB_COMMAND_CU_DUMPRESET;
		fxp_scb_wait(sc->csr);
	} else {
		/*
		 * A previous command is still waiting to be accepted.
		 * Just zero our copy of the stats and wait for the
		 * next timer event to update them.
		 */
		sp->tx_good = 0;
		sp->tx_underruns = 0;
		sp->tx_total_collisions = 0;

		sp->rx_good = 0;
		sp->rx_crc_errors = 0;
		sp->rx_alignment_errors = 0;
		sp->rx_rnr_errors = 0;
		sp->rx_overrun_errors = 0;
	}
	/*
	 * Schedule another timeout one second from now.
	 */
	timeout(fxp_stats_update, sc, hz);
}

/*
 * Stop the interface. Cancels the statistics updater and resets
 * the interface.
 */
static void
fxp_stop(sc)
	struct fxp_softc *sc;
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct fxp_cb_tx *txp;
	int i;

	/*
	 * Cancel stats updater.
	 */
	untimeout(fxp_stats_update, sc);

	/*
	 * Issue software reset
	 */
	sc->csr->port = FXP_PORT_SELECTIVE_RESET;
	DELAY(10);

	/*
	 * Release any xmit buffers.
	 */
	for (txp = sc->cbl_first; txp != NULL && txp->mb_head != NULL;
	    txp = txp->next) {
		m_freem(txp->mb_head);
		txp->mb_head = NULL;
	}
	sc->tx_queued = 0;

	/*
	 * Free all the receive buffers then reallocate/reinitialize
	 */
	if (sc->rfa_headm != NULL)
		m_freem(sc->rfa_headm);
	sc->rfa_headm = NULL;
	sc->rfa_tailm = NULL;
	for (i = 0; i < FXP_NRFABUFS; i++) {
		if (fxp_add_rfabuf(sc, NULL) != 0) {
			/*
			 * This "can't happen" - we're at splimp()
			 * and we just freed all the buffers we need
			 * above.
			 */
			panic("fxp_stop: no buffers!");
		}
	}

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;
}

/*
 * Watchdog/transmission transmit timeout handler. Called when a
 * transmission is started on the interface, but no interrupt is
 * received before the timeout. This usually indicates that the
 * card has wedged for some reason.
 */
static void
fxp_watchdog(ifp)
	struct ifnet *ifp;
{
	log(LOG_ERR, "fxp%d: device timeout\n", ifp->if_unit);
	ifp->if_oerrors++;

	fxp_init(ifp->if_softc);
}

static void
fxp_init(xsc)
	void *xsc;
{
	struct fxp_softc *sc = xsc;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct fxp_cb_config *cbp;
	struct fxp_cb_ias *cb_ias;
	struct fxp_cb_tx *txp;
	struct fxp_csr *csr = sc->csr;
	int i, s, mcast, prm;

	s = splimp();
	/*
	 * Cancel any pending I/O
	 */
	fxp_stop(sc);

	prm = (ifp->if_flags & IFF_PROMISC) ? 1 : 0;
	sc->promisc_mode = prm;
	/*
	 * Sleeze out here and enable reception of all multicasts if
	 * multicasts are enabled. Ideally, we'd program the multicast
	 * address filter to only accept specific multicasts.
	 */
	mcast = (ifp->if_flags & (IFF_MULTICAST|IFF_ALLMULTI)) ? 1 : 0;

	/*
	 * Initialize base of CBL and RFA memory. Loading with zero
	 * sets it up for regular linear addressing.
	 */
	csr->scb_general = 0;
	csr->scb_command = FXP_SCB_COMMAND_CU_BASE;

	fxp_scb_wait(csr);
	csr->scb_command = FXP_SCB_COMMAND_RU_BASE;

	/*
	 * Initialize base of dump-stats buffer.
	 */
	fxp_scb_wait(csr);
	csr->scb_general = vtophys(sc->fxp_stats);
	csr->scb_command = FXP_SCB_COMMAND_CU_DUMP_ADR;

	/*
	 * We temporarily use memory that contains the TxCB list to
	 * construct the config CB. The TxCB list memory is rebuilt
	 * later.
	 */
	cbp = (struct fxp_cb_config *) sc->cbl_base;

	/*
	 * This bcopy is kind of disgusting, but there are a bunch of must be
	 * zero and must be one bits in this structure and this is the easiest
	 * way to initialize them all to proper values.
	 */
	bcopy(fxp_cb_config_template, cbp, sizeof(struct fxp_cb_config));

	cbp->cb_status =	0;
	cbp->cb_command =	FXP_CB_COMMAND_CONFIG | FXP_CB_COMMAND_EL;
	cbp->link_addr =	-1;	/* (no) next command */
	cbp->byte_count =	22;	/* (22) bytes to config */
	cbp->rx_fifo_limit =	8;	/* rx fifo threshold (32 bytes) */
	cbp->tx_fifo_limit =	0;	/* tx fifo threshold (0 bytes) */
	cbp->adaptive_ifs =	0;	/* (no) adaptive interframe spacing */
	cbp->rx_dma_bytecount =	0;	/* (no) rx DMA max */
	cbp->tx_dma_bytecount =	0;	/* (no) tx DMA max */
	cbp->dma_bce =		0;	/* (disable) dma max counters */
	cbp->late_scb =		0;	/* (don't) defer SCB update */
	cbp->tno_int =		0;	/* (disable) tx not okay interrupt */
	cbp->ci_int =		0;	/* interrupt on CU not active */
	cbp->save_bf =		prm;	/* save bad frames */
	cbp->disc_short_rx =	!prm;	/* discard short packets */
	cbp->underrun_retry =	1;	/* retry mode (1) on DMA underrun */
	cbp->mediatype =	!sc->phy_10Mbps_only; /* interface mode */
	cbp->nsai =		1;	/* (don't) disable source addr insert */
	cbp->preamble_length =	2;	/* (7 byte) preamble */
	cbp->loopback =		0;	/* (don't) loopback */
	cbp->linear_priority =	0;	/* (normal CSMA/CD operation) */
	cbp->linear_pri_mode =	0;	/* (wait after xmit only) */
	cbp->interfrm_spacing =	6;	/* (96 bits of) interframe spacing */
	cbp->promiscuous =	prm;	/* promiscuous mode */
	cbp->bcast_disable =	0;	/* (don't) disable broadcasts */
	cbp->crscdt =		0;	/* (CRS only) */
	cbp->stripping =	!prm;	/* truncate rx packet to byte count */
	cbp->padding =		1;	/* (do) pad short tx packets */
	cbp->rcv_crc_xfer =	0;	/* (don't) xfer CRC to host */
	cbp->force_fdx =	0;	/* (don't) force full duplex */
	cbp->fdx_pin_en =	1;	/* (enable) FDX# pin */
	cbp->multi_ia =		0;	/* (don't) accept multiple IAs */
	cbp->mc_all =		mcast;	/* accept all multicasts */

	/*
	 * Start the config command/DMA.
	 */
	fxp_scb_wait(csr);
	csr->scb_general = vtophys(cbp);
	csr->scb_command = FXP_SCB_COMMAND_CU_START;
	/* ...and wait for it to complete. */
	while (!(cbp->cb_status & FXP_CB_STATUS_C));

	/*
	 * Now initialize the station address. Temporarily use the TxCB
	 * memory area like we did above for the config CB.
	 */
	cb_ias = (struct fxp_cb_ias *) sc->cbl_base;
	cb_ias->cb_status = 0;
	cb_ias->cb_command = FXP_CB_COMMAND_IAS | FXP_CB_COMMAND_EL;
	cb_ias->link_addr = -1;
	bcopy(sc->arpcom.ac_enaddr, (void *)cb_ias->macaddr,
	    sizeof(sc->arpcom.ac_enaddr));

	/*
	 * Start the IAS (Individual Address Setup) command/DMA.
	 */
	fxp_scb_wait(csr);
	csr->scb_command = FXP_SCB_COMMAND_CU_START;
	/* ...and wait for it to complete. */
	while (!(cb_ias->cb_status & FXP_CB_STATUS_C));

	/*
	 * Initialize transmit control block (TxCB) list.
	 */

	txp = sc->cbl_base;
	bzero(txp, sizeof(struct fxp_cb_tx) * FXP_NTXCB);
	for (i = 0; i < FXP_NTXCB; i++) {
		txp[i].cb_status = FXP_CB_STATUS_C | FXP_CB_STATUS_OK;
		txp[i].cb_command = FXP_CB_COMMAND_NOP;
		txp[i].link_addr = vtophys(&txp[(i + 1) & FXP_TXCB_MASK]);
		txp[i].tbd_array_addr = vtophys(&txp[i].tbd[0]);
		txp[i].next = &txp[(i + 1) & FXP_TXCB_MASK];
	}
	/*
	 * Set the stop flag on the first TxCB and start the control
	 * unit. It will execute the NOP and then suspend.
	 */
	txp->cb_command = FXP_CB_COMMAND_NOP | FXP_CB_COMMAND_S;
	sc->cbl_first = sc->cbl_last = txp;
	sc->tx_queued = 0;

	fxp_scb_wait(csr);
	csr->scb_command = FXP_SCB_COMMAND_CU_START;

	/*
	 * Initialize receiver buffer area - RFA.
	 */
	fxp_scb_wait(csr);
	csr->scb_general = vtophys(sc->rfa_headm->m_ext.ext_buf);
	csr->scb_command = FXP_SCB_COMMAND_RU_START;

	/*
	 * Toggle a few bits in the PHY.
	 */
	switch (sc->phy_primary_device) {
	case FXP_PHY_DP83840:
	case FXP_PHY_DP83840A:
		fxp_mdi_write(sc->csr, sc->phy_primary_addr, FXP_DP83840_PCR,
		    fxp_mdi_read(sc->csr, sc->phy_primary_addr, FXP_DP83840_PCR) |
		    FXP_DP83840_PCR_LED4_MODE |	/* LED4 always indicates duplex */
		    FXP_DP83840_PCR_F_CONNECT |	/* force link disconnect bypass */
		    FXP_DP83840_PCR_BIT10);	/* XXX I have no idea */
		/* fall through */
	case FXP_PHY_82555:
		/*
		 * If link0 is set, disable auto-negotiation and then:
		 *	If link1 is unset = 10Mbps
		 *	If link1 is set = 100Mbps
		 *	If link2 is unset = half duplex
		 *	If link2 is set = full duplex
		 */
		if (ifp->if_flags & IFF_LINK0) {
			int flags;

			flags = (ifp->if_flags & IFF_LINK1) ?
			     FXP_PHY_BMCR_SPEED_100M : 0;
			flags |= (ifp->if_flags & IFF_LINK2) ?
			     FXP_PHY_BMCR_FULLDUPLEX : 0;
			fxp_mdi_write(sc->csr, sc->phy_primary_addr, FXP_PHY_BMCR,
			    (fxp_mdi_read(sc->csr, sc->phy_primary_addr, FXP_PHY_BMCR) &
			    ~(FXP_PHY_BMCR_AUTOEN | FXP_PHY_BMCR_SPEED_100M |
			     FXP_PHY_BMCR_FULLDUPLEX)) | flags);
		} else {
			fxp_mdi_write(sc->csr, sc->phy_primary_addr, FXP_PHY_BMCR,
			    (fxp_mdi_read(sc->csr, sc->phy_primary_addr, FXP_PHY_BMCR) |
			    FXP_PHY_BMCR_AUTOEN));
		}
		break;
	default:
		printf("fxp%d: warning: unsupported PHY, type = %d, addr = %d\n",
		     ifp->if_unit, sc->phy_primary_device, sc->phy_primary_addr);
	}

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	splx(s);

	/*
	 * Start stats updater.
	 */
	timeout(fxp_stats_update, sc, hz);
}

/*
 * Add a buffer to the end of the RFA buffer list.
 * Return 0 if successful, 1 for failure. A failure results in
 * adding the 'oldm' (if non-NULL) on to the end of the list -
 * tossing out it's old contents and recycling it.
 * The RFA struct is stuck at the beginning of mbuf cluster and the
 * data pointer is fixed up to point just past it.
 */
static int
fxp_add_rfabuf(sc, oldm)
	struct fxp_softc *sc;
	struct mbuf *oldm;
{
	struct mbuf *m;
	struct fxp_rfa *rfa, *p_rfa;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m != NULL) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			if (oldm == NULL)
				return 1;
			m = oldm;
			m->m_data = m->m_ext.ext_buf;
		}
	} else {
		if (oldm == NULL)
			return 1;
		m = oldm;
		m->m_data = m->m_ext.ext_buf;
	}
	/*
	 * Get a pointer to the base of the mbuf cluster and move
	 * data start past it.
	 */
	rfa = mtod(m, struct fxp_rfa *);
	m->m_data += sizeof(struct fxp_rfa);
	rfa->size = MCLBYTES - sizeof(struct fxp_rfa);

	rfa->rfa_status = 0;
	rfa->rfa_control = FXP_RFA_CONTROL_EL;
	rfa->link_addr = -1;
	rfa->rbd_addr = -1;
	rfa->actual_size = 0;
	/*
	 * If there are other buffers already on the list, attach this
	 * one to the end by fixing up the tail to point to this one.
	 */
	if (sc->rfa_headm != NULL) {
		p_rfa = (struct fxp_rfa *) sc->rfa_tailm->m_ext.ext_buf;
		sc->rfa_tailm->m_next = m;
		p_rfa->link_addr = vtophys(rfa);
		p_rfa->rfa_control &= ~FXP_RFA_CONTROL_EL;
	} else {
		sc->rfa_headm = m;
	}
	sc->rfa_tailm = m;

	return (m == oldm);
}

static volatile int
fxp_mdi_read(csr, phy, reg)
	struct fxp_csr *csr;
	int phy;
	int reg;
{
	int count = 10000;
	int value;

	csr->mdi_control = (FXP_MDI_READ << 26) | (reg << 16) | (phy << 21);

	while (((value = csr->mdi_control) & 0x10000000) == 0 && count--)
		DELAY(10);

	if (count <= 0)
		printf("fxp_mdi_read: timed out\n");

	return (value & 0xffff);
}

static void
fxp_mdi_write(csr, phy, reg, value)
	struct fxp_csr *csr;
	int phy;
	int reg;
	int value;
{
	int count = 10000;

	csr->mdi_control = (FXP_MDI_WRITE << 26) | (reg << 16) | (phy << 21)
	    | (value & 0xffff);

	while ((csr->mdi_control & 0x10000000) == 0 && count--)
		DELAY(10);

	if (count <= 0)
		printf("fxp_mdi_write: timed out\n");
}

static int
fxp_ioctl(ifp, command, data)
	struct ifnet *ifp;
	int command;
	caddr_t data;
{
	struct ifaddr *ifa = (struct ifaddr *) data;
	struct fxp_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *) data;
	int s, error = 0;

	s = splimp();

	switch (command) {

	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFMTU:
		error = ether_ioctl(ifp, command, data);
		break;

	case SIOCSIFFLAGS:

		/*
		 * If interface is marked up and not running, then start it.
		 * If it is marked down and running, stop it.
		 * XXX If it's up then re-initialize it. This is so flags
		 * such as IFF_PROMISC are handled.
		 */
		if (ifp->if_flags & IFF_UP) {
			fxp_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				fxp_stop(sc);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/*
		 * Multicast list has changed; set the hardware filter
		 * accordingly.
		 */
		fxp_init(sc);
		error = 0;
		break;

	default:
		error = EINVAL;
	}
	(void) splx(s);
	return (error);
}
