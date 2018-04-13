/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2012, 2013 Bjoern A. Zeeb
 * Copyright (c) 2014 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-11-C-0249)
 * ("MRC2"), as part of the DARPA MRC research programme.
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
 */
/*
 * Altera Triple-Speed Ethernet MegaCore, Function User Guide
 * UG-01008-3.0, Software Version: 12.0, June 2012.
 * Available at the time of writing at:
 * http://www.altera.com/literature/ug/ug_ethernet.pdf
 *
 * We are using an Marvell E1111 (Alaska) PHY on the DE4.  See mii/e1000phy.c.
 */
/*
 * XXX-BZ NOTES:
 * - ifOutBroadcastPkts are only counted if both ether dst and src are all-1s;
 *   seems an IP core bug, they count ether broadcasts as multicast.  Is this
 *   still the case?
 * - figure out why the TX FIFO fill status and intr did not work as expected.
 * - test 100Mbit/s and 10Mbit/s
 * - blacklist the one special factory programmed ethernet address (for now
 *   hardcoded, later from loader?)
 * - resolve all XXX, left as reminders to shake out details later
 * - Jumbo frame support
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_device_polling.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/jail.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/types.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <net/bpf.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/altera/atse/if_atsereg.h>
#include <dev/altera/softdma/a_api.h>

MODULE_DEPEND(atse, ether, 1, 1, 1);
MODULE_DEPEND(atse, miibus, 1, 1, 1);


#define	ATSE_WATCHDOG_TIME	5

#ifdef DEVICE_POLLING
static poll_handler_t atse_poll;
#endif

/* XXX once we'd do parallel attach, we need a global lock for this. */
#define	ATSE_ETHERNET_OPTION_BITS_UNDEF	0
#define	ATSE_ETHERNET_OPTION_BITS_READ	1
static int atse_ethernet_option_bits_flag = ATSE_ETHERNET_OPTION_BITS_UNDEF;
static uint8_t atse_ethernet_option_bits[ALTERA_ETHERNET_OPTION_BITS_LEN];

static int	atse_intr_debug_enable = 0;
SYSCTL_INT(_debug, OID_AUTO, atse_intr_debug_enable, CTLFLAG_RW,
    &atse_intr_debug_enable, 0,
   "Extra debugging output for atse interrupts");

/*
 * Softc and critical resource locking.
 */
#define	ATSE_LOCK(_sc)		mtx_lock(&(_sc)->atse_mtx)
#define	ATSE_UNLOCK(_sc)	mtx_unlock(&(_sc)->atse_mtx)
#define	ATSE_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->atse_mtx, MA_OWNED)

#define	ATSE_TX_PENDING(sc)	(sc->atse_tx_m != NULL ||		\
				    !IFQ_DRV_IS_EMPTY(&ifp->if_snd))

#ifdef DEBUG
#define	DPRINTF(format, ...)	printf(format, __VA_ARGS__)
#else
#define	DPRINTF(format, ...)
#endif

/* a_api.c functions; factor out? */
static inline void
a_onchip_fifo_mem_core_write(struct resource *res, uint32_t off,
    uint32_t val4, const char *desc, const char *f, const int l)
{

	val4 = htole32(val4);
	DPRINTF("[%s:%d] FIFOW %s 0x%08x = 0x%08x\n", f, l, desc, off, val4);
	bus_write_4(res, off, val4);
}

static inline uint32_t
a_onchip_fifo_mem_core_read(struct resource *res, uint32_t off,
    const char *desc, const char *f, const int l)
{
	uint32_t val4;

	val4 = le32toh(bus_read_4(res, off));
	DPRINTF("[%s:%d] FIFOR %s 0x%08x = 0x%08x\n", f, l, desc, off, val4);

	return (val4);
}

/* The FIFO does an endian conversion, so we must not do it as well. */
/* XXX-BZ in fact we should do a htobe32 so le would be fine as well? */
#define	ATSE_TX_DATA_WRITE(sc, val4)					\
	bus_write_4((sc)->atse_tx_mem_res, A_ONCHIP_FIFO_MEM_CORE_DATA, val4)

#define	ATSE_TX_META_WRITE(sc, val4)					\
	a_onchip_fifo_mem_core_write((sc)->atse_tx_mem_res,		\
	    A_ONCHIP_FIFO_MEM_CORE_METADATA,				\
	    (val4), "TXM", __func__, __LINE__)
#define	ATSE_TX_META_READ(sc)						\
	a_onchip_fifo_mem_core_read((sc)->atse_tx_mem_res,		\
	    A_ONCHIP_FIFO_MEM_CORE_METADATA,				\
	    "TXM", __func__, __LINE__)

#define	ATSE_TX_READ_FILL_LEVEL(sc)					\
	a_onchip_fifo_mem_core_read((sc)->atse_txc_mem_res,		\
	    A_ONCHIP_FIFO_MEM_CORE_STATUS_REG_FILL_LEVEL,		\
	    "TX_FILL", __func__, __LINE__)
#define	ATSE_RX_READ_FILL_LEVEL(sc)					\
	a_onchip_fifo_mem_core_read((sc)->atse_rxc_mem_res,		\
	    A_ONCHIP_FIFO_MEM_CORE_STATUS_REG_FILL_LEVEL,		\
	    "RX_FILL", __func__, __LINE__)

/* The FIFO does an endian conversion, so we must not do it as well. */
/* XXX-BZ in fact we should do a htobe32 so le would be fine as well? */
#define	ATSE_RX_DATA_READ(sc)						\
	bus_read_4((sc)->atse_rx_mem_res, A_ONCHIP_FIFO_MEM_CORE_DATA)
#define	ATSE_RX_META_READ(sc)						\
	a_onchip_fifo_mem_core_read((sc)->atse_rx_mem_res,		\
	    A_ONCHIP_FIFO_MEM_CORE_METADATA,				\
	    "RXM", __func__, __LINE__)

#define	ATSE_RX_STATUS_READ(sc)						\
	a_onchip_fifo_mem_core_read((sc)->atse_rxc_mem_res,		\
	    A_ONCHIP_FIFO_MEM_CORE_STATUS_REG_I_STATUS,			\
	    "RX_EVENT", __func__, __LINE__)

#define	ATSE_TX_STATUS_READ(sc)						\
	a_onchip_fifo_mem_core_read((sc)->atse_txc_mem_res,		\
	    A_ONCHIP_FIFO_MEM_CORE_STATUS_REG_I_STATUS,			\
	    "TX_EVENT", __func__, __LINE__)

#define	ATSE_RX_EVENT_READ(sc)						\
	a_onchip_fifo_mem_core_read((sc)->atse_rxc_mem_res,		\
	    A_ONCHIP_FIFO_MEM_CORE_STATUS_REG_EVENT,			\
	    "RX_EVENT", __func__, __LINE__)

#define	ATSE_TX_EVENT_READ(sc)						\
	a_onchip_fifo_mem_core_read((sc)->atse_txc_mem_res,		\
	    A_ONCHIP_FIFO_MEM_CORE_STATUS_REG_EVENT,			\
	    "TX_EVENT", __func__, __LINE__)

#define	ATSE_RX_EVENT_CLEAR(sc)						\
	do {								\
		uint32_t val4;						\
									\
		val4 = a_onchip_fifo_mem_core_read(			\
		    (sc)->atse_rxc_mem_res,				\
		    A_ONCHIP_FIFO_MEM_CORE_STATUS_REG_EVENT,		\
		    "RX_EVENT", __func__, __LINE__);			\
		if (val4 != 0x00)					\
			a_onchip_fifo_mem_core_write(			\
			    (sc)->atse_rxc_mem_res,			\
			    A_ONCHIP_FIFO_MEM_CORE_STATUS_REG_EVENT,	\
			    val4, "RX_EVENT", __func__, __LINE__);	\
	} while(0)
#define	ATSE_TX_EVENT_CLEAR(sc)						\
	do {								\
		uint32_t val4;						\
									\
		val4 = a_onchip_fifo_mem_core_read(			\
		    (sc)->atse_txc_mem_res,				\
		    A_ONCHIP_FIFO_MEM_CORE_STATUS_REG_EVENT,		\
		    "TX_EVENT", __func__, __LINE__);			\
		if (val4 != 0x00)					\
			a_onchip_fifo_mem_core_write(			\
			    (sc)->atse_txc_mem_res,			\
			    A_ONCHIP_FIFO_MEM_CORE_STATUS_REG_EVENT,	\
			    val4, "TX_EVENT", __func__, __LINE__);	\
	} while(0)

#define	ATSE_RX_EVENTS	(A_ONCHIP_FIFO_MEM_CORE_INTR_FULL |	\
			    A_ONCHIP_FIFO_MEM_CORE_INTR_OVERFLOW |	\
			    A_ONCHIP_FIFO_MEM_CORE_INTR_UNDERFLOW)
#define	ATSE_RX_INTR_ENABLE(sc)						\
	a_onchip_fifo_mem_core_write((sc)->atse_rxc_mem_res,		\
	    A_ONCHIP_FIFO_MEM_CORE_STATUS_REG_INT_ENABLE,		\
	    ATSE_RX_EVENTS,						\
	    "RX_INTR", __func__, __LINE__)	/* XXX-BZ review later. */
#define	ATSE_RX_INTR_DISABLE(sc)					\
	a_onchip_fifo_mem_core_write((sc)->atse_rxc_mem_res,		\
	    A_ONCHIP_FIFO_MEM_CORE_STATUS_REG_INT_ENABLE, 0,		\
	    "RX_INTR", __func__, __LINE__)
#define	ATSE_RX_INTR_READ(sc)						\
	a_onchip_fifo_mem_core_read((sc)->atse_rxc_mem_res,		\
	    A_ONCHIP_FIFO_MEM_CORE_STATUS_REG_INT_ENABLE,		\
	    "RX_INTR", __func__, __LINE__)

