/*-
 * Copyright (c) 2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
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
 * Author: Hartmut Brandt <harti@freebsd.org>
 *
 * Driver for IDT77252 based cards like ProSum's.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_natm.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <sys/condvar.h>
#include <sys/endian.h>
#include <vm/uma.h>

#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_atm.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_atm.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/mbpool.h>

#include <dev/utopia/utopia.h>
#include <dev/patm/idt77252reg.h>
#include <dev/patm/if_patmvar.h>

static void patm_tst_init(struct patm_softc *sc);
static void patm_scd_init(struct patm_softc *sc);

/*
 * Start the card. This assumes the mutex to be held
 */
void
patm_initialize(struct patm_softc *sc)
{
	uint32_t cfg;
	u_int i;

	patm_debug(sc, ATTACH, "configuring...");

	/* clear SRAM */
	for (i = 0; i < sc->mmap->sram * 1024; i += 4)
		patm_sram_write4(sc, i, 0, 0, 0, 0);
	patm_scd_init(sc);

	/* configuration register. Setting NOIDLE makes the timing wrong! */
	cfg = IDT_CFG_TXFIFO9 | IDT_CFG_RXQ512 | PATM_CFG_VPI |
	    /* IDT_CFG_NOIDLE | */ sc->mmap->rxtab;
	if (!(sc->flags & PATM_UNASS))
		cfg |= IDT_CFG_IDLECLP;
	patm_nor_write(sc, IDT_NOR_CFG, cfg);

	/* clean all the status queues and the Raw handle */
	memset(sc->tsq, 0, sc->sq_size);

	/* initialize RSQ */
	patm_debug(sc, ATTACH, "RSQ %llx", (unsigned long long)sc->rsq_phy);
	patm_nor_write(sc, IDT_NOR_RSQB, sc->rsq_phy);
	patm_nor_write(sc, IDT_NOR_RSQT, sc->rsq_phy);
	patm_nor_write(sc, IDT_NOR_RSQH, 0);
	sc->rsq_last = PATM_RSQ_SIZE - 1;

	/* initialize TSTB */
	patm_nor_write(sc, IDT_NOR_TSTB, sc->mmap->tst1base << 2);
	patm_tst_init(sc);

	/* initialize TSQ */
	for (i = 0; i < IDT_TSQ_SIZE; i++)
		sc->tsq[i].stamp = htole32(IDT_TSQE_EMPTY);
	patm_nor_write(sc, IDT_NOR_TSQB, sc->tsq_phy);
	patm_nor_write(sc, IDT_NOR_TSQH, 0);
	patm_nor_write(sc, IDT_NOR_TSQT, 0);
	sc->tsq_next = sc->tsq;

	/* GP */
#if BYTE_ORDER == BIG_ENDIAN && 0
	patm_nor_write(sc, IDT_NOR_GP, IDT_GP_BIGE);
#else
	patm_nor_write(sc, IDT_NOR_GP, 0);
#endif

	/* VPM */
	patm_nor_write(sc, IDT_NOR_VPM, 0);

	/* RxFIFO */
	patm_nor_write(sc, IDT_NOR_RXFD,
	    IDT_RXFD(sc->mmap->rxfifo_addr, sc->mmap->rxfifo_code));
	patm_nor_write(sc, IDT_NOR_RXFT, 0);
	patm_nor_write(sc, IDT_NOR_RXFH, 0);

	/* RAWHND */
	patm_debug(sc, ATTACH, "RWH %llx",
	    (unsigned long long)sc->rawhnd_phy);
	patm_nor_write(sc, IDT_NOR_RAWHND, sc->rawhnd_phy);

	/* ABRSTD */
	patm_nor_write(sc, IDT_NOR_ABRSTD,
	    IDT_ABRSTD(sc->mmap->abrstd_addr, sc->mmap->abrstd_code));
	for (i = 0; i < sc->mmap->abrstd_size; i++)
		patm_sram_write(sc, sc->mmap->abrstd_addr + i, 0);
	patm_nor_write(sc, IDT_NOR_ABRRQ, 0);
	patm_nor_write(sc, IDT_NOR_VBRRQ, 0);

	/* rate tables */
	if (sc->flags & PATM_25M) {
		for (i = 0; i < patm_rtables_size; i++)
			patm_sram_write(sc, sc->mmap->rtables + i,
			    patm_rtables25[i]);
	} else {
		for (i = 0; i < patm_rtables_size; i++)
			patm_sram_write(sc, sc->mmap->rtables + i,
			    patm_rtables155[i]);
	}
	patm_nor_write(sc, IDT_NOR_RTBL, sc->mmap->rtables << 2);

	/* Maximum deficit */
	patm_nor_write(sc, IDT_NOR_MXDFCT, 32 | IDT_MDFCT_LCI | IDT_MDFCT_LNI);

	/* Free buffer queues */
	patm_nor_write(sc, IDT_NOR_FBQP0, 0);
	patm_nor_write(sc, IDT_NOR_FBQP1, 0);
	patm_nor_write(sc, IDT_NOR_FBQP2, 0);
	patm_nor_write(sc, IDT_NOR_FBQP3, 0);

	patm_nor_write(sc, IDT_NOR_FBQWP0, 0);
	patm_nor_write(sc, IDT_NOR_FBQWP1, 0);
	patm_nor_write(sc, IDT_NOR_FBQWP2, 0);
	patm_nor_write(sc, IDT_NOR_FBQWP3, 0);

	patm_nor_write(sc, IDT_NOR_FBQS0,
	    (SMBUF_THRESHOLD << 28) |
	    (SMBUF_NI_THRESH << 24) |
	    (SMBUF_CI_THRESH << 20) |
	    SMBUF_CELLS);
	patm_nor_write(sc, IDT_NOR_FBQS1,
	    (LMBUF_THRESHOLD << 28) |
	    (LMBUF_NI_THRESH << 24) |
	    (LMBUF_CI_THRESH << 20) |
	    LMBUF_CELLS);
	patm_nor_write(sc, IDT_NOR_FBQS2,
	    (VMBUF_THRESHOLD << 28) | VMBUF_CELLS);
	patm_nor_write(sc, IDT_NOR_FBQS3, 0);

	/* make SCD0 for UBR0 */
	if ((sc->scd0 = patm_scd_alloc(sc)) == NULL) {
		patm_printf(sc, "cannot create UBR0 SCD\n");
		patm_reset(sc);
		return;
	}
	sc->scd0->q.ifq_maxlen = PATM_DLFT_MAXQ;

	patm_scd_setup(sc, sc->scd0);
	patm_tct_setup(sc, sc->scd0, NULL);

	patm_debug(sc, ATTACH, "go...");

	sc->utopia.flags &= ~UTP_FL_POLL_CARRIER;
	sc->ifatm.ifnet.if_flags |= IFF_RUNNING;

	/* enable interrupts, Tx and Rx paths */
	cfg |= IDT_CFG_RXPTH | IDT_CFG_RXIIMM | IDT_CFG_RAWIE | IDT_CFG_RQFIE |
	    IDT_CFG_TIMOIE | IDT_CFG_FBIE | IDT_CFG_TXENB | IDT_CFG_TXINT |
	    IDT_CFG_TXUIE | IDT_CFG_TXSFI | IDT_CFG_PHYIE;
	patm_nor_write(sc, IDT_NOR_CFG, cfg);

	for (i = 0; i < sc->mmap->max_conn; i++)
		if (sc->vccs[i] != NULL)
			patm_load_vc(sc, sc->vccs[i], 1);

	ATMEV_SEND_IFSTATE_CHANGED(&sc->ifatm,
	    sc->utopia.carrier == UTP_CARR_OK);
}