#define	ATSE_TX_EVENTS	(A_ONCHIP_FIFO_MEM_CORE_INTR_EMPTY |		\
			    A_ONCHIP_FIFO_MEM_CORE_INTR_OVERFLOW |	\
			    A_ONCHIP_FIFO_MEM_CORE_INTR_UNDERFLOW)
#define	ATSE_TX_INTR_ENABLE(sc)						\
	a_onchip_fifo_mem_core_write((sc)->atse_txc_mem_res,		\
	    A_ONCHIP_FIFO_MEM_CORE_STATUS_REG_INT_ENABLE,		\
	    ATSE_TX_EVENTS,						\
	    "TX_INTR", __func__, __LINE__)	/* XXX-BZ review later. */
#define	ATSE_TX_INTR_DISABLE(sc)					\
	a_onchip_fifo_mem_core_write((sc)->atse_txc_mem_res,		\
	    A_ONCHIP_FIFO_MEM_CORE_STATUS_REG_INT_ENABLE, 0,		\
	    "TX_INTR", __func__, __LINE__)
#define	ATSE_TX_INTR_READ(sc)						\
	a_onchip_fifo_mem_core_read((sc)->atse_txc_mem_res,		\
	    A_ONCHIP_FIFO_MEM_CORE_STATUS_REG_INT_ENABLE,		\
	    "TX_INTR", __func__, __LINE__)

static int	atse_rx_locked(struct atse_softc *sc);

/*
 * Register space access macros.
 */
static inline void
csr_write_4(struct atse_softc *sc, uint32_t reg, uint32_t val4,
    const char *f, const int l)
{

	val4 = htole32(val4);
	DPRINTF("[%s:%d] CSR W %s 0x%08x (0x%08x) = 0x%08x\n", f, l,
	    "atse_mem_res", reg, reg * 4, val4);
	bus_write_4(sc->atse_mem_res, reg * 4, val4);
}

static inline uint32_t
csr_read_4(struct atse_softc *sc, uint32_t reg, const char *f, const int l)
{
	uint32_t val4;

	val4 = le32toh(bus_read_4(sc->atse_mem_res, reg * 4));
	DPRINTF("[%s:%d] CSR R %s 0x%08x (0x%08x) = 0x%08x\n", f, l, 
	    "atse_mem_res", reg, reg * 4, val4);

	return (val4);
}

/*
 * See page 5-2 that it's all dword offsets and the MS 16 bits need to be zero
 * on write and ignored on read.
 */
static inline void
pxx_write_2(struct atse_softc *sc, bus_addr_t bmcr, uint32_t reg, uint16_t val,
    const char *f, const int l, const char *s)
{
	uint32_t val4;

	val4 = htole32(val & 0x0000ffff);
	DPRINTF("[%s:%d] %s W %s 0x%08x (0x%08jx) = 0x%08x\n", f, l, s,
	    "atse_mem_res", reg, (bmcr + reg) * 4, val4);
	bus_write_4(sc->atse_mem_res, (bmcr + reg) * 4, val4);
}

static inline uint16_t
pxx_read_2(struct atse_softc *sc, bus_addr_t bmcr, uint32_t reg, const char *f,
    const int l, const char *s)
{
	uint32_t val4;
	uint16_t val;

	val4 = bus_read_4(sc->atse_mem_res, (bmcr + reg) * 4);
	val = le32toh(val4) & 0x0000ffff;
	DPRINTF("[%s:%d] %s R %s 0x%08x (0x%08jx) = 0x%04x\n", f, l, s,
	    "atse_mem_res", reg, (bmcr + reg) * 4, val);

	return (val);
}

#define	CSR_WRITE_4(sc, reg, val)	\
	csr_write_4((sc), (reg), (val), __func__, __LINE__)
#define	CSR_READ_4(sc, reg)		\
	csr_read_4((sc), (reg), __func__, __LINE__)
#define	PCS_WRITE_2(sc, reg, val)	\
	pxx_write_2((sc), sc->atse_bmcr0, (reg), (val), __func__, __LINE__, \
	    "PCS")
#define	PCS_READ_2(sc, reg)		\
	pxx_read_2((sc), sc->atse_bmcr0, (reg), __func__, __LINE__, "PCS")
#define	PHY_WRITE_2(sc, reg, val)	\
	pxx_write_2((sc), sc->atse_bmcr1, (reg), (val), __func__, __LINE__, \
	    "PHY")
#define	PHY_READ_2(sc, reg)		\
	pxx_read_2((sc), sc->atse_bmcr1, (reg), __func__, __LINE__, "PHY")

static void atse_tick(void *);
static int atse_detach(device_t);

devclass_t atse_devclass;

static int
atse_tx_locked(struct atse_softc *sc, int *sent)
{
	struct mbuf *m;
	uint32_t val4, fill_level;
	int leftm;
	int c;

	ATSE_LOCK_ASSERT(sc);

	m = sc->atse_tx_m;
	KASSERT(m != NULL, ("%s: m is null: sc=%p", __func__, sc));
	KASSERT(m->m_flags & M_PKTHDR, ("%s: not a pkthdr: m=%p", __func__, m));

	/*
	 * Copy to buffer to minimize our pain as we can only store
	 * double words which, after the first mbuf gets out of alignment
	 * quite quickly.
	 */
	if (sc->atse_tx_m_offset == 0) {
		m_copydata(m, 0, m->m_pkthdr.len, sc->atse_tx_buf);
		sc->atse_tx_buf_len = m->m_pkthdr.len;
	}

	fill_level = ATSE_TX_READ_FILL_LEVEL(sc);
#if 0	/* Returns 0xdeadc0de. */
	val4 = ATSE_TX_META_READ(sc);
#endif
	if (sc->atse_tx_m_offset == 0) {
		/* Write start of packet. */
		val4 = A_ONCHIP_FIFO_MEM_CORE_SOP;
		val4 &= ~A_ONCHIP_FIFO_MEM_CORE_EOP;
		ATSE_TX_META_WRITE(sc, val4);
	}

	/* TX FIFO is single clock mode, so we have the full FIFO. */
	c = 0;
	while ((sc->atse_tx_buf_len - sc->atse_tx_m_offset) > 4 &&
	     fill_level < AVALON_FIFO_TX_BASIC_OPTS_DEPTH) {

		bcopy(&sc->atse_tx_buf[sc->atse_tx_m_offset], &val4,
		    sizeof(val4));
		ATSE_TX_DATA_WRITE(sc, val4);
		sc->atse_tx_m_offset += sizeof(val4);
		c += sizeof(val4);

		fill_level++;
		if (fill_level == AVALON_FIFO_TX_BASIC_OPTS_DEPTH)
			fill_level = ATSE_TX_READ_FILL_LEVEL(sc);
	}
	if (sent != NULL)
		*sent += c;

	/* Set EOP *before* writing the last symbol. */
	if (sc->atse_tx_m_offset >= (sc->atse_tx_buf_len - 4) &&
	    fill_level < AVALON_FIFO_TX_BASIC_OPTS_DEPTH) {

		/* Set EndOfPacket. */
		val4 = A_ONCHIP_FIFO_MEM_CORE_EOP;

		/* Set EMPTY. */
		leftm = sc->atse_tx_buf_len - sc->atse_tx_m_offset;
		val4 |= ((4 - leftm) << A_ONCHIP_FIFO_MEM_CORE_EMPTY_SHIFT);
		ATSE_TX_META_WRITE(sc, val4);

		/* Write last symbol. */
		val4 = 0;
		bcopy(sc->atse_tx_buf + sc->atse_tx_m_offset, &val4, leftm);
		ATSE_TX_DATA_WRITE(sc, val4);

		if (sent != NULL)
			*sent += leftm;

		/* OK, the packet is gone. */
		sc->atse_tx_m = NULL;
		sc->atse_tx_m_offset = 0;

		/* If anyone is interested give them a copy. */
		BPF_MTAP(sc->atse_ifp, m);

		m_freem(m);
		return (0);
	}

	return (EBUSY);
}

static void
atse_start_locked(struct ifnet *ifp)
{
	struct atse_softc *sc;
	int error, sent;

	sc = ifp->if_softc;
	ATSE_LOCK_ASSERT(sc);

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING || (sc->atse_flags & ATSE_FLAGS_LINK) == 0)
		return;

#if 1
	/*
	 * Disable the watchdog while sending, we are batching packets.
	 * Though we should never reach 5 seconds, and are holding the lock,
	 * but who knows.
	 */
	sc->atse_watchdog_timer = 0;
#endif

	if (sc->atse_tx_m != NULL) {
		error = atse_tx_locked(sc, &sent);
		if (error != 0)
			goto done;
	}
	/* We have more space to send so continue ... */
	for (; !IFQ_DRV_IS_EMPTY(&ifp->if_snd); ) {

		IFQ_DRV_DEQUEUE(&ifp->if_snd, sc->atse_tx_m);
		sc->atse_tx_m_offset = 0;
		if (sc->atse_tx_m == NULL)
			break;
		error = atse_tx_locked(sc, &sent);
		if (error != 0)
			goto done;
	}

done:
	/* If the IP core walks into Nekromanteion try to bail out. */
	if (sent > 0)
		sc->atse_watchdog_timer = ATSE_WATCHDOG_TIME;
}

static void
atse_start(struct ifnet *ifp)
{
	struct atse_softc *sc;

	sc = ifp->if_softc;
	ATSE_LOCK(sc);
	atse_start_locked(ifp);
	ATSE_UNLOCK(sc);
}

static int
atse_stop_locked(struct atse_softc *sc)
{
	uint32_t mask, val4;
	struct ifnet *ifp;
	int i;

	ATSE_LOCK_ASSERT(sc);

	sc->atse_watchdog_timer = 0;
	callout_stop(&sc->atse_tick);

	ifp = sc->atse_ifp;
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	ATSE_RX_INTR_DISABLE(sc);
	ATSE_TX_INTR_DISABLE(sc);
	ATSE_RX_EVENT_CLEAR(sc);
	ATSE_TX_EVENT_CLEAR(sc);

	/* Disable MAC transmit and receive datapath. */
	mask = BASE_CFG_COMMAND_CONFIG_TX_ENA|BASE_CFG_COMMAND_CONFIG_RX_ENA;
	val4 = CSR_READ_4(sc, BASE_CFG_COMMAND_CONFIG);
	val4 &= ~mask;
	CSR_WRITE_4(sc, BASE_CFG_COMMAND_CONFIG, val4);
	/* Wait for bits to be cleared; i=100 is excessive. */
	for (i = 0; i < 100; i++) {
		val4 = CSR_READ_4(sc, BASE_CFG_COMMAND_CONFIG);
		if ((val4 & mask) == 0)
			break;
		DELAY(10);
	}
	if ((val4 & mask) != 0)
		device_printf(sc->atse_dev, "Disabling MAC TX/RX timed out.\n");
		/* Punt. */

	sc->atse_flags &= ~ATSE_FLAGS_LINK;

	/* XXX-BZ free the RX/TX rings. */

	return (0);
}

static uint8_t
atse_mchash(struct atse_softc *sc __unused, const uint8_t *addr)
{
	uint8_t x, y;
	int i, j;

	x = 0;
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		y = addr[i] & 0x01;
		for (j = 1; j < 8; j++)
			y ^= (addr[i] >> j) & 0x01;
		x |= (y << i);
	}

	return (x);
}

static int
atse_rxfilter_locked(struct atse_softc *sc)
{
	struct ifmultiaddr *ifma;
	struct ifnet *ifp;
	uint32_t val4;
	int i;

	/* XXX-BZ can we find out if we have the MHASH synthesized? */
	val4 = CSR_READ_4(sc, BASE_CFG_COMMAND_CONFIG);
	/* For simplicity always hash full 48 bits of addresses. */
	if ((val4 & BASE_CFG_COMMAND_CONFIG_MHASH_SEL) != 0)
		val4 &= ~BASE_CFG_COMMAND_CONFIG_MHASH_SEL;

	ifp = sc->atse_ifp;
	if (ifp->if_flags & IFF_PROMISC)
		val4 |= BASE_CFG_COMMAND_CONFIG_PROMIS_EN;
	else
		val4 &= ~BASE_CFG_COMMAND_CONFIG_PROMIS_EN;

	CSR_WRITE_4(sc, BASE_CFG_COMMAND_CONFIG, val4);

	if (ifp->if_flags & IFF_ALLMULTI) {
		/* Accept all multicast addresses. */
		for (i = 0; i <= MHASH_LEN; i++)
			CSR_WRITE_4(sc, MHASH_START + i, 0x1);
	} else {
		/*
		 * Can hold MHASH_LEN entries.
		 * XXX-BZ bitstring.h would be more general.
		 */
		uint64_t h;

		h = 0;
		/*
		 * Re-build and re-program hash table.  First build the
		 * bit-field "yes" or "no" for each slot per address, then
		 * do all the programming afterwards.
		 */
		if_maddr_rlock(ifp);
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;

			h |= (1 << atse_mchash(sc,
			    LLADDR((struct sockaddr_dl *)ifma->ifma_addr)));
		}
		if_maddr_runlock(ifp);
		for (i = 0; i <= MHASH_LEN; i++)
			CSR_WRITE_4(sc, MHASH_START + i,
			    (h & (1 << i)) ? 0x01 : 0x00);
	}

	return (0);
}