/*
 * External callable start function
 */
void
patm_init(void *p)
{
	struct patm_softc *sc = p;

	mtx_lock(&sc->mtx);
	patm_stop(sc);
	patm_initialize(sc);
	mtx_unlock(&sc->mtx);
}

/*
 * Stop the interface
 */
void
patm_stop(struct patm_softc *sc)
{
	u_int i;
	struct mbuf *m;
	struct patm_txmap *map;
	struct patm_scd *scd;

	sc->ifatm.ifnet.if_flags &= ~IFF_RUNNING;
	sc->utopia.flags |= UTP_FL_POLL_CARRIER;

	patm_reset(sc);

	mtx_lock(&sc->tst_lock);
	i = sc->tst_state;
	sc->tst_state = 0;
	callout_stop(&sc->tst_callout);
	mtx_unlock(&sc->tst_lock);

	if (i != 0) {
		/* this means we are just entering or leaving the timeout.
		 * wait a little bit. Doing this correctly would be more
		 * involved */
		DELAY(1000);
	}

	/*
	 * Give any waiters on closing a VCC a chance. They will stop
	 * to wait if they see that IFF_RUNNING disappeared.
	 */
	cv_broadcast(&sc->vcc_cv);

	/* free large buffers */
	patm_debug(sc, ATTACH, "freeing large buffers...");
	for (i = 0; i < sc->lbuf_max; i++)
		if (sc->lbufs[i].m != NULL)
			patm_lbuf_free(sc, &sc->lbufs[i]);

	/* free small buffers that are on the card */
	patm_debug(sc, ATTACH, "freeing small buffers...");
	mbp_card_free(sc->sbuf_pool);

	/* free aal0 buffers that are on the card */
	patm_debug(sc, ATTACH, "freeing aal0 buffers...");
	mbp_card_free(sc->vbuf_pool);

	/* freeing partial receive chains and reset vcc state */
	for (i = 0; i < sc->mmap->max_conn; i++) {
		if (sc->vccs[i] != NULL) {
			if (sc->vccs[i]->chain != NULL) {
				m_freem(sc->vccs[i]->chain);
				sc->vccs[i]->chain = NULL;
				sc->vccs[i]->last = NULL;
			}

			if (sc->vccs[i]->vflags & (PATM_VCC_RX_CLOSING |
			    PATM_VCC_TX_CLOSING)) {
				uma_zfree(sc->vcc_zone, sc->vccs[i]);
				sc->vccs[i] = NULL;
			} else {
				/* keep */
				sc->vccs[i]->vflags &= ~PATM_VCC_OPEN;
				sc->vccs[i]->cps = 0;
				sc->vccs[i]->scd = NULL;
			}
		}
	}

	/* stop all active SCDs */
	while ((scd = LIST_FIRST(&sc->scd_list)) != NULL) {
		/* free queue packets */
		for (;;) {
			_IF_DEQUEUE(&scd->q, m);
			if (m == NULL)
				break;
			m_freem(m);
		}

		/* free transmitting packets */
		for (i = 0; i < IDT_TSQE_TAG_SPACE; i++) {
			if ((m = scd->on_card[i]) != NULL) {
				scd->on_card[i] = 0;
				map = m->m_pkthdr.header;

				bus_dmamap_unload(sc->tx_tag, map->map);
				SLIST_INSERT_HEAD(&sc->tx_maps_free, map, link);
				m_freem(m);
			}
		}
		patm_scd_free(sc, scd);
	}
	sc->scd0 = NULL;

	sc->flags &= ~PATM_CLR;

	/* reset raw cell queue */
	sc->rawh = NULL;

	ATMEV_SEND_IFSTATE_CHANGED(&sc->ifatm,
	    sc->utopia.carrier == UTP_CARR_OK);
}