static int
atse_ethernet_option_bits_read_fdt(device_t dev)
{
	struct resource *res;
	device_t fdev;
	int i, rid;

	if (atse_ethernet_option_bits_flag & ATSE_ETHERNET_OPTION_BITS_READ)
		return (0);

	fdev = device_find_child(device_get_parent(dev), "cfi", 0);
	if (fdev == NULL)
		return (ENOENT);

	rid = 0;
	res = bus_alloc_resource_any(fdev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (res == NULL)
		return (ENXIO);

	for (i = 0; i < ALTERA_ETHERNET_OPTION_BITS_LEN; i++)
		atse_ethernet_option_bits[i] = bus_read_1(res,
		    ALTERA_ETHERNET_OPTION_BITS_OFF + i);

	bus_release_resource(fdev, SYS_RES_MEMORY, rid, res);
	atse_ethernet_option_bits_flag |= ATSE_ETHERNET_OPTION_BITS_READ;

	return (0);
}

static int
atse_ethernet_option_bits_read(device_t dev)
{
	int error;

	error = atse_ethernet_option_bits_read_fdt(dev);
	if (error == 0)
		return (0);

	device_printf(dev, "Cannot read Ethernet addresses from flash.\n");

	return (error);
}

static int
atse_get_eth_address(struct atse_softc *sc)
{
	unsigned long hostid;
	uint32_t val4;
	int unit;

	/*
	 * Make sure to only ever do this once.  Otherwise a reset would
	 * possibly change our ethernet address, which is not good at all.
	 */
	if (sc->atse_eth_addr[0] != 0x00 || sc->atse_eth_addr[1] != 0x00 ||
	    sc->atse_eth_addr[2] != 0x00)
		return (0);

	if ((atse_ethernet_option_bits_flag &
	    ATSE_ETHERNET_OPTION_BITS_READ) == 0)
		goto get_random;

	val4 = atse_ethernet_option_bits[0] << 24;
	val4 |= atse_ethernet_option_bits[1] << 16;
	val4 |= atse_ethernet_option_bits[2] << 8;
	val4 |= atse_ethernet_option_bits[3];
	/* They chose "safe". */
	if (val4 != le32toh(0x00005afe)) {
		device_printf(sc->atse_dev, "Magic '5afe' is not safe: 0x%08x. "
		    "Falling back to random numbers for hardware address.\n",
		     val4);
		goto get_random;
	}

	sc->atse_eth_addr[0] = atse_ethernet_option_bits[4];
	sc->atse_eth_addr[1] = atse_ethernet_option_bits[5];
	sc->atse_eth_addr[2] = atse_ethernet_option_bits[6];
	sc->atse_eth_addr[3] = atse_ethernet_option_bits[7];
	sc->atse_eth_addr[4] = atse_ethernet_option_bits[8];
	sc->atse_eth_addr[5] = atse_ethernet_option_bits[9];

	/* Handle factory default ethernet addresss: 00:07:ed:ff:ed:15 */
	if (sc->atse_eth_addr[0] == 0x00 && sc->atse_eth_addr[1] == 0x07 &&
	    sc->atse_eth_addr[2] == 0xed && sc->atse_eth_addr[3] == 0xff &&
	    sc->atse_eth_addr[4] == 0xed && sc->atse_eth_addr[5] == 0x15) {

		device_printf(sc->atse_dev, "Factory programmed Ethernet "
		    "hardware address blacklisted.  Falling back to random "
		    "address to avoid collisions.\n");
		device_printf(sc->atse_dev, "Please re-program your flash.\n");
		goto get_random;
	}

	if (sc->atse_eth_addr[0] == 0x00 && sc->atse_eth_addr[1] == 0x00 &&
	    sc->atse_eth_addr[2] == 0x00 && sc->atse_eth_addr[3] == 0x00 &&
	    sc->atse_eth_addr[4] == 0x00 && sc->atse_eth_addr[5] == 0x00) {
		device_printf(sc->atse_dev, "All zero's Ethernet hardware "
		    "address blacklisted.  Falling back to random address.\n");
		device_printf(sc->atse_dev, "Please re-program your flash.\n");
		goto get_random;
	}

	if (ETHER_IS_MULTICAST(sc->atse_eth_addr)) {
		device_printf(sc->atse_dev, "Multicast Ethernet hardware "
		    "address blacklisted.  Falling back to random address.\n");
		device_printf(sc->atse_dev, "Please re-program your flash.\n");
		goto get_random;
	}

	/*
	 * If we find an Altera prefixed address with a 0x0 ending
	 * adjust by device unit.  If not and this is not the first
	 * Ethernet, go to random.
	 */
	unit = device_get_unit(sc->atse_dev);
	if (unit == 0x00)
		return (0);

	if (unit > 0x0f) {
		device_printf(sc->atse_dev, "We do not support Ethernet "
		    "addresses for more than 16 MACs. Falling back to "
		    "random hadware address.\n");
		goto get_random;
	}
	if ((sc->atse_eth_addr[0] & ~0x2) != 0 ||
	    sc->atse_eth_addr[1] != 0x07 || sc->atse_eth_addr[2] != 0xed ||
	    (sc->atse_eth_addr[5] & 0x0f) != 0x0) {
		device_printf(sc->atse_dev, "Ethernet address not meeting our "
		    "multi-MAC standards. Falling back to random hadware "
		    "address.\n");
		goto get_random;
	}
	sc->atse_eth_addr[5] |= (unit & 0x0f);

	return (0);

get_random:
	/*
	 * Fall back to random code we also use on bridge(4).
	 */
	getcredhostid(curthread->td_ucred, &hostid);
	if (hostid == 0) {
		arc4rand(sc->atse_eth_addr, ETHER_ADDR_LEN, 1);
		sc->atse_eth_addr[0] &= ~1;/* clear multicast bit */
		sc->atse_eth_addr[0] |= 2; /* set the LAA bit */
	} else {
		sc->atse_eth_addr[0] = 0x2;
		sc->atse_eth_addr[1] = (hostid >> 24)	& 0xff;
		sc->atse_eth_addr[2] = (hostid >> 16)	& 0xff;
		sc->atse_eth_addr[3] = (hostid >> 8 )	& 0xff;
		sc->atse_eth_addr[4] = hostid		& 0xff;
		sc->atse_eth_addr[5] = sc->atse_unit	& 0xff;
	}

	return (0);
}

static int
atse_set_eth_address(struct atse_softc *sc, int n)
{
	uint32_t v0, v1;

	v0 = (sc->atse_eth_addr[3] << 24) | (sc->atse_eth_addr[2] << 16) |
	    (sc->atse_eth_addr[1] << 8) | sc->atse_eth_addr[0];
	v1 = (sc->atse_eth_addr[5] << 8) | sc->atse_eth_addr[4];

	if (n & ATSE_ETH_ADDR_DEF) {
		CSR_WRITE_4(sc, BASE_CFG_MAC_0, v0);
		CSR_WRITE_4(sc, BASE_CFG_MAC_1, v1);
	}
	if (n & ATSE_ETH_ADDR_SUPP1) {
		CSR_WRITE_4(sc, SUPPL_ADDR_SMAC_0_0, v0);
		CSR_WRITE_4(sc, SUPPL_ADDR_SMAC_0_1, v1);
	}
	if (n & ATSE_ETH_ADDR_SUPP2) {
		CSR_WRITE_4(sc, SUPPL_ADDR_SMAC_1_0, v0);
		CSR_WRITE_4(sc, SUPPL_ADDR_SMAC_1_1, v1);
	}
	if (n & ATSE_ETH_ADDR_SUPP3) {
		CSR_WRITE_4(sc, SUPPL_ADDR_SMAC_2_0, v0);
		CSR_WRITE_4(sc, SUPPL_ADDR_SMAC_2_1, v1);
	}
	if (n & ATSE_ETH_ADDR_SUPP4) {
		CSR_WRITE_4(sc, SUPPL_ADDR_SMAC_3_0, v0);
		CSR_WRITE_4(sc, SUPPL_ADDR_SMAC_3_1, v1);
	}

	return (0);
}

static int
atse_reset(struct atse_softc *sc)
{
	uint32_t val4, mask;
	uint16_t val;
	int i;

	/* 1. External PHY Initialization using MDIO. */
	/*
	 * We select the right MDIO space in atse_attach() and let MII do
	 * anything else.
	 */

	/* 2. PCS Configuration Register Initialization. */
	/* a. Set auto negotiation link timer to 1.6ms for SGMII. */
	PCS_WRITE_2(sc, PCS_EXT_LINK_TIMER_0, 0x0D40);
	PCS_WRITE_2(sc, PCS_EXT_LINK_TIMER_1, 0x0003);

	/* b. Configure SGMII. */
	val = PCS_EXT_IF_MODE_SGMII_ENA|PCS_EXT_IF_MODE_USE_SGMII_AN;
	PCS_WRITE_2(sc, PCS_EXT_IF_MODE, val);

	/* c. Enable auto negotiation. */
	/* Ignore Bits 6,8,13; should be set,set,unset. */
	val = PCS_READ_2(sc, PCS_CONTROL);
	val &= ~(PCS_CONTROL_ISOLATE|PCS_CONTROL_POWERDOWN);
	val &= ~PCS_CONTROL_LOOPBACK;		/* Make this a -link1 option? */
	val |= PCS_CONTROL_AUTO_NEGOTIATION_ENABLE;
	PCS_WRITE_2(sc, PCS_CONTROL, val);

	/* d. PCS reset. */
	val = PCS_READ_2(sc, PCS_CONTROL);
	val |= PCS_CONTROL_RESET;
	PCS_WRITE_2(sc, PCS_CONTROL, val);

	/* Wait for reset bit to clear; i=100 is excessive. */
	for (i = 0; i < 100; i++) {
		val = PCS_READ_2(sc, PCS_CONTROL);
		if ((val & PCS_CONTROL_RESET) == 0)
			break;
		DELAY(10);
	}

	if ((val & PCS_CONTROL_RESET) != 0) {
		device_printf(sc->atse_dev, "PCS reset timed out.\n");
		return (ENXIO);
	}

	/* 3. MAC Configuration Register Initialization. */
	/* a. Disable MAC transmit and receive datapath. */
	mask = BASE_CFG_COMMAND_CONFIG_TX_ENA|BASE_CFG_COMMAND_CONFIG_RX_ENA;
	val4 = CSR_READ_4(sc, BASE_CFG_COMMAND_CONFIG);
	val4 &= ~mask;
	/* Samples in the manual do have the SW_RESET bit set here, why? */
	CSR_WRITE_4(sc, BASE_CFG_COMMAND_CONFIG, val4);
	/* Wait for bits to be cleared; i=100 is excessive. */
	for (i = 0; i < 100; i++) {
		val4 = CSR_READ_4(sc, BASE_CFG_COMMAND_CONFIG);
		if ((val4 & mask) == 0)
			break;
		DELAY(10);
	}
	if ((val4 & mask) != 0) {
		device_printf(sc->atse_dev, "Disabling MAC TX/RX timed out.\n");
		return (ENXIO);
	}
	/* b. MAC FIFO configuration. */
	CSR_WRITE_4(sc, BASE_CFG_TX_SECTION_EMPTY, FIFO_DEPTH_TX - 16);
	CSR_WRITE_4(sc, BASE_CFG_TX_ALMOST_FULL, 3);
	CSR_WRITE_4(sc, BASE_CFG_TX_ALMOST_EMPTY, 8);
	CSR_WRITE_4(sc, BASE_CFG_RX_SECTION_EMPTY, FIFO_DEPTH_RX - 16);
	CSR_WRITE_4(sc, BASE_CFG_RX_ALMOST_FULL, 8);
	CSR_WRITE_4(sc, BASE_CFG_RX_ALMOST_EMPTY, 8);
#if 0
	CSR_WRITE_4(sc, BASE_CFG_TX_SECTION_FULL, 16);
	CSR_WRITE_4(sc, BASE_CFG_RX_SECTION_FULL, 16);
#else
	/* For store-and-forward mode, set this threshold to 0. */
	CSR_WRITE_4(sc, BASE_CFG_TX_SECTION_FULL, 0);
	CSR_WRITE_4(sc, BASE_CFG_RX_SECTION_FULL, 0);
#endif
	/* c. MAC address configuration. */
	/* Also intialize supplementary addresses to our primary one. */
	/* XXX-BZ FreeBSD really needs to grow and API for using these. */
	atse_get_eth_address(sc);
	atse_set_eth_address(sc, ATSE_ETH_ADDR_ALL);

	/* d. MAC function configuration. */
	CSR_WRITE_4(sc, BASE_CFG_FRM_LENGTH, 1518);	/* Default. */
	CSR_WRITE_4(sc, BASE_CFG_TX_IPG_LENGTH, 12);
	CSR_WRITE_4(sc, BASE_CFG_PAUSE_QUANT, 0xFFFF);

	val4 = CSR_READ_4(sc, BASE_CFG_COMMAND_CONFIG);
	/*
	 * If 1000BASE-X/SGMII PCS is initialized, set the ETH_SPEED (bit 3)
	 * and ENA_10 (bit 25) in command_config register to 0.  If half duplex
	 * is reported in the PHY/PCS status register, set the HD_ENA (bit 10)
	 * to 1 in command_config register.
	 * BZ: We shoot for 1000 instead.
	 */
#if 0
	val4 |= BASE_CFG_COMMAND_CONFIG_ETH_SPEED;
#else
	val4 &= ~BASE_CFG_COMMAND_CONFIG_ETH_SPEED;
#endif
	val4 &= ~BASE_CFG_COMMAND_CONFIG_ENA_10;
#if 0
	/*
	 * We do not want to set this, otherwise, we could not even send
	 * random raw ethernet frames for various other research.  By default
	 * FreeBSD will use the right ether source address.
	 */
	val4 |= BASE_CFG_COMMAND_CONFIG_TX_ADDR_INS;
#endif
	val4 |= BASE_CFG_COMMAND_CONFIG_PAD_EN;
	val4 &= ~BASE_CFG_COMMAND_CONFIG_CRC_FWD;
#if 0
	val4 |= BASE_CFG_COMMAND_CONFIG_CNTL_FRM_ENA;
#endif
#if 1
	val4 |= BASE_CFG_COMMAND_CONFIG_RX_ERR_DISC;
#endif
	val &= ~BASE_CFG_COMMAND_CONFIG_LOOP_ENA;		/* link0? */
	CSR_WRITE_4(sc, BASE_CFG_COMMAND_CONFIG, val4);

	/*
	 * Make sure we do not enable 32bit alignment;  FreeBSD cannot
	 * cope with the additional padding (though we should!?).
	 * Also make sure we get the CRC appended.
	 */
	val4 = CSR_READ_4(sc, TX_CMD_STAT);
	val4 &= ~(TX_CMD_STAT_OMIT_CRC|TX_CMD_STAT_TX_SHIFT16);
	CSR_WRITE_4(sc, TX_CMD_STAT, val4);
	val4 = CSR_READ_4(sc, RX_CMD_STAT);
	val4 &= ~RX_CMD_STAT_RX_SHIFT16;
	CSR_WRITE_4(sc, RX_CMD_STAT, val4);

	/* e. Reset MAC. */
	val4 = CSR_READ_4(sc, BASE_CFG_COMMAND_CONFIG);
	val4 |= BASE_CFG_COMMAND_CONFIG_SW_RESET;
	CSR_WRITE_4(sc, BASE_CFG_COMMAND_CONFIG, val4);
	/* Wait for bits to be cleared; i=100 is excessive. */
	for (i = 0; i < 100; i++) {
		val4 = CSR_READ_4(sc, BASE_CFG_COMMAND_CONFIG);
		if ((val4 & BASE_CFG_COMMAND_CONFIG_SW_RESET) == 0)
			break;
		DELAY(10);
	}
	if ((val4 & BASE_CFG_COMMAND_CONFIG_SW_RESET) != 0) {
		device_printf(sc->atse_dev, "MAC reset timed out.\n");
		return (ENXIO);
	}

	/* f. Enable MAC transmit and receive datapath. */
	mask = BASE_CFG_COMMAND_CONFIG_TX_ENA|BASE_CFG_COMMAND_CONFIG_RX_ENA;
	val4 = CSR_READ_4(sc, BASE_CFG_COMMAND_CONFIG);
	val4 |= mask;
	CSR_WRITE_4(sc, BASE_CFG_COMMAND_CONFIG, val4);
	/* Wait for bits to be cleared; i=100 is excessive. */
	for (i = 0; i < 100; i++) {
		val4 = CSR_READ_4(sc, BASE_CFG_COMMAND_CONFIG);
		if ((val4 & mask) == mask)
			break;
		DELAY(10);
	}
	if ((val4 & mask) != mask) {
		device_printf(sc->atse_dev, "Enabling MAC TX/RX timed out.\n");
		return (ENXIO);
	}

	return (0);
}

static void
atse_init_locked(struct atse_softc *sc)
{
	struct ifnet *ifp;
	struct mii_data *mii;
	uint8_t *eaddr;

	ATSE_LOCK_ASSERT(sc);
	ifp = sc->atse_ifp;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0)
		return;

	/*
	 * Must update the ether address if changed.  Given we do not handle
	 * in atse_ioctl() but it's in the general framework, just always
	 * do it here before atse_reset().
	 */
	eaddr = IF_LLADDR(sc->atse_ifp);
	bcopy(eaddr, &sc->atse_eth_addr, ETHER_ADDR_LEN);

	/* Make things frind to halt, cleanup, ... */
	atse_stop_locked(sc);
	/* ... reset, ... */
	atse_reset(sc);

	/* ... and fire up the engine again. */
	atse_rxfilter_locked(sc);

	/* Memory rings?  DMA engine? */

	sc->atse_rx_buf_len = 0;
	sc->atse_flags &= ATSE_FLAGS_LINK;	/* Preserve. */

#ifdef DEVICE_POLLING
	/* Only enable interrupts if we are not polling. */
	if (ifp->if_capenable & IFCAP_POLLING) {
		ATSE_RX_INTR_DISABLE(sc);
		ATSE_TX_INTR_DISABLE(sc);
		ATSE_RX_EVENT_CLEAR(sc);
		ATSE_TX_EVENT_CLEAR(sc);
	} else
#endif
	{
		ATSE_RX_INTR_ENABLE(sc);
		ATSE_TX_INTR_ENABLE(sc);
	}

	mii = device_get_softc(sc->atse_miibus);

	sc->atse_flags &= ~ATSE_FLAGS_LINK;
	mii_mediachg(mii);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	callout_reset(&sc->atse_tick, hz, atse_tick, sc);
}

static void
atse_init(void *xsc)
{
	struct atse_softc *sc;

	/*
	 * XXXRW: There is some argument that we should immediately do RX
	 * processing after enabling interrupts, or one may not fire if there
	 * are buffered packets.
	 */
	sc = (struct atse_softc *)xsc;
	ATSE_LOCK(sc);
	atse_init_locked(sc);
	ATSE_UNLOCK(sc);
}

static int
atse_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct atse_softc *sc;
	struct ifreq *ifr;
	int error, mask;

	error = 0;
	sc = ifp->if_softc;
	ifr = (struct ifreq *)data;

	switch (command) {
	case SIOCSIFFLAGS:
		ATSE_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0 &&
			    ((ifp->if_flags ^ sc->atse_if_flags) &
			    (IFF_PROMISC | IFF_ALLMULTI)) != 0)
				atse_rxfilter_locked(sc);
			else
				atse_init_locked(sc);
		} else if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			atse_stop_locked(sc);
		sc->atse_if_flags = ifp->if_flags;
		ATSE_UNLOCK(sc);
		break;
	case SIOCSIFCAP:
		ATSE_LOCK(sc);
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
#ifdef DEVICE_POLLING
		if ((mask & IFCAP_POLLING) != 0 &&
		    (IFCAP_POLLING & ifp->if_capabilities) != 0) {
			ifp->if_capenable ^= IFCAP_POLLING;
			if ((IFCAP_POLLING & ifp->if_capenable) != 0) {

				error = ether_poll_register(atse_poll, ifp);
				if (error != 0) {
					ATSE_UNLOCK(sc);
					break;
				}
				/* Disable interrupts. */
				ATSE_RX_INTR_DISABLE(sc);
				ATSE_TX_INTR_DISABLE(sc);
				ATSE_RX_EVENT_CLEAR(sc);
				ATSE_TX_EVENT_CLEAR(sc);

			/*
			 * Do not allow disabling of polling if we do
			 * not have interrupts.
			 */
			} else if (sc->atse_rx_irq_res != NULL ||
			    sc->atse_tx_irq_res != NULL) {
				error = ether_poll_deregister(ifp);
				/* Enable interrupts. */
				ATSE_RX_INTR_ENABLE(sc);
				ATSE_TX_INTR_ENABLE(sc);
			} else {
				ifp->if_capenable ^= IFCAP_POLLING;
				error = EINVAL;
			}
		}
#endif /* DEVICE_POLLING */
		ATSE_UNLOCK(sc);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ATSE_LOCK(sc);
		atse_rxfilter_locked(sc);
		ATSE_UNLOCK(sc);
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
	{
		struct mii_data *mii;
		struct ifreq *ifr;

		mii = device_get_softc(sc->atse_miibus);
		ifr = (struct ifreq *)data;
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	}
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
}

static void
atse_intr_debug(struct atse_softc *sc, const char *intrname)
{
	uint32_t rxs, rxe, rxi, rxf, txs, txe, txi, txf;

	if (!atse_intr_debug_enable)
		return;

	rxs = ATSE_RX_STATUS_READ(sc);
	rxe = ATSE_RX_EVENT_READ(sc);
	rxi = ATSE_RX_INTR_READ(sc);
	rxf = ATSE_RX_READ_FILL_LEVEL(sc);

	txs = ATSE_TX_STATUS_READ(sc);
	txe = ATSE_TX_EVENT_READ(sc);
	txi = ATSE_TX_INTR_READ(sc);
	txf = ATSE_TX_READ_FILL_LEVEL(sc);

	printf(
	    "%s - %s: "
	    "rxs 0x%x rxe 0x%x rxi 0x%x rxf 0x%x "
	    "txs 0x%x txe 0x%x txi 0x%x txf 0x%x\n",
	    __func__, intrname,
	    rxs, rxe, rxi, rxf,
	    txs, txe, txi, txf);
}

static void
atse_watchdog(struct atse_softc *sc)
{

	ATSE_LOCK_ASSERT(sc);

	if (sc->atse_watchdog_timer == 0 || --sc->atse_watchdog_timer > 0)
		return;

	device_printf(sc->atse_dev, "watchdog timeout\n");
	if_inc_counter(sc->atse_ifp, IFCOUNTER_OERRORS, 1);

	atse_intr_debug(sc, "poll");

	sc->atse_ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	atse_init_locked(sc);

	atse_rx_locked(sc);
	if (!IFQ_DRV_IS_EMPTY(&sc->atse_ifp->if_snd))
		atse_start_locked(sc->atse_ifp);
}

static void
atse_tick(void *xsc)
{
	struct atse_softc *sc;
	struct mii_data *mii;
	struct ifnet *ifp;

	sc = (struct atse_softc *)xsc;
	ATSE_LOCK_ASSERT(sc);
	ifp = sc->atse_ifp;

	mii = device_get_softc(sc->atse_miibus);
	mii_tick(mii);
	atse_watchdog(sc);
	if ((sc->atse_flags & ATSE_FLAGS_LINK) == 0)
		atse_miibus_statchg(sc->atse_dev);
	callout_reset(&sc->atse_tick, hz, atse_tick, sc);
}

/*
 * Set media options.
 */
static int
atse_ifmedia_upd(struct ifnet *ifp)
{
	struct atse_softc *sc;
	struct mii_data *mii;
	struct mii_softc *miisc;
	int error;

	sc = ifp->if_softc;

	ATSE_LOCK(sc);
	mii = device_get_softc(sc->atse_miibus);
	LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
		PHY_RESET(miisc);
	error = mii_mediachg(mii);
	ATSE_UNLOCK(sc);

	return (error);
}

static void
atse_update_rx_err(struct atse_softc *sc, uint32_t mask)
{
	int i;

	/* RX error are 6 bits, we only know 4 of them. */
	for (i = 0; i < ATSE_RX_ERR_MAX; i++)
		if ((mask & (1 << i)) != 0)
			sc->atse_rx_err[i]++;
}