/*
 * Stop the card and reset it
 */
void
patm_reset(struct patm_softc *sc)
{

	patm_debug(sc, ATTACH, "resetting...");

	patm_nor_write(sc, IDT_NOR_CFG, IDT_CFG_SWRST);
	DELAY(200);
	patm_nor_write(sc, IDT_NOR_CFG, 0);
	DELAY(200);

	patm_nor_write(sc, IDT_NOR_RSQH, 0);
	patm_nor_write(sc, IDT_NOR_TSQH, 0);

	patm_nor_write(sc, IDT_NOR_GP, IDT_GP_PHY_RST);
	DELAY(50);
	patm_nor_write(sc, IDT_NOR_GP, IDT_GP_EEDO | IDT_GP_EECS);
	DELAY(50);
}

/*
 * Initialize the soft TST to contain only ABR scheduling and
 * write it to SRAM
 */
static void
patm_tst_init(struct patm_softc *sc)
{
	u_int i;
	u_int base, idle;

	base = sc->mmap->tst1base;
	idle = sc->mmap->tst1base + sc->mmap->tst_size;

	/* soft */
	for (i = 0; i < sc->mmap->tst_size - 1; i++)
		sc->tst_soft[i] = IDT_TST_VBR;

	sc->tst_state = 0;
	sc->tst_jump[0] = base + sc->mmap->tst_size - 1;
	sc->tst_jump[1] = idle + sc->mmap->tst_size - 1;
	sc->tst_base[0] = base;
	sc->tst_base[1] = idle;

	/* TST1 */
	for (i = 0; i < sc->mmap->tst_size - 1; i++)
		patm_sram_write(sc, base + i, IDT_TST_VBR);
	patm_sram_write(sc, sc->tst_jump[0], IDT_TST_BR | (base << 2));

	/* TST2 */
	for (i = 0; i < sc->mmap->tst_size - 1; i++)
		patm_sram_write(sc, idle + i, IDT_TST_VBR);
	patm_sram_write(sc, sc->tst_jump[1], IDT_TST_BR | (idle << 2));

	sc->tst_free = sc->mmap->tst_size - 1;
	sc->tst_reserve = sc->tst_free * PATM_TST_RESERVE / 100;
	sc->bwrem = sc->ifatm.mib.pcr;
}