static int
atse_rx_locked(struct atse_softc *sc)
{
	uint32_t fill, i, j;
	uint32_t data, meta;
	struct ifnet *ifp;
	struct mbuf *m;
	int rx_npkts;

	ATSE_LOCK_ASSERT(sc);

	ifp = sc->atse_ifp;
	rx_npkts = 0;
	j = 0;
	meta = 0;
	do {
outer:
		if (sc->atse_rx_m == NULL) {
			m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
			if (m == NULL)
				return (rx_npkts);
			m->m_len = m->m_pkthdr.len = MCLBYTES;
			/* Make sure upper layers will be aligned. */
			m_adj(m, ETHER_ALIGN);
			sc->atse_rx_m = m;
		}

		fill = ATSE_RX_READ_FILL_LEVEL(sc);
		for (i = 0; i < fill; i++) {
			/*
			 * XXX-BZ for whatever reason the FIFO requires the
			 * the data read before we can access the meta data.
			 */
			data = ATSE_RX_DATA_READ(sc);
			meta = ATSE_RX_META_READ(sc);
			if (meta & A_ONCHIP_FIFO_MEM_CORE_ERROR_MASK) {
				/* XXX-BZ evaluate error. */
				atse_update_rx_err(sc, ((meta &
				    A_ONCHIP_FIFO_MEM_CORE_ERROR_MASK) >>
				    A_ONCHIP_FIFO_MEM_CORE_ERROR_SHIFT) & 0xff);
				if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
				sc->atse_rx_buf_len = 0;
				/*
				 * Should still read till EOP or next SOP.
				 *
				 * XXX-BZ might also depend on
				 * BASE_CFG_COMMAND_CONFIG_RX_ERR_DISC
				 */
				sc->atse_flags |= ATSE_FLAGS_ERROR;
				return (rx_npkts);
			}
			if ((meta & A_ONCHIP_FIFO_MEM_CORE_CHANNEL_MASK) != 0)
				device_printf(sc->atse_dev, "%s: unexpected "
				    "channel %u\n", __func__, (meta &
				    A_ONCHIP_FIFO_MEM_CORE_CHANNEL_MASK) >>
				    A_ONCHIP_FIFO_MEM_CORE_CHANNEL_SHIFT);

			if (meta & A_ONCHIP_FIFO_MEM_CORE_SOP) {
				/*
				 * There is no need to clear SOP between 1st
				 * and subsequent packet data junks.
				 */
				if (sc->atse_rx_buf_len != 0 &&
				    (sc->atse_flags & ATSE_FLAGS_SOP_SEEN) == 0)
				{
					device_printf(sc->atse_dev, "%s: SOP "
					    "without empty buffer: %u\n",
					    __func__, sc->atse_rx_buf_len);
					/* XXX-BZ any better counter? */
					if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
				}

				if ((sc->atse_flags & ATSE_FLAGS_SOP_SEEN) == 0)
				{
					sc->atse_flags |= ATSE_FLAGS_SOP_SEEN;
					sc->atse_rx_buf_len = 0;
				}
			}
#if 0 /* We had to read the data before we could access meta data. See above. */
			data = ATSE_RX_DATA_READ(sc);
#endif
			/* Make sure to not overflow the mbuf data size. */
			if (sc->atse_rx_buf_len >= sc->atse_rx_m->m_len -
			    sizeof(data)) {
				/*
				 * XXX-BZ Error.  We need more mbufs and are
				 * not setup for this yet.
				 */
				if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
				sc->atse_flags |= ATSE_FLAGS_ERROR;
			}
			if ((sc->atse_flags & ATSE_FLAGS_ERROR) == 0)
				/*
				 * MUST keep this bcopy as m_data after m_adj
				 * for IP header aligment is on half-word
				 * and not word alignment.
				 */
				bcopy(&data, (uint8_t *)(sc->atse_rx_m->m_data +
				    sc->atse_rx_buf_len), sizeof(data));
			if (meta & A_ONCHIP_FIFO_MEM_CORE_EOP) {
				uint8_t empty;

				empty = (meta &
				    A_ONCHIP_FIFO_MEM_CORE_EMPTY_MASK) >>
				    A_ONCHIP_FIFO_MEM_CORE_EMPTY_SHIFT;
				sc->atse_rx_buf_len += (4 - empty);

				if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);
				rx_npkts++;

				m = sc->atse_rx_m;
				m->m_pkthdr.len = m->m_len =
				    sc->atse_rx_buf_len;
				sc->atse_rx_m = NULL;

				sc->atse_rx_buf_len = 0;
				sc->atse_flags &= ~ATSE_FLAGS_SOP_SEEN;
				if (sc->atse_flags & ATSE_FLAGS_ERROR) {
					sc->atse_flags &= ~ATSE_FLAGS_ERROR;
					m_freem(m);
				} else {
					m->m_pkthdr.rcvif = ifp;
					ATSE_UNLOCK(sc);
					(*ifp->if_input)(ifp, m);
					ATSE_LOCK(sc);
				}
#ifdef DEVICE_POLLING
				if (ifp->if_capenable & IFCAP_POLLING) {
					if (sc->atse_rx_cycles <= 0)
						return (rx_npkts);
					sc->atse_rx_cycles--;
				}
#endif
				goto outer;	/* Need a new mbuf. */
			} else {
				sc->atse_rx_buf_len += sizeof(data);
			}
		} /* for */

	/* XXX-BZ could optimize in case of another packet waiting. */
	} while (fill > 0);

	return (rx_npkts);
}


/*
 * Report current media status.
 */
static void
atse_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct atse_softc *sc;
	struct mii_data *mii;

	sc = ifp->if_softc;

	ATSE_LOCK(sc);
	mii = device_get_softc(sc->atse_miibus);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
	ATSE_UNLOCK(sc);
}

static void
atse_rx_intr(void *arg)
{
	struct atse_softc *sc;
	struct ifnet *ifp;
	uint32_t rxe;

	sc = (struct atse_softc *)arg;
	ifp = sc->atse_ifp;

	ATSE_LOCK(sc);
#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING) {
		ATSE_UNLOCK(sc);
		return;
	}
#endif

	atse_intr_debug(sc, "rx");
	rxe = ATSE_RX_EVENT_READ(sc);
	if (rxe & (A_ONCHIP_FIFO_MEM_CORE_EVENT_OVERFLOW|
	    A_ONCHIP_FIFO_MEM_CORE_EVENT_UNDERFLOW)) {
		/* XXX-BZ ERROR HANDLING. */
		atse_update_rx_err(sc, ((rxe &
		    A_ONCHIP_FIFO_MEM_CORE_ERROR_MASK) >>
		    A_ONCHIP_FIFO_MEM_CORE_ERROR_SHIFT) & 0xff);
		if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
	}

	/*
	 * There is considerable subtlety in the race-free handling of rx
	 * interrupts: we must disable interrupts whenever we manipulate the
	 * FIFO to prevent further interrupts from firing before we are done;
	 * we must clear the event after processing to prevent the event from
	 * being immediately reposted due to data remaining; we must clear the
	 * event mask before reenabling interrupts or risk missing a positive
	 * edge; and we must recheck everything after completing in case the
	 * event posted between clearing events and reenabling interrupts.  If
	 * a race is experienced, we must restart the whole mechanism.
	 */
	do {
		ATSE_RX_INTR_DISABLE(sc);
#if 0
		sc->atse_rx_cycles = RX_CYCLES_IN_INTR;
#endif
		atse_rx_locked(sc);
		ATSE_RX_EVENT_CLEAR(sc);

		/* Disable interrupts if interface is down. */
		if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			ATSE_RX_INTR_ENABLE(sc);
	} while (!(ATSE_RX_STATUS_READ(sc) &
	    A_ONCHIP_FIFO_MEM_CORE_STATUS_EMPTY));
	ATSE_UNLOCK(sc);

}

static void
atse_tx_intr(void *arg)
{
	struct atse_softc *sc;
	struct ifnet *ifp;
	uint32_t txe;

	sc = (struct atse_softc *)arg;
	ifp = sc->atse_ifp;

	ATSE_LOCK(sc);
#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING) {
		ATSE_UNLOCK(sc);
		return;
	}
#endif

	/* XXX-BZ build histogram. */
	atse_intr_debug(sc, "tx");
	txe = ATSE_TX_EVENT_READ(sc);
	if (txe & (A_ONCHIP_FIFO_MEM_CORE_EVENT_OVERFLOW|
	    A_ONCHIP_FIFO_MEM_CORE_EVENT_UNDERFLOW)) {
		/* XXX-BZ ERROR HANDLING. */
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
	}

	/*
	 * There is also considerable subtlety in the race-free handling of
	 * tx interrupts: all processing occurs with interrupts disabled to
	 * prevent spurious refiring while transmit is in progress (which
	 * could occur if the FIFO drains while sending -- quite likely); we
	 * must not clear the event mask until after we've sent, also to
	 * prevent spurious refiring; once we've cleared the event mask we can
	 * reenable interrupts, but there is a possible race between clear and
	 * enable, so we must recheck and potentially repeat the whole process
	 * if it is detected.
	 */
	do {
		ATSE_TX_INTR_DISABLE(sc);
		sc->atse_watchdog_timer = 0;
		atse_start_locked(ifp);
		ATSE_TX_EVENT_CLEAR(sc);

		/* Disable interrupts if interface is down. */
		if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			ATSE_TX_INTR_ENABLE(sc);
	} while (ATSE_TX_PENDING(sc) &&
	    !(ATSE_TX_STATUS_READ(sc) & A_ONCHIP_FIFO_MEM_CORE_STATUS_FULL));
	ATSE_UNLOCK(sc);
}

#ifdef DEVICE_POLLING
static int
atse_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct atse_softc *sc;
	int rx_npkts = 0;

	sc = ifp->if_softc;
	ATSE_LOCK(sc);
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		ATSE_UNLOCK(sc);
		return (rx_npkts);
	}

	sc->atse_rx_cycles = count;
	rx_npkts = atse_rx_locked(sc);
	atse_start_locked(ifp);

	if (sc->atse_rx_cycles > 0 || cmd == POLL_AND_CHECK_STATUS) {
		uint32_t rx, tx;

		rx = ATSE_RX_EVENT_READ(sc);
		tx = ATSE_TX_EVENT_READ(sc);

		if (rx & (A_ONCHIP_FIFO_MEM_CORE_EVENT_OVERFLOW|
		    A_ONCHIP_FIFO_MEM_CORE_EVENT_UNDERFLOW)) {
			/* XXX-BZ ERROR HANDLING. */
			atse_update_rx_err(sc, ((rx &
			    A_ONCHIP_FIFO_MEM_CORE_ERROR_MASK) >>
			    A_ONCHIP_FIFO_MEM_CORE_ERROR_SHIFT) & 0xff);
			if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
		}
		if (tx & (A_ONCHIP_FIFO_MEM_CORE_EVENT_OVERFLOW|
		    A_ONCHIP_FIFO_MEM_CORE_EVENT_UNDERFLOW)) {
			/* XXX-BZ ERROR HANDLING. */
			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
		}
		if (ATSE_TX_READ_FILL_LEVEL(sc) == 0)
			sc->atse_watchdog_timer = 0;

#if 0
		if (/* Severe error; if only we could find out. */) {
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
			atse_init_locked(sc);
		}
#endif
	}

	ATSE_UNLOCK(sc);
	return (rx_npkts);
}
#endif /* DEVICE_POLLING */

static struct atse_mac_stats_regs {
	const char *name;
	const char *descr;	/* Mostly copied from Altera datasheet. */
} atse_mac_stats_regs[] = {
	[0x1a] =
	{ "aFramesTransmittedOK",
	    "The number of frames that are successfully transmitted including "
	    "the pause frames." },
	{ "aFramesReceivedOK",
	    "The number of frames that are successfully received including the "
	    "pause frames." },
	{ "aFrameCheckSequenceErrors",
	    "The number of receive frames with CRC error." },
	{ "aAlignmentErrors",
	    "The number of receive frames with alignment error." },
	{ "aOctetsTransmittedOK",
	    "The lower 32 bits of the number of data and padding octets that "
	    "are successfully transmitted." },
	{ "aOctetsReceivedOK",
	    "The lower 32 bits of the number of data and padding octets that "
	    " are successfully received." },
	{ "aTxPAUSEMACCtrlFrames",
	    "The number of pause frames transmitted." },
	{ "aRxPAUSEMACCtrlFrames",
	    "The number received pause frames received." },
	{ "ifInErrors",
	    "The number of errored frames received." },
	{ "ifOutErrors",
	    "The number of transmit frames with either a FIFO overflow error, "
	    "a FIFO underflow error, or a error defined by the user "
	    "application." },
	{ "ifInUcastPkts",
	    "The number of valid unicast frames received." },
	{ "ifInMulticastPkts",
	    "The number of valid multicast frames received. The count does "
	    "not include pause frames." },
	{ "ifInBroadcastPkts",
	    "The number of valid broadcast frames received." },
	{ "ifOutDiscards",
	    "This statistics counter is not in use.  The MAC function does not "
	    "discard frames that are written to the FIFO buffer by the user "
	    "application." },
	{ "ifOutUcastPkts",
	    "The number of valid unicast frames transmitted." },
	{ "ifOutMulticastPkts",
	    "The number of valid multicast frames transmitted, excluding pause "
	    "frames." },
	{ "ifOutBroadcastPkts",
	    "The number of valid broadcast frames transmitted." },
	{ "etherStatsDropEvents",
	    "The number of frames that are dropped due to MAC internal errors "
	    "when FIFO buffer overflow persists." },
	{ "etherStatsOctets",
	    "The lower 32 bits of the total number of octets received. This "
	    "count includes both good and errored frames." },
	{ "etherStatsPkts",
	    "The total number of good and errored frames received." },
	{ "etherStatsUndersizePkts",
	    "The number of frames received with length less than 64 bytes. "
	    "This count does not include errored frames." },
	{ "etherStatsOversizePkts",
	    "The number of frames received that are longer than the value "
	    "configured in the frm_length register. This count does not "
	    "include errored frames." },
	{ "etherStatsPkts64Octets",
	    "The number of 64-byte frames received. This count includes good "
	    "and errored frames." },
	{ "etherStatsPkts65to127Octets",
	    "The number of received good and errored frames between the length "
	    "of 65 and 127 bytes." },
	{ "etherStatsPkts128to255Octets",
	    "The number of received good and errored frames between the length "
	    "of 128 and 255 bytes." },
	{ "etherStatsPkts256to511Octets",
	    "The number of received good and errored frames between the length "
	    "of 256 and 511 bytes." },
	{ "etherStatsPkts512to1023Octets",
	    "The number of received good and errored frames between the length "
	    "of 512 and 1023 bytes." },
	{ "etherStatsPkts1024to1518Octets",
	    "The number of received good and errored frames between the length "
	    "of 1024 and 1518 bytes." },
	{ "etherStatsPkts1519toXOctets",
	    "The number of received good and errored frames between the length "
	    "of 1519 and the maximum frame length configured in the frm_length "
	    "register." },
	{ "etherStatsJabbers",
	    "Too long frames with CRC error." },
	{ "etherStatsFragments",
	    "Too short frames with CRC error." },
	/* 0x39 unused, 0x3a/b non-stats. */
	[0x3c] =
	/* Extended Statistics Counters */
	{ "msb_aOctetsTransmittedOK",
	    "Upper 32 bits of the number of data and padding octets that are "
	    "successfully transmitted." },
	{ "msb_aOctetsReceivedOK",
	    "Upper 32 bits of the number of data and padding octets that are "
	    "successfully received." },
	{ "msb_etherStatsOctets",
	    "Upper 32 bits of the total number of octets received. This count "
	    "includes both good and errored frames." }
};

static int
sysctl_atse_mac_stats_proc(SYSCTL_HANDLER_ARGS)
{
	struct atse_softc *sc;
	int error, offset, s;

	sc = arg1;
	offset = arg2;

	s = CSR_READ_4(sc, offset);
	error = sysctl_handle_int(oidp, &s, 0, req);
	if (error || !req->newptr)
		return (error);

	return (0);
}

static struct atse_rx_err_stats_regs {
	const char *name;
	const char *descr;
} atse_rx_err_stats_regs[] = {

#define	ATSE_RX_ERR_FIFO_THRES_EOP	0 /* FIFO threshold reached, on EOP. */
#define	ATSE_RX_ERR_ELEN		1 /* Frame/payload length not valid. */
#define	ATSE_RX_ERR_CRC32		2 /* CRC-32 error. */
#define	ATSE_RX_ERR_FIFO_THRES_TRUNC	3 /* FIFO thresh., truncated frame. */
#define	ATSE_RX_ERR_4			4 /* ? */
#define	ATSE_RX_ERR_5			5 /* / */

	{ "rx_err_fifo_thres_eop",
	    "FIFO threshold reached, reported on EOP." },
	{ "rx_err_fifo_elen",
	    "Frame or payload length not valid." },
	{ "rx_err_fifo_crc32",
	    "CRC-32 error." },
	{ "rx_err_fifo_thres_trunc",
	    "FIFO threshold reached, truncated frame" },
	{ "rx_err_4",
	    "?" },
	{ "rx_err_5",
	    "?" },
};

static int
sysctl_atse_rx_err_stats_proc(SYSCTL_HANDLER_ARGS)
{
	struct atse_softc *sc;
	int error, offset, s;

	sc = arg1;
	offset = arg2;

	s = sc->atse_rx_err[offset];
	error = sysctl_handle_int(oidp, &s, 0, req);
	if (error || !req->newptr)
		return (error);

	return (0);
}

static void
atse_sysctl_stats_attach(device_t dev)
{
	struct sysctl_ctx_list *sctx;
	struct sysctl_oid *soid;
	struct atse_softc *sc;
	int i;

	sc = device_get_softc(dev);
	sctx = device_get_sysctl_ctx(dev);
	soid = device_get_sysctl_tree(dev);

	/* MAC statistics. */
	for (i = 0; i < nitems(atse_mac_stats_regs); i++) {
		if (atse_mac_stats_regs[i].name == NULL ||
		    atse_mac_stats_regs[i].descr == NULL)
			continue;

		SYSCTL_ADD_PROC(sctx, SYSCTL_CHILDREN(soid), OID_AUTO,
		    atse_mac_stats_regs[i].name, CTLTYPE_UINT|CTLFLAG_RD,
		    sc, i, sysctl_atse_mac_stats_proc, "IU",
		    atse_mac_stats_regs[i].descr);
	}

	/* rx_err[]. */
	for (i = 0; i < ATSE_RX_ERR_MAX; i++) {
		if (atse_rx_err_stats_regs[i].name == NULL ||
		    atse_rx_err_stats_regs[i].descr == NULL)
			continue;

		SYSCTL_ADD_PROC(sctx, SYSCTL_CHILDREN(soid), OID_AUTO,
		    atse_rx_err_stats_regs[i].name, CTLTYPE_UINT|CTLFLAG_RD,
		    sc, i, sysctl_atse_rx_err_stats_proc, "IU",
		    atse_rx_err_stats_regs[i].descr);
	}
}

/*
 * Generic device handling routines.
 */