/*
 * Initialize the SCDs. This is done by building a list of all free
 * SCDs in SRAM. The first word of each potential SCD is used as a
 * link to the next free SCD. The list is rooted in softc.
 */
static void
patm_scd_init(struct patm_softc *sc)
{
	u_int s;	/* SRAM address of current SCD */

	sc->scd_free = 0;
	for (s = sc->mmap->scd_base; s + 12 <= sc->mmap->tst1base; s += 12) {
		patm_sram_write(sc, s, sc->scd_free);
		sc->scd_free = s;
	}
}

/*
 * allocate an SCQ
 */
struct patm_scd *
patm_scd_alloc(struct patm_softc *sc)
{
	u_int sram, next;	/* SRAM address of this and next SCD */
	int error;
	void *p;
	struct patm_scd *scd;
	bus_dmamap_t map;
	bus_addr_t phy;

	/* get an SCD from the free list */
	if ((sram = sc->scd_free) == 0)
		return (NULL);
	next = patm_sram_read(sc, sram);

	/* allocate memory for the queue and our host stuff */
	error = bus_dmamem_alloc(sc->scd_tag, &p, BUS_DMA_NOWAIT, &map);
	if (error != 0)
		return (NULL);
	phy = 0x3ff;
	error = bus_dmamap_load(sc->scd_tag, map, p, sizeof(scd->scq),
	    patm_load_callback, &phy, BUS_DMA_NOWAIT);
	if (error != 0) {
		bus_dmamem_free(sc->scd_tag, p, map);
		return (NULL);
	}
	KASSERT((phy & 0x1ff) == 0, ("SCD not aligned %lx", (u_long)phy));

	scd = p;
	bzero(scd, sizeof(*scd));

	scd->sram = sram;
	scd->phy = phy;
	scd->map = map;
	scd->space = IDT_SCQ_SIZE;
	scd->last_tag = IDT_TSQE_TAG_SPACE - 1;
	scd->q.ifq_maxlen = PATM_TX_IFQLEN;

	/* remove the scd from the free list */
	sc->scd_free = next;
	LIST_INSERT_HEAD(&sc->scd_list, scd, link);

	return (scd);
}

/*
 * Free an SCD
 */
void
patm_scd_free(struct patm_softc *sc, struct patm_scd *scd)
{

	LIST_REMOVE(scd, link);

	/* clear SCD and insert link word */
	patm_sram_write4(sc, scd->sram, sc->scd_free, 0, 0, 0);
	patm_sram_write4(sc, scd->sram, 0, 0, 0, 0);
	patm_sram_write4(sc, scd->sram, 0, 0, 0, 0);

	/* put on free list */
	sc->scd_free = scd->sram;

	/* free memory */
	bus_dmamap_unload(sc->scd_tag, scd->map);
	bus_dmamem_free(sc->scd_tag, scd, scd->map);
}

/*
 * DMA loading helper function. This function handles the loading of
 * all one segment DMA maps. The argument is a pointer to a bus_addr_t
 * which must contain the desired alignment of the address as a bitmap.
 */
void
patm_load_callback(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	bus_addr_t *phy = arg;

	if (error)
		return;

	KASSERT(nsegs == 1,
	    ("too many segments for DMA: %d", nsegs));
	KASSERT(segs[0].ds_addr <= 0xffffffffUL,
	    ("phys addr too large %lx", (u_long)segs[0].ds_addr));
	KASSERT((segs[0].ds_addr & *phy) == 0,
	    ("bad alignment %lx:%lx",  (u_long)segs[0].ds_addr, (u_long)*phy));

	*phy = segs[0].ds_addr;
}