int
atse_attach(device_t dev)
{
	struct atse_softc *sc;
	struct ifnet *ifp;
	int error;

	sc = device_get_softc(dev);

	atse_ethernet_option_bits_read(dev);

	mtx_init(&sc->atse_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF);

	callout_init_mtx(&sc->atse_tick, &sc->atse_mtx, 0);

	sc->atse_tx_buf = malloc(ETHER_MAX_LEN_JUMBO, M_DEVBUF, M_WAITOK);

	/*
	 * We are only doing single-PHY with this driver currently.  The
	 * defaults would be right so that BASE_CFG_MDIO_ADDR0 points to the
	 * 1st PHY address (0) apart from the fact that BMCR0 is always
	 * the PCS mapping, so we always use BMCR1. See Table 5-1 0xA0-0xBF.
	 */
#if 0	/* Always PCS. */
	sc->atse_bmcr0 = MDIO_0_START;
	CSR_WRITE_4(sc, BASE_CFG_MDIO_ADDR0, 0x00);
#endif
	/* Always use matching PHY for atse[0..]. */
	sc->atse_phy_addr = device_get_unit(dev);
	sc->atse_bmcr1 = MDIO_1_START;
	CSR_WRITE_4(sc, BASE_CFG_MDIO_ADDR1, sc->atse_phy_addr);

	/* Reset the adapter. */
	atse_reset(sc);

	/* Setup interface. */
	ifp = sc->atse_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "if_alloc() failed\n");
		error = ENOSPC;
		goto err;
	}
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = atse_ioctl;
	ifp->if_start = atse_start;
	ifp->if_init = atse_init;
	IFQ_SET_MAXLEN(&ifp->if_snd, ATSE_TX_LIST_CNT - 1);
	ifp->if_snd.ifq_drv_maxlen = ATSE_TX_LIST_CNT - 1;
	IFQ_SET_READY(&ifp->if_snd);

	/* MII setup. */
	error = mii_attach(dev, &sc->atse_miibus, ifp, atse_ifmedia_upd,
	    atse_ifmedia_sts, BMSR_DEFCAPMASK, MII_PHY_ANY, MII_OFFSET_ANY, 0);
	if (error != 0) {
		device_printf(dev, "attaching PHY failed: %d\n", error);
		goto err;
	}

	/* Call media-indepedent attach routine. */
	ether_ifattach(ifp, sc->atse_eth_addr);

	/* Tell the upper layer(s) about vlan mtu support. */
	ifp->if_hdrlen = sizeof(struct ether_vlan_header);
	ifp->if_capabilities |= IFCAP_VLAN_MTU;
	ifp->if_capenable = ifp->if_capabilities;
#ifdef DEVICE_POLLING
	/* We will enable polling by default if no irqs available. See below. */
	ifp->if_capabilities |= IFCAP_POLLING;
#endif

	/* Hook up interrupts. */
	if (sc->atse_rx_irq_res != NULL) {
		error = bus_setup_intr(dev, sc->atse_rx_irq_res, INTR_TYPE_NET |
		    INTR_MPSAFE, NULL, atse_rx_intr, sc, &sc->atse_rx_intrhand);
		if (error != 0) {
			device_printf(dev, "enabling RX IRQ failed\n");
			ether_ifdetach(ifp);
			goto err;
		}
	}

	if (sc->atse_tx_irq_res != NULL) {
		error = bus_setup_intr(dev, sc->atse_tx_irq_res, INTR_TYPE_NET |
		    INTR_MPSAFE, NULL, atse_tx_intr, sc, &sc->atse_tx_intrhand);
		if (error != 0) {
			bus_teardown_intr(dev, sc->atse_rx_irq_res,
			    sc->atse_rx_intrhand);
			device_printf(dev, "enabling TX IRQ failed\n");
			ether_ifdetach(ifp);
			goto err;
		}
	}

	if ((ifp->if_capenable & IFCAP_POLLING) != 0 ||
	   (sc->atse_rx_irq_res == NULL && sc->atse_tx_irq_res == NULL)) {
#ifdef DEVICE_POLLING
		/* If not on and no IRQs force it on. */
		if (sc->atse_rx_irq_res == NULL && sc->atse_tx_irq_res == NULL){
			ifp->if_capenable |= IFCAP_POLLING;
			device_printf(dev, "forcing to polling due to no "
			    "interrupts\n");
		}
		error = ether_poll_register(atse_poll, ifp);
		if (error != 0)
			goto err;
#else
		device_printf(dev, "no DEVICE_POLLING in kernel and no IRQs\n");
		error = ENXIO;
#endif
	} else {
		ATSE_RX_INTR_ENABLE(sc);
		ATSE_TX_INTR_ENABLE(sc);
	}

err:
	if (error != 0)
		atse_detach(dev);

	if (error == 0)
		atse_sysctl_stats_attach(dev);

	return (error);
}

static int
atse_detach(device_t dev)
{
	struct atse_softc *sc;
	struct ifnet *ifp;

	sc = device_get_softc(dev);
	KASSERT(mtx_initialized(&sc->atse_mtx), ("%s: mutex not initialized",
	    device_get_nameunit(dev)));
	ifp = sc->atse_ifp;

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING)
		ether_poll_deregister(ifp);
#endif

	/* Only cleanup if attach succeeded. */
	if (device_is_attached(dev)) {
		ATSE_LOCK(sc);
		atse_stop_locked(sc);
		ATSE_UNLOCK(sc);
		callout_drain(&sc->atse_tick);
		ether_ifdetach(ifp);
	}
	if (sc->atse_miibus != NULL)
		device_delete_child(dev, sc->atse_miibus);

	if (sc->atse_tx_intrhand)
		bus_teardown_intr(dev, sc->atse_tx_irq_res,
		    sc->atse_tx_intrhand);
	if (sc->atse_rx_intrhand)
		bus_teardown_intr(dev, sc->atse_rx_irq_res,
		    sc->atse_rx_intrhand);

	if (ifp != NULL)
		if_free(ifp);

	if (sc->atse_tx_buf != NULL)
		free(sc->atse_tx_buf, M_DEVBUF);

	mtx_destroy(&sc->atse_mtx);

	return (0);
}

/* Shared between nexus and fdt implementation. */
void
atse_detach_resources(device_t dev)
{
	struct atse_softc *sc;

	sc = device_get_softc(dev);

	if (sc->atse_txc_mem_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->atse_txc_mem_rid,
		    sc->atse_txc_mem_res);
		sc->atse_txc_mem_res = NULL;
	}
	if (sc->atse_tx_mem_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->atse_tx_mem_rid,
		    sc->atse_tx_mem_res);
		sc->atse_tx_mem_res = NULL;
	}
	if (sc->atse_tx_irq_res != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->atse_tx_irq_rid,
		    sc->atse_tx_irq_res);
		sc->atse_tx_irq_res = NULL;
	}
	if (sc->atse_rxc_mem_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->atse_rxc_mem_rid,
		    sc->atse_rxc_mem_res);
		sc->atse_rxc_mem_res = NULL;
	}
	if (sc->atse_rx_mem_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->atse_rx_mem_rid,
		    sc->atse_rx_mem_res);
		sc->atse_rx_mem_res = NULL;
	}
	if (sc->atse_rx_irq_res != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->atse_rx_irq_rid,
		    sc->atse_rx_irq_res);
		sc->atse_rx_irq_res = NULL;
	}
	if (sc->atse_mem_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->atse_mem_rid,
		    sc->atse_mem_res);
		sc->atse_mem_res = NULL;
	}
}

int
atse_detach_dev(device_t dev)
{
	int error;

	error = atse_detach(dev);
	if (error) {
		/* We are basically in undefined state now. */
		device_printf(dev, "atse_detach() failed: %d\n", error);
		return (error);
	}

	atse_detach_resources(dev);

	return (0);
}

int
atse_miibus_readreg(device_t dev, int phy, int reg)
{
	struct atse_softc *sc;

	sc = device_get_softc(dev);

	/*
	 * We currently do not support re-mapping of MDIO space on-the-fly
	 * but de-facto hard-code the phy#.
	 */
	if (phy != sc->atse_phy_addr)
		return (0);

	return (PHY_READ_2(sc, reg));
}

int
atse_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct atse_softc *sc;

	sc = device_get_softc(dev);

	/*
	 * We currently do not support re-mapping of MDIO space on-the-fly
	 * but de-facto hard-code the phy#.
	 */
	if (phy != sc->atse_phy_addr)
		return (0);

	PHY_WRITE_2(sc, reg, data);
	return (0);
}

void
atse_miibus_statchg(device_t dev)
{
	struct atse_softc *sc;
	struct mii_data *mii;
	struct ifnet *ifp;
	uint32_t val4;

	sc = device_get_softc(dev);
	ATSE_LOCK_ASSERT(sc);

	mii = device_get_softc(sc->atse_miibus);
	ifp = sc->atse_ifp;
	if (mii == NULL || ifp == NULL ||
	    (ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	val4 = CSR_READ_4(sc, BASE_CFG_COMMAND_CONFIG);

	/* Assume no link. */
	sc->atse_flags &= ~ATSE_FLAGS_LINK;

	if ((mii->mii_media_status & (IFM_ACTIVE | IFM_AVALID)) ==
	    (IFM_ACTIVE | IFM_AVALID)) {

		switch (IFM_SUBTYPE(mii->mii_media_active)) {
		case IFM_10_T:
			val4 |= BASE_CFG_COMMAND_CONFIG_ENA_10;
			val4 &= ~BASE_CFG_COMMAND_CONFIG_ETH_SPEED;
			sc->atse_flags |= ATSE_FLAGS_LINK;
			break;
		case IFM_100_TX:
			val4 &= ~BASE_CFG_COMMAND_CONFIG_ENA_10;
			val4 &= ~BASE_CFG_COMMAND_CONFIG_ETH_SPEED;
			sc->atse_flags |= ATSE_FLAGS_LINK;
			break;
		case IFM_1000_T:
			val4 &= ~BASE_CFG_COMMAND_CONFIG_ENA_10;
			val4 |= BASE_CFG_COMMAND_CONFIG_ETH_SPEED;
			sc->atse_flags |= ATSE_FLAGS_LINK;
			break;
		default:
			break;
		}
	}

	if ((sc->atse_flags & ATSE_FLAGS_LINK) == 0) {
		/* XXX-BZ need to stop the MAC? */
		return;
	}

	if (IFM_OPTIONS(mii->mii_media_active & IFM_FDX) != 0)
		val4 &= ~BASE_CFG_COMMAND_CONFIG_HD_ENA;
	else
		val4 |= BASE_CFG_COMMAND_CONFIG_HD_ENA;
	/* XXX-BZ flow control? */

	/* Make sure the MAC is activated. */
	val4 |= BASE_CFG_COMMAND_CONFIG_TX_ENA;
	val4 |= BASE_CFG_COMMAND_CONFIG_RX_ENA;

	CSR_WRITE_4(sc, BASE_CFG_COMMAND_CONFIG, val4);
}

/* end */
